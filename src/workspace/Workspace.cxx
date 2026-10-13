#include "workspace/Workspace.hxx"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <unordered_set>
#include <cctype>

namespace zagent {

Workspace::Workspace(const std::string& root)
    : m_root(root) {
    // Ensure the workspace root is absolute
    if (!m_root.is_absolute()) {
        m_root = std::filesystem::current_path() / m_root;
    }
    // Normalize the path to remove any ".." or "." components
    m_root = std::filesystem::weakly_canonical(m_root);
}

const std::filesystem::path& Workspace::Root() const {
    return m_root;
}

bool Workspace::IsAllowed(const std::filesystem::path& path) const {
    m_error.clear();

    // Handle relative paths by resolving them relative to workspace root
    std::filesystem::path abs_path = path;
    if (!path.is_absolute()) {
        abs_path = m_root / path;
    }

    // Normalize to resolve any ".." or "." components and symlinks
    std::error_code ec;
    abs_path = std::filesystem::weakly_canonical(abs_path, ec);
    if (ec) {
        m_error = "Cannot resolve path: " + path.string() + " - " + ec.message();
        return false;
    }

    // Check if path is within workspace bounds
    try {
        // Check if abs_path starts with m_root
        auto rel_path = std::filesystem::relative(abs_path, m_root);
        // If relative path starts with "..", it's outside the workspace
        if (!rel_path.empty() && rel_path.native().find(L"..") == 0) {
            m_error = "Path is outside workspace: " + path.string();
            return false;
        }
    } catch (const std::filesystem::filesystem_error&) {
        // If relative_path throws, the paths have no common base
        m_error = "Path is outside workspace: " + path.string();
        return false;
    }

    // Check for dangerous patterns (platform-independent)
    std::string path_str = abs_path.generic_string(); // Use forward slashes for pattern matching
    std::transform(path_str.begin(), path_str.end(), path_str.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Check for common dangerous paths (simplified patterns)
    const std::vector<std::string> dangerous_substrings = {
        "/.ssh/", "/.git/", "/etc/passwd", "/etc/shadow",
        "/proc/", "/sys/", "/dev/", "/var/log/", "/root/",
        "/appdata/local/", "\\\\.ssh\\\\", "\\\\.git\\\\",
        "windows\\\\system32", "program files", "users\\\\"
    };

    for (const auto& dangerous : dangerous_substrings) {
        if (path_str.find(dangerous) != std::string::npos) {
            m_error = "Access to dangerous path blocked: " + path.string();
            return false;
        }
    }

    // Additional Windows-specific checks
#ifdef _WIN32
    // Check for drive access outside workspace
    if (abs_path.has_root_name() && m_root.has_root_name()) {
        if (abs_path.root_name() != m_root.root_name()) {
            m_error = "Access to different drive blocked: " + path.string();
            return false;
        }
    }
    // Check for common Windows dangerous paths
    const std::vector<std::string> win_dangerous = {
        "\\\\.ssh\\\\", "\\\\.git\\\\", "\\\\appdata\\\\local\\\\",
        "\\\\program files\\\\", "\\\\windows\\\\system32\\\\",
        "\\\\users\\\\", "\\\\documents and settings\\\\"
    };
    for (const auto& dangerous : win_dangerous) {
        if (path_str.find(dangerous) != std::string::npos) {
            m_error = "Access to dangerous Windows path blocked: " + path.string();
            return false;
        }
    }
#endif

    return true;
}

std::filesystem::path Workspace::Resolve(const std::filesystem::path& path) const {
    if (path.is_absolute()) {
        return path;
    }
    return m_root / path;
}

std::string Workspace::GetError() const {
    return m_error;
}

} // namespace zagent
