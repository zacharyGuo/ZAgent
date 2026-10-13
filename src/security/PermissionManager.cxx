#include "security/PermissionManager.hxx"

#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <sstream>
#include <iostream>

namespace zagent {

// Permission category to string conversion
std::string PermissionCategoryToString(PermissionCategory category) {
    switch (category) {
        case PermissionCategory::FileRead: return "FileRead";
        case PermissionCategory::FileWrite: return "FileWrite";
        case PermissionCategory::FileDelete: return "FileDelete";
        case PermissionCategory::ShellExecute: return "ShellExecute";
        case PermissionCategory::GitOperation: return "GitOperation";
        case PermissionCategory::NetworkRequest: return "NetworkRequest";
        case PermissionCategory::CodeModify: return "CodeModify";
        case PermissionCategory::BuildTest: return "BuildTest";
        default: return "Unknown";
    }
}

// Permission action to string conversion
std::string PermissionActionToString(PermissionAction action) {
    switch (action) {
        case PermissionAction::Allow: return "Allow";
        case PermissionAction::Deny: return "Deny";
        case PermissionAction::AllowAlways: return "AllowAlways";
        case PermissionAction::Cancel: return "Cancel";
        default: return "Unknown";
    }
}

// Helper: Check if path matches a pattern (supports wildcards)
static bool PathMatchesPattern(const std::string& path, const std::string& pattern) {
    if (pattern.empty() || path.empty()) {
        return pattern == path;
    }

    // Simple prefix matching for now (e.g., "*/CMakeCache.txt" matches any CMakeCache.txt)
    if (pattern.front() == '*' && pattern.back() == '*') {
        std::string middle = pattern.substr(1, pattern.length() - 2);
        return path.find(middle) != std::string::npos;
    }

    // Simple prefix match
    if (pattern.front() == '*') {
        std::string suffix = pattern.substr(1);
        return path.length() >= suffix.length() &&
               path.compare(path.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    // Simple suffix match
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.length() - 1);
        return path.length() >= prefix.length() &&
               path.compare(0, prefix.length(), prefix) == 0;
    }

    // Exact match
    return path == pattern;
}

// Default permission callback that denies all operations
static PermissionAction DefaultPermissionCallback(const PermissionRequest& request) {
    std::cerr << "Permission denied: " << request.operation << " on " << request.target << "\n";
    return PermissionAction::Deny;
}

// Permission Manager Implementation
PermissionManager::PermissionManager()
    : m_enabled(true),
      m_callback(DefaultPermissionCallback),
      m_granted(),
      m_denied()
{
    // Default granted permissions: file reads
    // (IsAllowed() returns true for FileRead by default)
    // Users should be prompted for writes, shell, git, etc.
}

void PermissionManager::SetPermissionCallback(PermissionCallback callback) {
    m_callback = callback ? callback : DefaultPermissionCallback;
}

PermissionCallback PermissionManager::GetPermissionCallback() const {
    return m_callback;
}

bool PermissionManager::IsAllowed(PermissionCategory category, const std::string& target,
                                  const std::string& operation) {
    if (!m_enabled) {
        return true; // If disabled, allow all
    }

    // Check explicit grants
    if (IsGranted(category, target)) {
        return true;
    }

    // Check explicit denials
    if (IsDenied(category, target)) {
        return false;
    }

    // Check default category permissions
    // File read is allowed by default
    if (category == PermissionCategory::FileRead) {
        return true;
    }

    return false;
}

PermissionResponse PermissionManager::RequestPermission(const PermissionRequest& request) {
    if (!m_enabled) {
        return PermissionResponse(request.id, PermissionAction::Allow);
    }

    // Check if already granted/denied
    if (IsGranted(request.category, request.target)) {
        return PermissionResponse(request.id, PermissionAction::AllowAlways);
    }

    if (IsDenied(request.category, request.target)) {
        return PermissionResponse(request.id, PermissionAction::Deny);
    }

    // Invoke callback for user decision
    PermissionAction action = m_callback(request);
    return PermissionResponse(request.id, action);
}

void PermissionManager::GrantPermission(PermissionCategory category, const std::string& target) {
    m_denied.erase(std::make_pair(category, target));
    m_granted.insert(std::make_pair(category, target));
}

void PermissionManager::DenyPermission(PermissionCategory category, const std::string& target) {
    m_granted.erase(std::make_pair(category, target));
    m_denied.insert(std::make_pair(category, target));
}

bool PermissionManager::IsGranted(PermissionCategory category, const std::string& target) const {
    return m_granted.find(std::make_pair(category, target)) != m_granted.end();
}

bool PermissionManager::IsDenied(PermissionCategory category, const std::string& target) const {
    return m_denied.find(std::make_pair(category, target)) != m_denied.end();
}

std::vector<std::pair<PermissionCategory, std::string>> PermissionManager::GetGrantedPermissions() const {
    std::vector<std::pair<PermissionCategory, std::string>> result;
    result.reserve(m_granted.size());
    for (const auto& pair : m_granted) {
        result.push_back(pair);
    }
    return result;
}

std::vector<std::pair<PermissionCategory, std::string>> PermissionManager::GetDeniedPermissions() const {
    std::vector<std::pair<PermissionCategory, std::string>> result;
    result.reserve(m_denied.size());
    for (const auto& pair : m_denied) {
        result.push_back(pair);
    }
    return result;
}

void PermissionManager::ClearPermissions() {
    m_granted.clear();
    m_denied.clear();
}

void PermissionManager::SetEnabled(bool enabled) {
    m_enabled = enabled;
}

bool PermissionManager::IsEnabled() const {
    return m_enabled;
}

// Tool Permission Checker Implementation
bool ToolPermissionChecker::CheckFileRead(const std::string& path) {
    if (!m_permManager) {
        return true; // No manager = allow
    }
    return m_permManager->IsAllowed(PermissionCategory::FileRead, path);
}

bool ToolPermissionChecker::CheckFileWrite(const std::string& path) {
    if (!m_permManager) {
        return true;
    }
    return m_permManager->IsAllowed(PermissionCategory::FileWrite, path);
}

bool ToolPermissionChecker::CheckFileDelete(const std::string& path) {
    if (!m_permManager) {
        return true;
    }
    return m_permManager->IsAllowed(PermissionCategory::FileDelete, path);
}

bool ToolPermissionChecker::CheckShellExecute(const std::string& command) {
    if (!m_permManager) {
        return true;
    }
    return m_permManager->IsAllowed(PermissionCategory::ShellExecute, command);
}

bool ToolPermissionChecker::CheckGitOperation(const std::string& operation) {
    if (!m_permManager) {
        return true;
    }
    return m_permManager->IsAllowed(PermissionCategory::GitOperation, operation);
}

bool ToolPermissionChecker::CheckNetworkRequest(const std::string& url) {
    if (!m_permManager) {
        return true;
    }
    return m_permManager->IsAllowed(PermissionCategory::NetworkRequest, url);
}

bool ToolPermissionChecker::CheckCodeModify(const std::string& filePath) {
    if (!m_permManager) {
        return true;
    }
    return m_permManager->IsAllowed(PermissionCategory::CodeModify, filePath);
}

bool ToolPermissionChecker::CheckBuildTest(const std::string& operation) {
    if (!m_permManager) {
        return true;
    }
    return m_permManager->IsAllowed(PermissionCategory::BuildTest, operation);
}

} // namespace zagent