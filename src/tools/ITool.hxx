#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "llm/LLMResponse.hxx"
#include "workspace/Workspace.hxx"

namespace zagent {

struct ToolResult {
    bool success;
    std::string output;
    std::string error;
};

class ITool {
public:
    virtual ~ITool() = default;

    // Tool name (e.g., "list_files")
    virtual std::string Name() const = 0;

    // Human-readable description for the LLM
    virtual std::string Description() const = 0;

    // JSON Schema for tool parameters
    virtual nlohmann::json Parameters() const = 0;

    // Execute the tool with given arguments
    // Returns ToolResult with success flag, output, and error
    virtual ToolResult Execute(const nlohmann::json& arguments) = 0;

    // Set the workspace (security boundary) for this tool
    virtual void SetWorkspace(const Workspace* workspace) { m_workspace = workspace; }

    // Get the current workspace
    const Workspace* GetWorkspace() const { return m_workspace; }

protected:
    const Workspace* m_workspace = nullptr;
};

} // namespace zagent