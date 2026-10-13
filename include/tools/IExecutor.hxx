#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <chrono>
#include <utility>

#include "workspace/Workspace.hxx"

namespace zagent {

/// Execution result returned by IExecutor.
///
/// Provides structured information about a shell command execution:
/// whether it succeeded, stdout/stderr captures, exit code, and duration.
struct ExecutionResult {
    // Whether the command executed successfully (exit code == 0).
    bool success;
    // Standard output captured from the command.
    std::string stdoutOutput;
    // Standard error captured from the command.
    std::string stderrOutput;
    // Exit code returned by the command (0 typically means success).
    int exitCode;
    // Time spent executing the command, in milliseconds.
    std::chrono::milliseconds durationMs;

    ExecutionResult()
        : success(false), exitCode(-1), durationMs(std::chrono::milliseconds(0)) {}

    // Convenience: true when the command exited with code 0 and produced no error output.
    [[nodiscard]] bool IsSuccess() const { return success; }

    // Convenience: true when the command exited with a non-zero code.
    [[nodiscard]] bool IsFailure() const { return !success; }

    // Convenience: true when there is no stdout or stderr output.
    [[nodiscard]] bool IsSilent() const { return stdoutOutput.empty() && stderrOutput.empty(); }

    // Convenience: true when stdout contains content.
    [[nodiscard]] bool HasStdout() const { return !stdoutOutput.empty(); }

    // Convenience: true when stderr contains content.
    [[nodiscard]] bool HasStderr() const { return !stderrOutput.empty(); }
};

/// Abstract interface for command execution backends.
///
/// Provides a platform-independent way to execute shell commands with
/// support for working directory, timeout, output capture, and output limiting.
class IExecutor {
public:
    virtual ~IExecutor() = default;

    // Execute a shell command.
    //
    // @param command The shell command to run (platform-specific shell).
    // @param workingDir Working directory for the command (empty = current).
    // @param timeoutMs Maximum execution time in milliseconds (0 = no timeout).
    // @param outputLimit Maximum number of characters to capture in output.
    // @return ExecutionResult containing stdout, stderr, exit code, etc.
    virtual ExecutionResult Execute(
        const std::string& command,
        const std::string& workingDir = "",
        int timeoutMs = 30000,
        std::size_t outputLimit = 1024 * 1024) = 0;

    // Set the workspace (security boundary) for this executor.
    virtual void SetWorkspace(const Workspace* workspace);

    // Get the current workspace.
    const Workspace* GetWorkspace() const;

protected:
    /// Validate a path against the workspace security boundary.
    ///
    /// @return true if the path is within the allowed workspace, false otherwise.
    /// Sets an error message that can be retrieved via GetError().
    virtual bool IsAllowed(const std::filesystem::path& path);

    std::string GetError() const;

    /// Truncate output to the given limit while preserving a truncation marker.
    ///
    /// @param output The output string to potentially truncate.
    /// @param limit Maximum number of characters to keep.
    /// @return The possibly truncated output, with a truncation marker if trimmed.
    static std::string TruncateOutput(std::string output, std::size_t limit);

    const Workspace* m_workspace = nullptr;
    std::string m_error;
};

} // namespace zagent
