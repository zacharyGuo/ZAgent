#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "tools/ITool.hxx"

namespace zagent {

class ToolRegistry {
public:
    void Register(std::unique_ptr<ITool> tool);

    // Execute a tool call from the LLM
    ToolResult Execute(const ToolCall& call);

    // Get all tool definitions for the LLM
    std::vector<ToolDefinition> GetDefinitions() const;

    // Check if a tool is registered
    bool Contains(const std::string& name) const;

    // Set workspace for all registered tools
    void SetWorkspace(const Workspace* workspace);

private:
    std::unordered_map<std::string, std::unique_ptr<ITool>> m_tools;
    const Workspace* m_workspace = nullptr;
};

} // namespace zagent