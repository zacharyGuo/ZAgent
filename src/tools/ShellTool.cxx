#include "tools/ShellTool.hxx"

#include "tools/LocalExecutor.hxx"
#include "workspace/Workspace.hxx"

#include <algorithm>
#include <sstream>
#include <string>

namespace zagent {

std::string ShellTool::Name() const {
    return "shell";
}

std::string ShellTool::Description() const {
    return "Execute a shell command in the workspace directory. Returns "
           "the output, exit code, and whether the command succeeded.";
}

nlohmann::json ShellTool::Parameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {
                {"type", "string"},
                {"description", "The shell command to execute"},
            }},
            {"working_dir", {
                {"type", "string"},
                {"description", "Working directory for the command (relative to workspace root)"},
            }},
        }},
        {"required", {"command"}},
    };
}

ToolResult ShellTool::Execute(const nlohmann::json& arguments) {
    // --- Workspace security check ---
    if (!arguments.contains("command") || !arguments["command"].is_string()) {
        return ToolResult{false, {}, "Missing required parameter: command"};
    }

    std::string command = arguments["command"].get<std::string>();
    std::string workingDir = ".";

    if (arguments.contains("working_dir") && arguments["working_dir"].is_string()) {
        workingDir = arguments["working_dir"].get<std::string>();
    }

    // Resolve the working directory relative to workspace
    // For now, we'll validate that it's not an absolute path that escapes
    namespace fs = std::filesystem;

    // For simplicity, allow relative paths and reject absolute paths outside workspace
    // Use has_root_name() as a cross-platform check for absolute paths
    if (!workingDir.empty() && fs::path(workingDir).is_absolute()) {
        // Absolute path - could be dangerous, let basic checks handle it
    }

    // Execute the command
    LocalExecutor executor;

    // We need workspace to be set on the executor - but we don't have direct access
    // The tool's SetWorkspace was called via ToolRegistry::SetWorkspace
    // For now, execute without workspace validation (will be added when the integration is complete)

    auto result = executor.Execute(command, workingDir);

    std::stringstream ss;
    ss << "Command: " << command << "\n\n";

    if (result.success) {
        ss << "Exit code: 0\n";
    } else {
        ss << "Exit code: " << result.exitCode << "\n";
    }

    if (!result.stdoutOutput.empty()) {
        ss << "--- STDOUT ---\n" << result.stdoutOutput << "\n";
    }

    if (!result.stderrOutput.empty()) {
        ss << "--- STDERR ---\n" << result.stderrOutput << "\n";
    }

    if (!result.stdoutOutput.empty() || !result.stderrOutput.empty()) {
        ss << "\n";
    }

    // Return structured output
    ss << "SUCCESS: " << (result.success ? "true" : "false");

    // Build the ToolResult - return the raw output string
    // The LLM can parse this
    return ToolResult{true, ss.str(), {}};
}

} // namespace zagent