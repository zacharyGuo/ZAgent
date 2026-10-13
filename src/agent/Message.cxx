#include "agent/Message.hxx"

namespace zagent {

Message Message::System(std::string content) {
    return Message{MessageRole::System, std::move(content), "", ""};
}

Message Message::User(std::string content) {
    return Message{MessageRole::User, std::move(content), "", ""};
}

Message Message::Assistant(std::string content) {
    return Message{MessageRole::Assistant, std::move(content), "", ""};
}

Message Message::ToolResult(std::string toolCallId, std::string content) {
    return Message{MessageRole::Tool, std::move(content), std::move(toolCallId), ""};
}

const char* ToString(MessageRole role) {
    switch (role) {
    case MessageRole::System:
        return "system";
    case MessageRole::User:
        return "user";
    case MessageRole::Assistant:
        return "assistant";
    case MessageRole::Tool:
        return "tool";
    }

    return "unknown";
}

} // namespace zagent
