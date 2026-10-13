#include "web/ChatEndpoint.hxx"
#include "agent/Session.hxx"
#include "agent/Agent.hxx"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace zagent {

namespace {

// Process a chat request with optional tool calling (same logic as Agent::Run)
std::string ProcessChatWithTools(
    ILLMClient& llmClient,
    ToolRegistry& toolRegistry,
    const std::string& systemPrompt,
    const std::vector<Message>& history) {
    std::vector<Message> messages;
    messages.push_back(Message::System(systemPrompt));
    for (const auto& msg : history) {
        messages.push_back(msg);
    }

    for (int iteration = 0; iteration < Agent::kMaxIterations; ++iteration) {
        LLMResponse response = llmClient.Chat(messages, toolRegistry.GetDefinitions());
        if (!response.success) {
            throw std::runtime_error(
                response.error.empty() ? "LLM request failed" : response.error);
        }

        if (!response.toolCalls.empty()) {
            for (const auto& call : response.toolCalls) {
                const ToolResult result = toolRegistry.Execute(call);
                messages.push_back(Message::ToolResult(call.id, result.output));
                if (!result.success) {
                    messages.back().content += "\n\nError: " + result.error;
                }
            }
            continue;
        }

        if (response.content.empty()) {
            throw std::runtime_error("LLM returned an empty response");
        }

        return response.content;
    }

    throw std::runtime_error("Maximum agent iterations exceeded");
}

} // namespace

ChatEndpoint::ChatEndpoint(
    std::unique_ptr<ILLMClient> llmClient,
    std::string systemPrompt,
    ToolRegistry* toolRegistry)
    : m_llmClient(std::move(llmClient)),
      m_systemPrompt(std::move(systemPrompt)),
      m_toolRegistry(toolRegistry) {}

void ChatEndpoint::Register(HttpServer& server) {
    std::cout << "[ChatEndpoint] Registering routes...\n";
    server.AddRoute(HttpMethod::Post, "/api/chat",
                    [this](const HttpRequest& request) {
                        std::cout << "[ChatEndpoint] Handling /api/chat POST\n";
                        return HandleChat(request);
                    });
    std::cout << "[ChatEndpoint] Registered /api/chat\n";

    server.AddRoute(HttpMethod::Get, "/api/health",
                    [](const HttpRequest&) {
                        std::cout << "[ChatEndpoint] Handling /api/health GET\n";
                        HttpResponse response;
                        response.statusCode = 200;
                        response.statusText = "OK";
                        response.headers["Content-Type"] = "application/json";
                        response.body = R"({"status":"ok"})";
                        return response;
                    });
    std::cout << "[ChatEndpoint] Registered /api/health\n";

    server.AddRoute(HttpMethod::Get, "/api/sessions",
                    [](const HttpRequest&) {
                        std::cout << "[ChatEndpoint] Handling /api/sessions GET\n";
                        HttpResponse response;
                        response.statusCode = 200;
                        response.statusText = "OK";
                        response.headers["Content-Type"] = "application/json";

                        nlohmann::json j;
                        j["sessions"] = SessionManager::Instance().GetAllSessionIds();
                        response.body = j.dump();
                        return response;
                    });
    std::cout << "[ChatEndpoint] Registered /api/sessions\n";
}

HttpResponse ChatEndpoint::HandleChat(const HttpRequest& request) {
    try {
        const auto requestBody = nlohmann::json::parse(request.body);
        const std::string sessionId = requestBody.value("sessionId", "");
        const std::string message = requestBody.value("message", "");

        if (message.empty()) {
            throw std::invalid_argument("message cannot be empty");
        }

        auto session = SessionManager::Instance().GetSession(sessionId);
        session->AddUserMessage(message);

        std::string reply;
        std::string toolCallInfo;
        if (m_llmClient) {
            // Use the same tool-calling loop as the console Agent
            // Pass the session's message history for context persistence
            if (m_toolRegistry != nullptr) {
                auto& sessionMessages = session->Messages();
                // Exclude the system message from the first position (already included in ProcessChatWithTools)
                std::vector<Message> history(sessionMessages.begin() + 1, sessionMessages.end());
                reply = ProcessChatWithTools(
                    *m_llmClient,
                    *m_toolRegistry,
                    m_systemPrompt,
                    history);
                // Extract tool call info for frontend display
                const auto& msgHist = session->Messages();
                if (msgHist.size() >= 3) {
                    const auto& lastMsg = msgHist[msgHist.size() - 2];  // Last was user message
                    // Tool call info could be extracted from the message history
                }
            } else {
                LLMResponse llmResponse = m_llmClient->Chat(
                    session->Messages(), std::vector<ToolDefinition>{});
                if (llmResponse.success && !llmResponse.content.empty()) {
                    reply = llmResponse.content;
                } else {
                    throw std::runtime_error(
                        llmResponse.error.empty()
                            ? "LLM returned empty response"
                            : llmResponse.error);
                }
            }
        } else {
            // Stub mode - echo the message back
            reply = "Echo: " + message;
        }

        session->AddAssistantMessage(reply);

        HttpResponse response;
        response.statusCode = 200;
        response.statusText = "OK";
        response.headers["Content-Type"] = "application/json";
        response.body = GenerateChatResponse(sessionId, reply, toolCallInfo);
        return response;
    } catch (const std::exception& exception) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.headers["Content-Type"] = "application/json";
        response.body = R"({"error":)" +
                        std::string(exception.what()) + R"("})";
        return response;
    }
}

std::string ChatEndpoint::GenerateChatResponse(
    const std::string& sessionId,
    const std::string& reply,
    const std::string& toolCall) {
    nlohmann::json response;
    response["sessionId"] = sessionId;
    response["reply"] = reply;
    if (!toolCall.empty()) {
        response["toolCall"] = toolCall;
    }
    return response.dump();
}

} // namespace zagent