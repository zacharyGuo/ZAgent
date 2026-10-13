#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <regex>

namespace zagent {

class Workspace {
public:
    explicit Workspace(const std::string& root);

    const std::filesystem::path& Root() const;

    bool IsAllowed(const std::filesystem::path& path) const;

    std::filesystem::path Resolve(const std::filesystem::path& path) const;

    std::string GetError() const;

private:
    std::filesystem::path m_root;
    mutable std::string m_error;

    static const std::vector<std::regex> kDangerousPatterns;

    bool IsPathInDangerousZone(const std::filesystem::path& path) const;
    std::filesystem::path Normalize(const std::filesystem::path& path) const;
};

} // namespace zagent
