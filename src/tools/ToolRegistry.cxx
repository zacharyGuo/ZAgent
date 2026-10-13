#include "tools/ToolRegistry.hxx"

#include <stdexcept>

namespace zagent {

void ToolRegistry::Register(std::unique_ptr<ITool> tool) {
    if (!tool) {
        throw std::invalid_argument("Cannot register null tool");
    }

    const std::string name = tool->Name();
    if (name.empty()) {
        throw std::invalid_argument("Tool name cannot be empty");
    }
    if (m_tools.find(name) != m_tools.end()) {
        throw std::invalid_argument("Tool already registered: " + name);
    }

    m_tools.emplace(name, std::move(tool));
}

ToolResult ToolRegistry::Execute(const ToolCall& call) {
    const auto it = m_tools.find(call.name);
    if (it == m_tools.end()) {
        return ToolResult{false, {}, "Unknown tool: " + call.name};
    }

    try {
        return it->second->Execute(call.arguments);
    } catch (const std::exception& exception) {
        return ToolResult{false, {}, exception.what()};
    } catch (...) {
        return ToolResult{false, {}, "Tool execution failed with unknown error"};
    }
}

std::vector<ToolDefinition> ToolRegistry::GetDefinitions() const {
    std::vector<ToolDefinition> definitions;
    definitions.reserve(m_tools.size());

    for (const auto& [name, tool] : m_tools) {
        (void)name;
        definitions.push_back(
            ToolDefinition{tool->Name(), tool->Description(), tool->Parameters()});
    }

    return definitions;
}

bool ToolRegistry::Contains(const std::string& name) const {
    return m_tools.find(name) != m_tools.end();
}

void ToolRegistry::SetWorkspace(const Workspace* workspace) {
    m_workspace = workspace;
    for (auto& [name, tool] : m_tools) {
        tool->SetWorkspace(workspace);
    }
}

} // namespace zagent