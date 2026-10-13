#pragma once

#include <string>

#include "tools/IExecutor.hxx"

namespace zagent {

/// Local executor implementation for running shell commands.
///
/// Uses platform-specific APIs:
/// - Windows: CreateProcess with pipe redirection
/// - POSIX: fork/exec with pipe redirection
class LocalExecutor : public IExecutor {
public:
    LocalExecutor() = default;

    ExecutionResult Execute(
        const std::string& command,
        const std::string& workingDir = "",
        int timeoutMs = 30000,
        std::size_t outputLimit = 1024 * 1024) override;

private:
    ExecutionResult ExecuteInternal(
        const std::string& command,
        const std::string& workingDir,
        int timeoutMs,
        std::size_t outputLimit);
};

} // namespace zagent