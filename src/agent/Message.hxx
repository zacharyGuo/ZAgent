#pragma once

#include <string>

namespace zagent {

enum class MessageRole {
    System,
    User,
    Assistant,
    Tool
};

struct Message {
    MessageRole role;
    std::string content;
    std::string toolCallId;
    std::string name;

    static Message System(std::string content);
    static Message User(std::string content);
    static Message Assistant(std::string content);
    static Message ToolResult(std::string toolCallId, std::string content);

    // Accessors for serialization
    MessageRole GetRole() const { return role; }
    const std::string& GetContent() const { return content; }
};

const char* ToString(MessageRole role);

} // namespace zagent
