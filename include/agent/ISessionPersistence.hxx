#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "agent/Session.hxx"

namespace zagent {

/// Abstract interface for session persistence (saving/loading sessions to/from disk).
class ISessionPersistence {
public:
    virtual ~ISessionPersistence() = default;

    // Initialize the persistence layer with a storage directory
    // @param storageDir Directory where sessions will be saved
    virtual bool Initialize(const std::string& storageDir) = 0;

    // Shutdown the persistence layer
    virtual void Shutdown() = 0;

    // Check if persistence is ready
    virtual bool IsReady() const = 0;

    // Save a session to disk
    // @param session The session to save
    // @return true if save succeeded
    virtual bool SaveSession(const std::shared_ptr<Session>& session) = 0;

    // Load a session from disk
    // @param sessionId The ID of the session to load
    // @return Shared pointer to the loaded session, or nullptr if not found/failed
    virtual std::shared_ptr<Session> LoadSession(const std::string& sessionId) = 0;

    // Delete a session from disk
    // @param sessionId The ID of the session to delete
    // @return true if deletion succeeded
    virtual bool DeleteSession(const std::string& sessionId) = 0;

    // List all available session IDs
    // @return Vector of session IDs
    virtual std::vector<std::string> ListSessions() const = 0;

    // Check if a session exists on disk
    // @param sessionId The session ID to check
    // @return true if session file exists
    virtual bool SessionExists(const std::string& sessionId) const = 0;

    // Get the storage directory
    virtual std::string GetStorageDir() const = 0;
};

/// Concrete implementation using JSON files on disk.
class FileSessionPersistence : public ISessionPersistence {
public:
    FileSessionPersistence();
    ~FileSessionPersistence() override = default;

    bool Initialize(const std::string& storageDir) override;
    void Shutdown() override;
    bool IsReady() const override;

    bool SaveSession(const std::shared_ptr<Session>& session) override;
    std::shared_ptr<Session> LoadSession(const std::string& sessionId) override;
    bool DeleteSession(const std::string& sessionId) override;
    std::vector<std::string> ListSessions() const override;
    bool SessionExists(const std::string& sessionId) const override;
    std::string GetStorageDir() const override;

private:
    std::string m_storageDir;
    bool m_isInitialized = false;
    bool m_isReady = false;

    std::string GetSessionFilePath(const std::string& sessionId) const;
    bool EnsureDirectoryExists() const;
};

} // namespace zagent