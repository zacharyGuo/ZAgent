#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace zagent {

struct ToolDefinition {
    std::string name;
    std::string description;
    nlohmann::json parameters;
};

enum class LLMResponseType {
    Text,
    ToolCall
};

struct ToolCall {
    std::string id;
    std::string name;
    nlohmann::json arguments;
};

struct LLMResponse {
    bool success;
    LLMResponseType type;
    std::string content;
    std::vector<ToolCall> toolCalls;
    std::string error;
    std::string model;
    std::string requestId;

    LLMResponse();
};

const char* ToString(LLMResponseType type);

} // namespace zagent
