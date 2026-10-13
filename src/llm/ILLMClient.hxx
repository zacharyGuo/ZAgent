#pragma once

#include <vector>

#include "agent/Message.hxx"
#include "llm/LLMResponse.hxx"

namespace zagent {

class ILLMClient {
public:
    virtual ~ILLMClient() = default;

    virtual LLMResponse Chat(
        const std::vector<Message>& messages,
        const std::vector<ToolDefinition>& tools) = 0;
};

} // namespace zagent
