#include "planner/Planner.hxx"

#pragma execution_character_set("utf-8")

#include <algorithm>
#include <chrono>
#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace zagent {

Planner::Planner(ToolRegistry& tools)
    : m_tools(tools), m_workspace(nullptr) {
}

std::vector<std::string> Planner::GetAvailableTools() const {
    std::vector<std::string> tools;
    // Get tool names from the registry
    // For now, return the known tool set
    tools = {
        "list_files",
        "read_file",
        "search",
        "apply_patch",
        "shell",
        "git",
        "build",
        "test",
    };
    return tools;
}

void Planner::SetWorkspace(const Workspace* workspace) {
    m_workspace = workspace;
}

const Workspace* Planner::GetWorkspace() const {
    return m_workspace;
}

std::vector<Task> Planner::AnalyzeGoal(const std::string& goal, const std::string& context) {
    std::vector<Task> tasks;

    // Normalize the goal for analysis
    std::string normalized = goal;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Remove surrounding quotes if any
    if (normalized.size() >= 2) {
        if ((normalized.front() == '"' && normalized.back() == '"') ||
            (normalized.front() == '\'' && normalized.back() == '\'')) {
            normalized = normalized.substr(1, normalized.size() - 2);
        }
    }

    // Task 1: Always start with understanding the project structure
    Task task1;
    task1.id = "analyze";
    task1.description = "Analyze project structure";
    task1.dependencies = {};
    task1.toolName = "list_files";
    task1.toolArguments = {{"path", "."}};
    task1.state = Task::State::Pending;
    tasks.push_back(task1);

    // Determine what type of task this is and add appropriate tasks
    bool needsBuild = normalized.find("build") != std::string::npos ||
                      normalized.find("compile") != std::string::npos ||
                      normalized.find("cmake") != std::string::npos;

    bool needsTest = normalized.find("test") != std::string::npos;

    bool needsModify = normalized.find("修改") != std::string::npos ||
                       normalized.find("change") != std::string::npos ||
                       normalized.find("add") != std::string::npos ||
                       normalized.find("增加") != std::string::npos ||
                       normalized.find("remove") != std::string::npos ||
                       normalized.find("删除") != std::string::npos ||
                       normalized.find("delete") != std::string::npos;

    bool needsSearch = normalized.find("搜索") != std::string::npos ||
                       normalized.find("find") != std::string::npos ||
                       normalized.find("查找") != std::string::npos ||
                       needsModify || needsTest;

    bool needsGit = normalized.find("git") != std::string::npos ||
                    normalized.find("版本") != std::string::npos ||
                    normalized.find("commit") != std::string::npos ||
                    normalized.find("status") != std::string::npos;

    // Task 2: Search if needed
    if (needsSearch) {
        Task task2;
        task2.id = "search";
        task2.description = "Search for relevant code patterns";
        task2.dependencies = {"analyze"};
        task2.toolName = "search";
        // Extract search pattern from the goal
        std::string searchPattern = ExtractSearchPattern(goal);
        task2.toolArguments = {{"pattern", searchPattern.empty() ? goal : searchPattern}};
        task2.state = Task::State::Pending;
        tasks.push_back(task2);
    }

    // Task 3: Modify if needed (after search)
    if (needsModify) {
        Task task3;
        task3.id = "modify";
        task3.description = "Modify code based on goal";
        task3.dependencies = {"search"};
        task3.toolName = "apply_patch";
        task3.state = Task::State::Pending;
        tasks.push_back(task3);
    }

    // Task 4: Build if needed
    if (needsBuild) {
        Task task4;
        task4.id = "build";
        task4.description = "Build the project";
        task4.dependencies = {};
        task4.toolName = "shell";
        task4.toolArguments = {{"command", "cmake --build build --verbose"}};
        task4.state = Task::State::Pending;
        tasks.push_back(task4);
    }

    // Task 5: Test if needed
    if (needsTest) {
        Task task5;
        task5.id = "test";
        task5.description = "Run tests";
        task5.dependencies = {};
        task5.toolName = "test";
        task5.toolArguments = {{"directory", "."}};
        task5.state = Task::State::Pending;
        tasks.push_back(task5);
    }

    // Task 6: Git operations if needed
    if (needsGit) {
        Task task6;
        task6.id = "git_ops";
        task6.description = "Git operations";
        task6.dependencies = {};
        task6.toolName = "git";
        // Determine git subcommand
        std::string gitCmd = ExtractGitCommand(goal);
        if (!gitCmd.empty()) {
            task6.toolArguments = {{"command", gitCmd}};
        } else {
            task6.toolArguments = {{"command", "status"}};
        }
        task6.state = Task::State::Pending;
        tasks.push_back(task6);
    }

    return tasks;
}

