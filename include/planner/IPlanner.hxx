#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tools/ITool.hxx"

namespace zagent {

class Workspace;

/// Task represents a single step in a planner's execution plan.
struct Task {
    std::string id;
    std::string description;
    std::vector<std::string> dependencies;

    enum class State {
        Pending,
        Running,
        Completed,
        Failed
    } state;

    // Optional: tool name and arguments to execute for this task
    std::string toolName;
    nlohmann::json toolArguments;

    Task() : state(State::Pending) {}
};

/// Abstract interface for the Planner component.
///
/// The Planner breaks down complex user tasks into executable steps
/// and manages their execution.
class IPlanner {
public:
    virtual ~IPlanner() = default;

    // Generate a task plan from a user goal.
    // @param goal The user's high-level goal (e.g., "Add Foo function")
    // @param context Repository context (workspace path, available tools, etc.)
    // @return Vector of tasks to execute
    virtual std::vector<Task> Plan(
        const std::string& goal,
        const std::string& context) = 0;

    // Execute a plan and return results for each task.
    // @param tasks The tasks to execute
    // @return Vector of ToolResult, one per task
    virtual std::vector<ToolResult> ExecutePlan(const std::vector<Task>& tasks) = 0;

    // Get all available tool names that the planner can use.
    // @return Vector of tool name strings
    virtual std::vector<std::string> GetAvailableTools() const = 0;

    // Set the workspace for security validation.
    virtual void SetWorkspace(const Workspace* workspace) { m_workspace = workspace; }

    // Get the current workspace.
    const Workspace* GetWorkspace() const { return m_workspace; }

protected:
    const Workspace* m_workspace = nullptr;
};

} // namespace zagent