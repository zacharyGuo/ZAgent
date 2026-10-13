#include <iostream>
#include <string>
#include <thread>
#include <memory>

#include "common/Config.hxx"
#include "common/Types.hxx"
#include "agent/Agent.hxx"
#include "llm/OpenAIClient.hxx"
#include "web/HttpServer.hxx"
#include "web/ChatEndpoint.hxx"
#include "web/StaticFileServer.hxx"
#include "tools/ToolRegistry.hxx"
#include "tools/ListFilesTool.hxx"
#include "tools/ReadFileTool.hxx"
#include "tools/SearchTool.hxx"
#include "tools/ApplyPatchTool.hxx"
#include "tools/ShellTool.hxx"
#include "tools/GitTool.hxx"
#include "tools/BuildTool.hxx"
#include "tools/TestTool.hxx"
#include "planner/Planner.hxx"
#include "code/ICodeIntelligence.hxx"
#include "code/ClangdCodeIntelligence.hxx"
#include "context/IContextManager.hxx"
#include "context/ContextManager.hxx"
#include "security/PermissionManager.hxx"
#include "agent/EventBus.hxx"
#include "agent/ISessionPersistence.hxx"
#include "agent/AgentRole.hxx"
#include "agent/MultiAgentManager.hxx"

namespace {

void PrintBanner() {
    std::cout << "\n";
    std::cout << "╭──────────────────────────────────────╮\n";
    std::cout << "│          ZAgent v" << zagent::Version{0, 1, 0}.ToString() << "          │\n";
    std::cout << "│                                      │\n";
    std::cout << "│    Engineering AI Coding Agent       │\n";
    std::cout << "╰──────────────────────────────────────╯\n";
    std::cout << "\n";
}

void PrintHelp() {
    std::cout << "Usage: zagent [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --workspace <path>   Set workspace directory\n";
    std::cout << "  --base-url <url>     LLM API base URL\n";
    std::cout << "  --api-key <key>      API key for authentication\n";
    std::cout << "  --model <model>      Model name to use\n";
    std::cout << "  --web                Enable web interface (Phase 2)\n";
    std::cout << "  --port <port>        Web interface port (default: 8080)\n";
    std::cout << "  --help               Show help message\n\n";
    std::cout << "Environment variables:\n";
    std::cout << "  AI_WORKSPACE, AI_BASE_URL, AI_API_KEY, AI_MODEL\n";
}

std::string BuildSystemPrompt(const zagent::Config& config) {
    std::string prompt = zagent::Agent::kDefaultSystemPrompt;
    if (!config.workspace.empty()) {
        prompt += "\n\nWorkspace: " + config.workspace + ".";
    }
    return prompt;
}

void RegisterTools(zagent::ToolRegistry& toolRegistry) {
    toolRegistry.Register(std::make_unique<zagent::ListFilesTool>());
    toolRegistry.Register(std::make_unique<zagent::ReadFileTool>());
    toolRegistry.Register(std::make_unique<zagent::SearchTool>());
    toolRegistry.Register(std::make_unique<zagent::ApplyPatchTool>());
    toolRegistry.Register(std::make_unique<zagent::ShellTool>());
    toolRegistry.Register(std::make_unique<zagent::GitTool>());
    toolRegistry.Register(std::make_unique<zagent::BuildTool>());
    toolRegistry.Register(std::make_unique<zagent::TestTool>());
}

} // namespace

