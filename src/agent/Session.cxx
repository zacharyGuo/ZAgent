#include "agent/Session.hxx"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>

namespace zagent {

// Generate UUID v4
static std::string GenerateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            ss << '-';
        } else if (i == 14) {
            ss << '4';
        } else if (i == 19) {
            ss << (char)('8' + dis2(gen));
        } else {
            ss << std::hex << dis(gen);
        }
    }
    return ss.str();
}

// Format current time as ISO 8601 string
static std::string CurrentTimeISO() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

// Session implementation
Session::Session(const std::string& id)
    : m_id(id), m_createdAt(CurrentTimeISO()), m_lastActivity(CurrentTimeISO()) {
    // Initialize with system message
    m_messages.push_back(Message::System(
        "You are ZAgent, an AI coding assistant. "
        "Give clear, accurate answers. Never claim to have inspected, built, "
        "tested, or modified a repository unless you actually did so."));
}

const std::string& Session::Id() const {
    return m_id;
}

const std::string& Session::CreatedAt() const {
    return m_createdAt;
}

const std::string& Session::LastActivity() const {
    return m_lastActivity;
}

const std::vector<Message>& Session::Messages() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_messages;
}

void Session::AddUserMessage(const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_messages.push_back(Message::User(content));
    m_lastActivity = CurrentTimeISO();
}

void Session::AddAssistantMessage(const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_messages.push_back(Message::Assistant(content));
    m_lastActivity = CurrentTimeISO();
}

void Session::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_messages.clear();
    m_messages.push_back(Message::System(
        "You are ZAgent, an AI coding assistant. "
        "Give clear, accurate answers. Never claim to have inspected, built, "
        "tested, or modified a repository unless you actually did so."));
    m_lastActivity = CurrentTimeISO();
}

void Session::UpdateActivity() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastActivity = CurrentTimeISO();
}

// SessionManager implementation
SessionManager& SessionManager::Instance() {
    static SessionManager instance;
    return instance;
}

std::shared_ptr<Session> SessionManager::GetSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end()) {
        return it->second;
    }
    // Create new session if not found
    auto newSession = std::make_shared<Session>(sessionId);
    m_sessions[sessionId] = newSession;
    return newSession;
}

std::shared_ptr<Session> SessionManager::CreateSession() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string sessionId = GenerateUUID();
    auto session = std::make_shared<Session>(sessionId);
    m_sessions[sessionId] = session;
    return session;
}

bool SessionManager::RemoveSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sessions.erase(sessionId) > 0;
}

std::vector<std::string> SessionManager::GetAllSessionIds() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> ids;
    ids.reserve(m_sessions.size());
    for (const auto& [id, session] : m_sessions) {
        ids.push_back(id);
    }
    return ids;
}

std::shared_ptr<Session> SessionManager::GetSessionById(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace zagent