std::string Planner::ExtractSearchPattern(const std::string& goal) {
    // Try to extract a specific search pattern from the goal
    // Common patterns: "给 Foo 增加", "修改 Bar", "find X", "search for Y"
    std::string result;

    // Look for specific entity names (capitalized words, function names, etc.)
    // Simple heuristic: look for words that might be identifiers
    std::istringstream iss(goal);
    std::string word;
    std::vector<std::string> potentialIds;

    while (iss >> word) {
        // Keep words that look like identifiers (not stop words)
        const std::vector<std::string> stopWords = {
            "的", "了", "在", "是", "我", "有", "和", "就", "不", "人", "都", "一个", "上",
        };

        bool isStopWord = false;
        for (const auto& sw : stopWords) {
            if (word == sw) {
                isStopWord = true;
                break;
            }
        }
        if (!isStopWord && word.size() > 1) {
            potentialIds.push_back(word);
        }
    }

    // Return the first potentially meaningful identifier, or the whole goal
    if (!potentialIds.empty()) {
        result = potentialIds[0];
    } else {
        result = goal;
    }

    return result;
}

std::string Planner::ExtractGitCommand(const std::string& goal) {
    std::string lower = goal;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Extract git subcommand
    if (lower.find("git status") != std::string::npos ||
        lower.find("git diff") != std::string::npos) {
        return lower;
    }

    // Look for common git patterns
    if (lower.find("commit") != std::string::npos) {
        return "git commit -m \"修改描述\"";
    }
    if (lower.find("log") != std::string::npos) {
        return "git log --oneline -5";
    }
    if (lower.find("branch") != std::string::npos) {
        return "git branch -a";
    }
    if (lower.find("push") != std::string::npos) {
        return "git push";
    }
    if (lower.find("pull") != std::string::npos) {
        return "git pull";
    }

    return "";
}

std::vector<Task> Planner::Plan(const std::string& goal, const std::string& context) {
    // Analyze the goal and create task list
    std::vector<Task> tasks = AnalyzeGoal(goal, context);

    // Validate paths against workspace if available
    for (auto& task : tasks) {
        if (!task.toolArguments.empty() && task.toolArguments.contains("path")) {
            if (task.toolArguments["path"].is_string()) {
                std::string path = task.toolArguments["path"].get<std::string>();
                if (!path.empty() && m_workspace != nullptr) {
                    // TODO: Validate path through workspace::IsAllowed()
                    // For now, just log and continue
                }
            }
        }
    }

    return tasks;
}

std::vector<ToolResult> Planner::ExecutePlan(const std::vector<Task>& tasks) {
    std::vector<ToolResult> results;
    results.reserve(tasks.size());

    // Track which dependencies are completed
    std::set<std::string> completedIds;

    for (size_t i = 0; i < tasks.size(); ++i) {
        const Task& task = tasks[i];

        // Check if all dependencies are completed
        bool depsMet = true;
        for (const auto& depId : task.dependencies) {
            if (completedIds.find(depId) == completedIds.end()) {
                depsMet = false;
                break;
            }
        }

        if (!depsMet) {
            // Cannot execute - dependency not met
            ToolResult result;
            result.success = false;
            result.error = "Dependency not met: " + task.description;
            results.push_back(result);
            continue;
        }

        // Check workspace validation
        if (m_workspace != nullptr) {
            // TODO: Validate all paths in task.toolArguments
        }

        // Execute the task
        if (!task.toolName.empty()) {
            // Build ToolCall arguments
            nlohmann::json args = task.toolArguments.empty() ? nlohmann::json::object() : task.toolArguments;

            // Build ToolCall
            ToolCall call;
            call.id = task.id;
            call.name = task.toolName;
            call.arguments = args;

            // Execute through tool registry
            ToolResult result = m_tools.Execute(call);
            results.push_back(result);

            // Mark as completed if successful
            if (result.success) {
                completedIds.insert(task.id);
            }
        } else {
            ToolResult result;
            result.success = false;
            result.error = "No tool specified for task: " + task.description;
            results.push_back(result);
        }
    }

    return results;
}

} // namespace zagent

// Note: This file contains UTF-8 characters that require proper encoding handling
// The Chinese text in this file may cause compilation issues on Windows
// Consider using UTF-8 pragmas or encoding declarations if needed