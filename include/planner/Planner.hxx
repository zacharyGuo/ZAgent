#pragma once

#include <string>
#include <vector>

#include "planner/IPlanner.hxx"
#include "tools/ToolRegistry.hxx"

namespace zagent {

/// Concrete Planner implementation that breaks down user goals into tasks.
///
/// The Planner analyzes the user's request and generates a sequence of
/// tasks, each representing a tool call or other action. It manages
/// task dependencies and execution order.
class Planner : public IPlanner {
public:
    explicit Planner(ToolRegistry& tools);

    // Plan breaks down a user goal into executable tasks.
    std::vector<Task> Plan(
        const std::string& goal,
        const std::string& context) override;

    // ExecutePlan executes all tasks and returns their results.
    std::vector<ToolResult> ExecutePlan(const std::vector<Task>& tasks) override;

    // GetAvailableTools returns the names of tools registered with the registry.
    std::vector<std::string> GetAvailableTools() const override;

    // SetWorkspace sets the security boundary for task execution.
    void SetWorkspace(const Workspace* workspace) override;

    // GetWorkspace returns the current security boundary.
    const Workspace* GetWorkspace() const;

private:
    // Analyze the goal and create initial tasks
    std::vector<Task> AnalyzeGoal(const std::string& goal, const std::string& context);

    // Determine what tool to use for a given task description
    std::string DetermineTool(const std::string& taskDescription) const;

    // Extract search pattern from goal
    std::string ExtractSearchPattern(const std::string& goal);

    // Extract git command from goal
    std::string ExtractGitCommand(const std::string& goal);

    ToolRegistry& m_tools;
    const Workspace* m_workspace;
};

} // namespace zagent