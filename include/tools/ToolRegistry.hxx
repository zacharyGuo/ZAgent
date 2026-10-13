#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "llm/LLMResponse.hxx"
#include "workspace/Workspace.hxx"

namespace zagent {

class ITool;

/// Registry for managing tools
///
/// Provides:
/// - Tool registration
/// - Tool lookup by name
/// - Tool execution
/// - Tool definition generation for LLM
class ToolRegistry {
public:
    ToolRegistry() = default;
    ~ToolRegistry() = default;

    // Register a tool with the registry
    void Register(std::unique_ptr<ITool> tool);

    // Execute a tool call
    ToolResult Execute(const ToolCall& call);

    // Get tool definitions for LLM
    std::vector<ToolDefinition> GetDefinitions() const;

    // Check if a tool is registered
    bool Contains(const std::string& name) const;

    // Set the workspace for all tools
    void SetWorkspace(const Workspace* workspace);

    // Get the workspace
    const Workspace* GetWorkspace() const { return m_workspace; }

private:
    std::unordered_map<std::string, std::unique_ptr<ITool>> m_tools;
    const Workspace* m_workspace = nullptr;
};

} // namespace zagent