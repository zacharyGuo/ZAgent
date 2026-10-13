#pragma once

#include <memory>
#include <string>
#include <functional>

#include "web/HttpServer.hxx"
#include "agent/Session.hxx"
#include "llm/ILLMClient.hxx"
#include "tools/ToolRegistry.hxx"

namespace zagent {

// Chat endpoint handler - processes /api/chat requests
class ChatEndpoint {
public:
    // Construct with LLM client (can be null for stub mode), system prompt, and tool registry
    ChatEndpoint(std::unique_ptr<ILLMClient> llmClient,
                 std::string systemPrompt,
                 ToolRegistry* toolRegistry = nullptr);
    ~ChatEndpoint() = default;

    // Delete copy, allow move
    ChatEndpoint(const ChatEndpoint&) = delete;
    ChatEndpoint& operator=(const ChatEndpoint&) = delete;
    ChatEndpoint(ChatEndpoint&&) = default;
    ChatEndpoint& operator=(ChatEndpoint&&) = default;

    // Register the endpoint with the HTTP server
    void Register(HttpServer& server);

private:
    // Request handler for /api/chat
    HttpResponse HandleChat(const HttpRequest& request);

    // Process chat with optional tool calling
    std::string ProcessChat(const std::string& sessionId, const std::string& message);

    // Generate JSON chat response
    std::string GenerateChatResponse(const std::string& sessionId,
                                     const std::string& reply,
                                     const std::string& toolCall = "");

    std::unique_ptr<ILLMClient> m_llmClient;
    std::string m_systemPrompt;
    ToolRegistry* m_toolRegistry;
};

} // namespace zagent