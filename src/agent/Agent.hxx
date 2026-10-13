#pragma once

#include <memory>
#include <string>
#include <vector>

#include "agent/Message.hxx"
#include "llm/ILLMClient.hxx"
#include "tools/ToolRegistry.hxx"

namespace zagent {

enum class AgentState {
    Idle,
    Running,
    Completed,
    Failed
};

class Agent {
public:
    static constexpr int kMaxIterations = 30;
    static constexpr const char* kDefaultSystemPrompt =
        "You are ZAgent, an AI coding assistant. "
        "Give clear, accurate answers. Never claim to have inspected, built, "
        "tested, or modified a repository unless you actually did so.";

    explicit Agent(
        std::unique_ptr<ILLMClient> llm,
        ToolRegistry& tools,
        std::string systemPrompt = kDefaultSystemPrompt);

    std::string Run(const std::string& userInput);
    void Reset();

    const std::vector<Message>& Messages() const;
    AgentState State() const;
    int IterationCount() const;

private:
    std::unique_ptr<ILLMClient> m_llm;
    ToolRegistry& m_tools;
    std::vector<Message> m_messages;
    AgentState m_state;
    int m_iterationCount;
    std::string m_systemPrompt;
};

} // namespace zagent