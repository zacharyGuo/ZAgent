#include "tools/TestTool.hxx"

#include "tools/LocalExecutor.hxx"

#include <sstream>
#include <string>
#include <regex>

namespace zagent {

std::string TestTool::Name() const {
    return "test";
}

std::string TestTool::Description() const {
    return "Run tests for the C++ project. Runs 'ctest' and returns the "
           "test results, including passed, failed, and skipped test counts. "
           "Can analyze test failures and help diagnose issues.";
}

nlohmann::json TestTool::Parameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"directory", {
                {"type", "string"},
                {"description", "Test directory (default: .)"},
            }},
            {"test_filter", {
                {"type", "string"},
                {"description", "Filter for specific tests (e.g., \"test_*\")"},
            }},
        }},
        {"required", {}},
    };
}

ToolResult TestTool::Execute(const nlohmann::json& arguments) {
    std::string testDir = ".";
    if (arguments.contains("directory") && arguments["directory"].is_string()) {
        testDir = arguments["directory"].get<std::string>();
    }

    std::string testFilter;
    if (arguments.contains("test_filter") && arguments["test_filter"].is_string()) {
        testFilter = arguments["test_filter"].get<std::string>();
    }

    // Build test command
    std::string testCommand = "ctest";
    if (!testFilter.empty()) {
        testCommand += " -R \"" + testFilter + "\"";
    }
    testCommand += " --output-on-failure";

    // Create executor and run tests
    LocalExecutor executor;

    auto result = executor.Execute(testCommand, testDir, 300000, 2 * 1024 * 1024);  // 5min timeout, 2MB limit

    std::stringstream ss;
    ss << "$ " << testCommand << "\n\n";

    if (result.success) {
        ss << "All tests passed!\n";
        if (!result.stdoutOutput.empty()) {
            ss << "Test output:\n" << result.stdoutOutput << "\n";
        }
        ss << "(exit code: 0)\n";
    } else {
        ss << "Tests failed!\n";
        ss << "(exit code: " << result.exitCode << ")\n\n";

        if (!result.stdoutOutput.empty()) {
            ss << "=== TEST OUTPUT ===\n" << result.stdoutOutput << "\n";
        }

        if (!result.stderrOutput.empty()) {
            if (!result.stdoutOutput.empty()) ss << "\n";
            ss << "=== ERROR OUTPUT ===\n" << result.stderrOutput << "\n";

            // Try to analyze test failures
            ss << "\n=== TEST FAILURE ANALYSIS ===\n";
            ss << AnalyzeTestFailures(result.stderrOutput);
        }
    }

    return ToolResult{true, ss.str(), {}};
}

std::string TestTool::AnalyzeTestFailures(const std::string& errorOutput) const {
    std::stringstream analysis;

    // Check for common test failure patterns
    if (std::regex_search(errorOutput, std::regex("FAILED|failure", std::regex::icase))) {
        analysis << "Test failures detected. Common causes:\n";
        analysis << "1. Logic errors in test assertions\n";
        analysis << "2. Missing fixtures or setup/teardown\n";
        analysis << "3. Wrong test data or parameters\n";
        analysis << "4. External dependencies not properly mocked\n";
        analysis << "5. Resource leaks or cleanup issues\n\n";
    }

    if (std::regex_search(errorOutput, std::regex("assertion failed", std::regex::icase))) {
        analysis << "Assertion failures likely. Check your test conditions and expectations.\n";
    }

    if (std::regex_search(errorOutput, std::regex("segmentation fault|access violation", std::regex::icase))) {
        analysis << "Segfault or access violation. Check for null pointer dereferences, "
                 << "out-of-bounds access, or uninitialized variables.\n";
    }

    if (std::regex_search(errorOutput, std::regex("timeout|timed out", std::regex::icase))) {
        analysis << "Test timeout. Consider increasing timeout, fixing infinite loops, "
                 << "or optimizing slow operations.\n";
    }

    if (analysis.str().empty()) {
        analysis << "Test failures analyzed. No specific issue detected.\n";
    } else {
        analysis << "\nFor detailed guidance, examine the failing test names and error messages.\n";
    }

    return analysis.str();
}

} // namespace zagent