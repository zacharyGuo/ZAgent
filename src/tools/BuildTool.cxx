#include "tools/BuildTool.hxx"

#include <filesystem>
#include <sstream>
#include <algorithm>

#include "tools/LocalExecutor.hxx"

namespace zagent {

std::string BuildTool::Name() const {
    return "build";
}

std::string BuildTool::Description() const {
    return "Build the project. Runs cmake --build build by default, "
           "or a custom build command. Returns build output and success status.";
}

nlohmann::json BuildTool::Parameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {
                {"type", "string"},
                {"description", "Build command to execute (default: cmake --build build)"},
            }},
            {"config", {
                {"type", "string"},
                {"description", "Build configuration: Debug, Release, RelWithDebInfo (default: Debug)"},
                {"default", "Debug"},
            }},
        }},
        {"required", {}},
    };
}

ToolResult BuildTool::Execute(const nlohmann::json& arguments) {
    std::string command;
    if (arguments.contains("command") && arguments["command"].is_string()) {
        command = arguments["command"].get<std::string>();
    } else {
        command = "cmake --build build";
        if (arguments.contains("config") && arguments["config"].is_string()) {
            const std::string config = arguments["config"].get<std::string>();
            command += " --config " + config;
        }
    }

    // Security check
    if (m_workspace != nullptr) {
        if (!m_workspace->IsAllowed(".")) {
            return ToolResult{false, {},
                              "Access denied: " + m_workspace->GetError()};
        }
    }

    LocalExecutor executor;

    const ExecutionResult result = executor.Execute(
        command,
        m_workspace != nullptr ? m_workspace->Root().string() : ".",
        120000);

    ToolResult toolResult;
    toolResult.success = result.exitCode == 0;
    toolResult.output = result.stdoutOutput;
    if (!result.stderrOutput.empty()) {
        toolResult.output += "\n" + result.stderrOutput;
    }
    if (result.exitCode != 0) {
        toolResult.error = "Build failed with exit code " + std::to_string(result.exitCode);
    }
    return toolResult;
}

} // namespace zagent