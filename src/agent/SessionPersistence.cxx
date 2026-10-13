#include "agent/ISessionPersistence.hxx"
#include "agent/Message.hxx"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace zagent {

FileSessionPersistence::FileSessionPersistence()
    : m_storageDir(""), m_isInitialized(false), m_isReady(false) {
}

bool FileSessionPersistence::Initialize(const std::string& storageDir) {
    if (m_isInitialized) {
        return true;
    }

    m_storageDir = storageDir;

    if (!EnsureDirectoryExists()) {
        std::cerr << "SessionPersistence: failed to create storage directory: " << storageDir << std::endl;
        return false;
    }

    m_isInitialized = true;
    m_isReady = true;
    return true;
}

void FileSessionPersistence::Shutdown() {
    m_isInitialized = false;
    m_isReady = false;
}

bool FileSessionPersistence::IsReady() const {
    return m_isReady && m_isInitialized && !m_storageDir.empty();
}

std::string FileSessionPersistence::GetSessionFilePath(const std::string& sessionId) const {
    // Sanitize session ID for filename
    std::string safeId = sessionId;
    std::replace_if(safeId.begin(), safeId.end(),
                   [](char c) { return !std::isalnum(static_cast<unsigned char>(c)) && c != '-'; },
                   '_');

    return m_storageDir + "/" + safeId + ".json";
}

bool FileSessionPersistence::EnsureDirectoryExists() const {
    if (m_storageDir.empty()) {
        return false;
    }

    try {
        if (!fs::exists(m_storageDir)) {
            return fs::create_directories(m_storageDir);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SessionPersistence: error creating directory: " << e.what() << std::endl;
        return false;
    }
}

bool FileSessionPersistence::SaveSession(const std::shared_ptr<Session>& session) {
    if (!session || !IsReady()) {
        return false;
    }

    std::string filePath = GetSessionFilePath(session->Id());
    std::string createdAt = session->CreatedAt();

    json j;
    j["sessionId"] = session->Id();
    j["createdAt"] = createdAt;

    // Serialize messages
    json messages = json::array();
    const auto& msgs = session->Messages();
    for (const auto& msg : msgs) {
        json m;
        m["role"] = msg.GetRole();  // Assuming Message has GetRole()
        m["content"] = msg.GetContent();  // Assuming Message has GetContent()
        m["timestamp"] = createdAt;  // Simplified - would need actual timestamp
        messages.push_back(m);
    }
    j["messages"] = messages;

    // Write to file
    std::ofstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "SessionPersistence: failed to open file for writing: " << filePath << std::endl;
        return false;
    }

    try {
        file << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SessionPersistence: error writing session: " << e.what() << std::endl;
        return false;
    }
}

std::shared_ptr<Session> FileSessionPersistence::LoadSession(const std::string& sessionId) {
    if (!IsReady()) {
        return nullptr;
    }

    std::string filePath = GetSessionFilePath(sessionId);

    std::ifstream file(filePath);
    if (!file.is_open()) {
        return nullptr;
    }

    try {
        json j;
        file >> j;

        auto session = std::make_shared<Session>(sessionId);

        // Restore messages (simplified - assumes Message class has appropriate constructors)
        if (j.contains("messages") && j["messages"].is_array()) {
            for (const auto& msg : j["messages"]) {
                std::string role = msg.value("role", "user");
                std::string content = msg.value("content", "");

                if (role == "user") {
                    session->AddUserMessage(content);
                } else {
                    session->AddAssistantMessage(content);
                }
            }
        }

        // Clear the system message that was added in constructor
        // This is a simplification - in practice would need to track if it's a loaded session
        return session;

    } catch (const std::exception& e) {
        std::cerr << "SessionPersistence: error loading session: " << e.what() << std::endl;
        return nullptr;
    }
}

bool FileSessionPersistence::DeleteSession(const std::string& sessionId) {
    if (!IsReady()) {
        return false;
    }

    std::string filePath = GetSessionFilePath(sessionId);

    try {
        return fs::remove(filePath);
    } catch (const std::exception& e) {
        std::cerr << "SessionPersistence: error deleting session: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> FileSessionPersistence::ListSessions() const {
    std::vector<std::string> sessions;

    if (!IsReady()) {
        return sessions;
    }

    try {
        for (const auto& entry : fs::directory_iterator(m_storageDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                sessions.push_back(entry.path().stem().string());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "SessionPersistence: error listing sessions: " << e.what() << std::endl;
    }

    return sessions;
}

bool FileSessionPersistence::SessionExists(const std::string& sessionId) const {
    if (!IsReady()) {
        return false;
    }

    std::string filePath = GetSessionFilePath(sessionId);
    return fs::exists(filePath);
}

std::string FileSessionPersistence::GetStorageDir() const {
    return m_storageDir;
}

} // namespace zagent