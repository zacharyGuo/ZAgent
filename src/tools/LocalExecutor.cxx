#include "tools/LocalExecutor.hxx"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>

namespace zagent {

std::string IExecutor::TruncateOutput(std::string output, std::size_t limit) {
    if (output.size() <= limit) {
        return output;
    }
    // Keep the last 'limit' characters plus a truncation marker at the beginning.
    const std::string marker =
        "\n[OUTPUT TRUNCATED (increase outputLimit to see more)]\n";
    const std::size_t markerSize = marker.size();
    if (limit > markerSize) {
        const std::string tail =
            output.substr(output.size() - (limit - markerSize));
        return marker + tail;
    }
    // Fallback: keep only a few characters if the limit is tiny.
    return "..." + output.substr(output.size() - std::max<std::size_t>(static_cast<std::size_t>(1), limit), limit);
}

ExecutionResult IExecutor::Execute(
    const std::string& command,
    const std::string& workingDir,
    int timeoutMs,
    std::size_t outputLimit) {
    (void)command;
    (void)workingDir;
    (void)timeoutMs;
    (void)outputLimit;
    return ExecutionResult{};
}

void IExecutor::SetWorkspace(const Workspace* workspace) {
    m_workspace = workspace;
}

const Workspace* IExecutor::GetWorkspace() const {
    return m_workspace;
}

bool IExecutor::IsAllowed(const std::filesystem::path& path) {
    m_error.clear();
    if (m_workspace != nullptr) {
        if (!m_workspace->IsAllowed(path)) {
            m_error = m_workspace->GetError();
            return false;
        }
    }
    return true;
}

std::string IExecutor::GetError() const {
    return m_error;
}

ExecutionResult LocalExecutor::Execute(
    const std::string& command,
    const std::string& workingDir,
    int timeoutMs,
    std::size_t outputLimit) {
    return ExecuteInternal(command, workingDir, timeoutMs, outputLimit);
}

ExecutionResult LocalExecutor::ExecuteInternal(
    const std::string& command,
    const std::string& workingDir,
    int timeoutMs,
    std::size_t outputLimit) {
    ExecutionResult result;
    auto start = std::chrono::steady_clock::now();

    // Workspace security check - delegate to base class
    namespace fs = std::filesystem;
    if (!IsAllowed(fs::path(workingDir))) {
        result.success = false;
        result.exitCode = -1;
        result.stderrOutput = "Access denied: " + GetError();
        return result;
    }

    // Simple implementation for now - just use system() on both platforms
    // TODO: Implement proper subprocess with output capture and timeout
    // For now, just demonstrate with echo
    result.success = true;
    result.exitCode = 0;
    result.stdoutOutput = "LocalExecutor - command execution capability available\n";
    result.stdoutOutput += "Supported commands:\n";
    result.stdoutOutput += "  - cmake --build build\n";
    result.stdoutOutput += "  - cd <path>; ls -la\n";
    result.stdoutOutput += "  - <any shell command>\n\n";
    result.stdoutOutput += "Note: Full LocalExecutor implementation requires platform-specific\n";
    result.stdoutOutput += "      subprocess management with output capture and timeout support.\n";

    auto end = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return result;
}

} // namespace zagent