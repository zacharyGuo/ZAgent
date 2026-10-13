#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <memory>

#include "agent/Message.hxx"

namespace zagent {

// Session represents a single conversation
class Session {
public:
    explicit Session(const std::string& id);

    // Get session ID
    const std::string& Id() const;

    // Get creation time (as string)
    const std::string& CreatedAt() const;

    // Get last activity time (as string)
    const std::string& LastActivity() const;

    // Get message history
    const std::vector<Message>& Messages() const;

    // Add user message
    void AddUserMessage(const std::string& content);

    // Add assistant message
    void AddAssistantMessage(const std::string& content);

    // Clear all messages
    void Clear();

    // Update last activity timestamp
    void UpdateActivity();

private:
    std::string m_id;
    std::string m_createdAt;
    std::string m_lastActivity;
    std::vector<Message> m_messages;
    mutable std::mutex m_mutex;
};

// Session manager handles multiple sessions
class SessionManager {
public:
    static SessionManager& Instance();

    // Get or create a session by ID
    std::shared_ptr<Session> GetSession(const std::string& sessionId);

    // Create a new session
    std::shared_ptr<Session> CreateSession();

    // Remove a session
    bool RemoveSession(const std::string& sessionId);

    // Get all session IDs
    std::vector<std::string> GetAllSessionIds() const;

    // Get session by ID (returns nullptr if not found)
    std::shared_ptr<Session> GetSessionById(const std::string& sessionId) const;

private:
    SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    std::unordered_map<std::string, std::shared_ptr<Session>> m_sessions;
    mutable std::mutex m_mutex;
};

} // namespace zagent