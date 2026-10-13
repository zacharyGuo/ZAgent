#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <unordered_set>
#include <utility>
#include <mutex>

namespace zagent {

/// Permission action types
enum class PermissionAction {
    Allow,      // Allow the operation
    Deny,       // Deny the operation
    AllowAlways, // Allow and remember for this session
    Cancel      // Cancel the operation
};

/// Permission categories for different operation types
enum class PermissionCategory {
    FileRead,       // Reading files
    FileWrite,      // Writing/modifying files
    FileDelete,     // Deleting files
    ShellExecute,   // Executing shell commands
    GitOperation,   // Git operations (status, diff, log)
    NetworkRequest, // Network/HTTP requests
    CodeModify,     // Applying code patches
    BuildTest       // Build and test operations
};

/// Permission request details
struct PermissionRequest {
    std::string id;              // Unique request ID
    PermissionCategory category; // Operation category
    std::string operation;       // Human-readable operation description
    std::string target;          // Target path or resource
    std::string details;         // Additional details for user
    bool isDestructive;          // Whether operation is potentially destructive
    std::chrono::system_clock::time_point timestamp; // Request time

    PermissionRequest()
        : category(PermissionCategory::FileRead), isDestructive(false),
          timestamp(std::chrono::system_clock::now()) {}

    PermissionRequest(PermissionCategory cat, const std::string& op,
                     const std::string& tgt, const std::string& det = "",
                     bool destructive = false)
        : category(cat), operation(op), target(tgt), details(det),
          isDestructive(destructive),
          timestamp(std::chrono::system_clock::now()) {}
};

/// Permission response from user
struct PermissionResponse {
    std::string requestId;
    PermissionAction action;
    std::chrono::system_clock::time_point timestamp;

    PermissionResponse() : action(PermissionAction::Deny),
        timestamp(std::chrono::system_clock::now()) {}

    PermissionResponse(const std::string& id, PermissionAction act)
        : requestId(id), action(act),
          timestamp(std::chrono::system_clock::now()) {}
};

/// Callback type for permission prompts
using PermissionCallback = std::function<PermissionAction(const PermissionRequest&)>;

/// Abstract interface for permission management
class IPermissionManager {
public:
    virtual ~IPermissionManager() = default;

    // Check if an operation is allowed without prompting
    // Returns true if explicitly allowed or already granted
    virtual bool IsAllowed(PermissionCategory category, const std::string& target,
                          const std::string& operation = "") = 0;

    // Request permission for an operation
    // If callback is set, it will be invoked to get user decision
    virtual PermissionResponse RequestPermission(const PermissionRequest& request) = 0;

    // Grant permission for a category/target combination (remember for session)
    virtual void GrantPermission(PermissionCategory category, const std::string& target) = 0;

    // Deny permission for a category/target combination
    virtual void DenyPermission(PermissionCategory category, const std::string& target) = 0;

    // Check if permission is explicitly granted
    virtual bool IsGranted(PermissionCategory category, const std::string& target) const = 0;

    // Check if permission is explicitly denied
    virtual bool IsDenied(PermissionCategory category, const std::string& target) const = 0;

    // Get all granted permissions
    virtual std::vector<std::pair<PermissionCategory, std::string>> GetGrantedPermissions() const = 0;

    // Get all denied permissions
    virtual std::vector<std::pair<PermissionCategory, std::string>> GetDeniedPermissions() const = 0;

    // Clear all permissions
    virtual void ClearPermissions() = 0;

    // Set the permission callback (for user interaction)
    virtual void SetPermissionCallback(PermissionCallback callback) = 0;

    // Get current callback
    virtual PermissionCallback GetPermissionCallback() const = 0;

    // Enable/disable permission system
    virtual void SetEnabled(bool enabled) = 0;

    // Check if permission system is enabled
    virtual bool IsEnabled() const = 0;
};

/// Convenience function to convert PermissionCategory to string
std::string PermissionCategoryToString(PermissionCategory category);

/// Convenience function to convert PermissionAction to string
std::string PermissionActionToString(PermissionAction action);

/// Concrete permission manager implementation.
class PermissionManager : public IPermissionManager {
public:
    PermissionManager();
    ~PermissionManager() override = default;

    // IPermissionManager interface
    bool IsAllowed(PermissionCategory category, const std::string& target,
                   const std::string& operation = "") override;
    PermissionResponse RequestPermission(const PermissionRequest& request) override;
    void GrantPermission(PermissionCategory category, const std::string& target) override;
    void DenyPermission(PermissionCategory category, const std::string& target) override;
    bool IsGranted(PermissionCategory category, const std::string& target) const override;
    bool IsDenied(PermissionCategory category, const std::string& target) const override;
    std::vector<std::pair<PermissionCategory, std::string>> GetGrantedPermissions() const override;
    std::vector<std::pair<PermissionCategory, std::string>> GetDeniedPermissions() const override;
    void ClearPermissions() override;
    void SetPermissionCallback(PermissionCallback callback) override;
    PermissionCallback GetPermissionCallback() const override;
    void SetEnabled(bool enabled) override;
    bool IsEnabled() const override;

private:
    // Helper hash for pair (must be defined before use in member variables)
    struct PairHash {
        std::size_t operator()(const std::pair<PermissionCategory, std::string>& p) const {
            return std::hash<std::string>()(
                       std::string(1, static_cast<char>(static_cast<int>(p.first))) +
                       p.second);
        }
    };

    PermissionCallback m_callback;
    bool m_enabled;
    // Using maps for (category, target) -> bool
    std::unordered_set<std::pair<PermissionCategory, std::string>, PairHash> m_granted;
    std::unordered_set<std::pair<PermissionCategory, std::string>, PairHash> m_denied;
    mutable std::mutex m_mutex; // For thread safety
};

/// Permission checker for tool operations
class ToolPermissionChecker {
public:
    explicit ToolPermissionChecker(IPermissionManager* permManager = nullptr)
        : m_permManager(permManager) {}

    // Set the permission manager
    void SetPermissionManager(IPermissionManager* pm) { m_permManager = pm; }

    // Check file read permission
    bool CheckFileRead(const std::string& path);

    // Check file write permission
    bool CheckFileWrite(const std::string& path);

    // Check file delete permission
    bool CheckFileDelete(const std::string& path);

    // Check shell execute permission
    bool CheckShellExecute(const std::string& command);

    // Check git operation permission
    bool CheckGitOperation(const std::string& operation);

    // Check network request permission
    bool CheckNetworkRequest(const std::string& url);

    // Check code modify permission
    bool CheckCodeModify(const std::string& filePath);

    // Check build/test permission
    bool CheckBuildTest(const std::string& operation);

private:
    IPermissionManager* m_permManager;
};

} // namespace zagent