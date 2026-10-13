#pragma once

#include <string>
#include <vector>

#include "llm/ILLMClient.hxx"

namespace zagent {

class OpenAIClient final : public ILLMClient {
public:
    OpenAIClient(
        std::string baseUrl,
        std::string apiKey,
        std::string model);

    LLMResponse Chat(
        const std::vector<Message>& messages,
        const std::vector<ToolDefinition>& tools) override;

private:
    std::string BuildUrl() const;
    std::string BuildRequestBody(
        const std::vector<Message>& messages,
        const std::vector<ToolDefinition>& tools) const;
    LLMResponse ParseResponse(
        long httpStatus,
        const std::string& responseBody,
        const std::string& requestId) const;

private:
    std::string m_baseUrl;
    std::string m_apiKey;
    std::string m_model;
};

} // namespace zagent
