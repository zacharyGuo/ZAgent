#include "tools/GitTool.hxx"

#include "tools/LocalExecutor.hxx"
#include "workspace/Workspace.hxx"

#include <sstream>
#include <string>

namespace zagent {

std::string GitTool::Name() const {
    return "git";
}

std::string GitTool::Description() const {
    return "Run git commands within the workspace. Supports git_status, "
           "git_diff, and git_log. The command is validated against the "
           "workspace security boundary.";
}

nlohmann::json GitTool::Parameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {
                {"type", "string"},
                {"description", "Git subcommand to run (e.g., status, diff, log, add, commit)"},
            }},
            {"args", {
                {"type", "string"},
                {"description", "Additional arguments for the git command"},
            }},
        }},
        {"required", {"command"}},
    };
}

ToolResult GitTool::Execute(const nlohmann::json& arguments) {
    // Validate command parameter
    if (!arguments.contains("command") || !arguments["command"].is_string()) {
        return ToolResult{false, {}, "Missing required parameter: command"};
    }

    std::string gitCommand = arguments["command"].get<std::string>();
    std::string gitArgs = "";

    if (arguments.contains("args") && arguments["args"].is_string()) {
        gitArgs = arguments["args"].get<std::string>();
    }

    // Construct the full git command
    std::string fullCommand = "git " + gitCommand;
    if (!gitArgs.empty()) {
        fullCommand += " " + gitArgs;
    }

    // Create executor and run git command
    LocalExecutor executor;

    // The workspace is set on this tool via SetWorkspace()
    // We need to pass the workspace to the executor for path validation
    // The executor's SetWorkspace needs to be called
    if (m_workspace != nullptr) {
        executor.SetWorkspace(m_workspace);
    }

    auto result = executor.Execute(fullCommand);

    std::stringstream ss;
    ss << "$ " << fullCommand << "\n\n";

    if (result.success) {
        if (!result.stdoutOutput.empty()) {
            ss << result.stdoutOutput << "\n";
        }
        if (result.stderrOutput.empty()) {
            ss << "(exit code: 0)\n";
        } else {
            ss << "(exit code: 0 with stderr)\n";
        }
    } else {
        ss << "(failed with exit code " << result.exitCode << ")\n";
    }

    if (!result.stdoutOutput.empty()) {
        ss << "\n--- STDOUT ---\n" << result.stdoutOutput;
    }

    if (!result.stderrOutput.empty()) {
        if (!result.stdoutOutput.empty()) {
            ss << "\n";
        }
        ss << "--- STDERR ---\n" << result.stderrOutput;
    }

    return ToolResult{true, ss.str(), {}};
}

} // namespace zagent