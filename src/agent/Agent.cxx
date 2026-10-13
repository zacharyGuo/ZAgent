#include "agent/Agent.hxx"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace zagent {
namespace {

bool IsBlank(const std::string& value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

} // namespace

Agent::Agent(std::unique_ptr<ILLMClient> llm,
             ToolRegistry& tools,
             std::string systemPrompt)
    : m_llm(std::move(llm)),
      m_tools(tools),
      m_messages(),
      m_state(AgentState::Idle),
      m_iterationCount(0),
      m_systemPrompt(std::move(systemPrompt)) {
    if (!m_llm) {
        throw std::invalid_argument("Agent requires an ILLMClient");
    }
    if (m_systemPrompt.empty()) {
        throw std::invalid_argument("Agent system prompt cannot be empty");
    }

    Reset();
}

std::string Agent::Run(const std::string& userInput) {
    if (m_state == AgentState::Running) {
        throw std::logic_error("Agent is already running");
    }
    if (IsBlank(userInput)) {
        throw std::invalid_argument("User input cannot be empty");
    }

    m_messages.push_back(Message::User(userInput));
    m_state = AgentState::Running;
    m_iterationCount = 0;

    try {
        while (m_iterationCount < kMaxIterations) {
            ++m_iterationCount;

            LLMResponse response = m_llm->Chat(m_messages, m_tools.GetDefinitions());
            if (!response.success) {
                throw std::runtime_error(response.error.empty()
                                             ? "LLM request failed"
                                             : response.error);
            }

            if (!response.toolCalls.empty()) {
                for (const auto& call : response.toolCalls) {
                    const ToolResult result = m_tools.Execute(call);

                    m_messages.push_back(Message::ToolResult(call.id, result.output));
                    if (!result.success) {
                        // If the tool call failed, include the error in the message
                        // so the LLM can retry or report the issue to the user
                        m_messages.back().content += "\n\nError: " + result.error;
                    }
                }
                continue;
            }

            if (response.content.empty()) {
                throw std::runtime_error("LLM returned an empty response");
            }

            m_messages.push_back(Message::Assistant(response.content));
            m_state = AgentState::Completed;
            return response.content;
        }
    } catch (...) {
        m_state = AgentState::Failed;
        throw;
    }

    m_state = AgentState::Failed;
    throw std::runtime_error("Maximum agent iterations exceeded");
}

void Agent::Reset() {
    if (m_state == AgentState::Running) {
        throw std::logic_error("Cannot reset a running Agent");
    }

    m_messages.clear();
    m_messages.push_back(Message::System(m_systemPrompt));
    m_state = AgentState::Idle;
    m_iterationCount = 0;
}

const std::vector<Message>& Agent::Messages() const {
    return m_messages;
}

AgentState Agent::State() const {
    return m_state;
}

int Agent::IterationCount() const {
    return m_iterationCount;
}

} // namespace zagent