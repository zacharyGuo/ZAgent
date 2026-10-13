#include "llm/LLMResponse.hxx"

namespace zagent {

LLMResponse::LLMResponse()
    : success(false),
      type(LLMResponseType::Text),
      content(),
      toolCalls(),
      error(),
      model(),
      requestId() {}

const char* ToString(LLMResponseType type) {
    switch (type) {
    case LLMResponseType::Text:
        return "text";
    case LLMResponseType::ToolCall:
        return "tool_call";
    }

    return "unknown";
}

} // namespace zagent