int main(int argc, char* argv[]) {
    zagent::CommandLineOptions options;
    try {
        options = zagent::ParseCommandLine(argc, argv);
    } catch (const std::exception& exception) {
        std::cerr << "zagent: error: " << exception.what() << "\n";
        return 2;
    }

    if (options.helpRequested) {
        PrintBanner();
        PrintHelp();
        return 0;
    }

    const zagent::Config& config = options.config;

    PrintBanner();
    std::cout << "Workspace: " << (config.workspace.empty()
                                        ? "(not set)"
                                        : config.workspace)
              << "\n";
    std::cout << "Base URL:  " << config.baseUrl << "\n";
    std::cout << "Model:     " << config.model << "\n";
    std::cout << "Platform:  " << zagent::Platform::Name() << " "
              << zagent::Platform::Arch() << "\n";

    if (config.webMode) {
        std::cout << "\nWeb interface enabled on port " << config.port << "\n";
        std::cout << "Open http://localhost:" << config.port << " in your browser.\n";
        std::cout << "Press Ctrl+C to stop the server.\n\n";

        try {
            zagent::HttpServer server("0.0.0.0", config.port);

            // Create LLM client (may be empty if no API key)
            std::unique_ptr<zagent::ILLMClient> llmClient;
            if (!config.apiKey.empty() || !config.baseUrl.empty()) {
                llmClient = std::make_unique<zagent::OpenAIClient>(
                    config.baseUrl, config.apiKey, config.model);
            }

            zagent::ToolRegistry toolRegistry;
            RegisterTools(toolRegistry);

            zagent::ChatEndpoint chatEndpoint(std::move(llmClient),
                                              BuildSystemPrompt(config),
                                              &toolRegistry);
            chatEndpoint.Register(server);

            // Register static files (web UI)
            zagent::StaticFileServer staticFileServer;
            staticFileServer.Register(server, "web/static", "/");

            server.Start();

            // Keep the main thread alive — server runs on a detached thread.
            // Press Ctrl+C to stop the server.
            std::cout << "\n( web server running on http://localhost:" << config.port << " )\n";
            std::cout << "Press Ctrl+C to stop the server.\n\n";
            while (server.IsRunning()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } catch (const std::exception& exception) {
            std::cerr << "zagent: web server error: " << exception.what() << "\n";
            return 1;
        }

        return 0;
    }

    // Initialize code intelligence
    std::unique_ptr<zagent::ICodeIntelligence> codeIntelligence;
    if (!config.workspace.empty()) {
        codeIntelligence = std::make_unique<zagent::ClangdCodeIntelligence>();
        codeIntelligence->SetWorkspace(config.workspace);
        if (codeIntelligence->Initialize()) {
            std::cout << "Code Intelligence: initialized (clangd LSP)" << "\n";
        } else {
            std::cerr << "Code Intelligence: failed to initialize (clangd may not be installed)" << "\n";
        }
    }

    if (config.workspace.empty()) {
        std::cout << "\nWarning: --workspace not set.\n";
        std::cout << "Use --workspace <path> to set the workspace "
                     "directory.\n";
    }

    if (config.apiKey.empty()) {
        std::cout << "\nWarning: no API key provided. Local "
                     "OpenAI-compatible servers may accept empty "
                     "keys.\n";
    }

    std::cout << "\n";

    try {
        zagent::ToolRegistry toolRegistry;
        RegisterTools(toolRegistry);

        // Initialize permission manager
        std::unique_ptr<zagent::IPermissionManager> permissionManager;
        if (!config.workspace.empty()) {
            permissionManager = std::make_unique<zagent::PermissionManager>();
            permissionManager->SetEnabled(true);
            std::cout << "Permission Manager: initialized" << "\n";
        }

        // Initialize context manager
        std::unique_ptr<zagent::IContextManager> contextManager;
        if (!config.workspace.empty()) {
            contextManager = std::make_unique<zagent::ContextManager>(toolRegistry);
            contextManager->Initialize(config.workspace);

            // Set up code intelligence for context manager
            if (codeIntelligence) {
                contextManager->SetCodeIntelligence(std::move(codeIntelligence));
            } else {
                auto ci = std::make_unique<zagent::ClangdCodeIntelligence>();
                ci->SetWorkspace(config.workspace);
                if (ci->Initialize()) {
                    contextManager->SetCodeIntelligence(std::move(ci));
                }
            }

            std::cout << "Context Manager: initialized" << "\n";
        }

        std::unique_ptr<zagent::ILLMClient> llmClient =
            std::make_unique<zagent::OpenAIClient>(config.baseUrl, config.apiKey, config.model);

        zagent::Agent agent(
            std::move(llmClient),
            toolRegistry,
            BuildSystemPrompt(config));

        // Initialize Multi-Agent Runtime
        zagent::MultiAgentRuntime& multiAgentRuntime = zagent::MultiAgentRuntime::Instance();
        if (multiAgentRuntime.Initialize()) {
            std::cout << "Multi-Agent Runtime: initialized" << "\n";
            std::cout << "Available agent roles: Coordinator, Planner, Coder, Reviewer, Tester, Debugger, Architect, Optimizer, Documenter, SecurityAuditor" << "\n";
        } else {
            std::cout << "Multi-Agent Runtime: initialization failed" << "\n";
        }

        std::cout << "Type a message and press Enter to start.\n";
        std::cout << "Commands: /exit, /reset, /status, /help\n";
        std::cout << "Available tools: list_files, read_file, search, apply_patch\n";
        std::cout << "Multi-Agent commands: /agents, /role <role>, /task <task>" << "\n";

        std::string input;
        while (true) {
            std::cout << "\n> ";
            if (!std::getline(std::cin, input)) {
                break;
            }

            if (input == "/exit" || input == "/quit") {
                break;
            }
            if (input == "/help") {
                PrintHelp();
                continue;
            }
            if (input == "/reset") {
                agent.Reset();
                std::cout << "Conversation reset.\n";
                continue;
            }
            if (input == "/status") {
                std::cout << "State: ";
                switch (agent.State()) {
                case zagent::AgentState::Idle:
                    std::cout << "Idle";
                    break;
                case zagent::AgentState::Running:
                    std::cout << "Running";
                    break;
                case zagent::AgentState::Completed:
                    std::cout << "Completed";
                    break;
                case zagent::AgentState::Failed:
                    std::cout << "Failed";
                    break;
                }
                std::cout << "\nIterations: " << agent.IterationCount()
                          << "\n";
                continue;
            }
            if (input == "/agents") {
                auto agents = multiAgentRuntime.GetAgents();
                std::cout << "Registered Agents (" << agents.size() << "):\n";
                for (const auto& a : agents) {
                    std::cout << "  - " << a->Name() << " (" << zagent::AgentRoleToString(a->Role()) << "): " << a->Description() << "\n";
                }
                continue;
            }
            if (input.rfind("/role ", 0) == 0) {
                std::string roleName = input.substr(6);
                // Show agents by role
                zagent::AgentRole role = zagent::AgentRole::Custom;
                if (roleName == "coordinator") role = zagent::AgentRole::Coordinator;
                else if (roleName == "planner") role = zagent::AgentRole::Planner;
                else if (roleName == "coder") role = zagent::AgentRole::Coder;
                else if (roleName == "reviewer") role = zagent::AgentRole::Reviewer;
                else if (roleName == "tester") role = zagent::AgentRole::Tester;
                else if (roleName == "debugger") role = zagent::AgentRole::Debugger;
                else if (roleName == "architect") role = zagent::AgentRole::Architect;
                else if (roleName == "optimizer") role = zagent::AgentRole::Optimizer;
                else if (roleName == "documenter") role = zagent::AgentRole::Documenter;
                else if (roleName == "security") role = zagent::AgentRole::SecurityAuditor;

                if (role != zagent::AgentRole::Custom) {
                    auto agents = multiAgentRuntime.GetAgentsByRole(role);
                    std::cout << "Agents with role " << roleName << " (" << agents.size() << "):\n";
                    for (const auto& a : agents) {
                        std::cout << "  - " << a->Name() << ": " << a->Description() << "\n";
                    }
                } else {
                    std::cout << "Unknown role. Valid roles: coordinator, planner, coder, reviewer, tester, debugger, architect, optimizer, documenter, security\n";
                }
                continue;
            }
            if (input.rfind("/task ", 0) == 0) {
                std::string task = input.substr(6);
                std::cout << "Executing multi-agent task: " << task << "\n";
                std::string result = multiAgentRuntime.CoordinateTask(task, "Workspace: " + config.workspace);
                std::cout << "\n" << result << "\n";
                continue;
            }

            try {
                const std::string reply = agent.Run(input);
                std::cout << "\n" << reply << "\n";
            } catch (const std::exception& exception) {
                std::cout << "\nError: " << exception.what() << "\n";
            }
        }
    } catch (const std::exception& exception) {
        std::cerr << "zagent: fatal error: " << exception.what() << "\n";
        return 1;
    }

    return 0;
}