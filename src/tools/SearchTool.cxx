#include "tools/SearchTool.hxx"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace zagent {

namespace {

// Simple file search implementation
// In the future, this could be replaced with ripgrep or a native search engine
struct SearchResult {
    std::string filePath;
    std::size_t lineNumber;
    std::string line;
};

std::vector<SearchResult> SearchInFile(
    const std::filesystem::path& filePath,
    const std::string& pattern,
    bool caseSensitive) {
    std::vector<SearchResult> results;

    std::ifstream file(filePath);
    if (!file) {
        return results;
    }

    std::string line;
    std::size_t lineNumber = 0;
    std::string searchPattern = pattern;
    if (!caseSensitive) {
        std::transform(searchPattern.begin(), searchPattern.end(),
                       searchPattern.begin(), ::tolower);
    }

    while (std::getline(file, line)) {
        ++lineNumber;
        std::string haystack = line;
        if (!caseSensitive) {
            std::transform(haystack.begin(), haystack.end(),
                           haystack.begin(), ::tolower);
        }

        if (haystack.find(searchPattern) != std::string::npos) {
            results.push_back({filePath.string(), lineNumber, line});
        }
    }

    return results;
}

} // namespace

std::string SearchTool::Name() const {
    return "search";
}

std::string SearchTool::Description() const {
    return "Search for a text pattern in files within a directory. Returns matching lines with file paths and line numbers.";
}

nlohmann::json SearchTool::Parameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"pattern", {
                {"type", "string"},
                {"description", "Text pattern to search for"},
            }},
            {"path", {
                {"type", "string"},
                {"description", "Directory path to search in (default: current directory)"},
            }},
            {"case_sensitive", {
                {"type", "boolean"},
                {"description", "Whether search is case sensitive (default: false)"},
            }},
        }},
        {"required", {"pattern"}},
    };
}

ToolResult SearchTool::Execute(const nlohmann::json& arguments) {
    if (!arguments.contains("pattern") || !arguments["pattern"].is_string()) {
        return ToolResult{false, {}, "Missing required parameter: pattern"};
    }

    const std::string pattern = arguments["pattern"].get<std::string>();
    std::string path = ".";
    if (arguments.contains("path") && arguments["path"].is_string()) {
        path = arguments["path"].get<std::string>();
    }
    bool caseSensitive = false;
    if (arguments.contains("case_sensitive") && arguments["case_sensitive"].is_boolean()) {
        caseSensitive = arguments["case_sensitive"].get<bool>();
    }

    namespace fs = std::filesystem;
    fs::path searchPath(path);

    // --- Workspace security check ---
    if (m_workspace != nullptr) {
        if (!m_workspace->IsAllowed(searchPath)) {
            return ToolResult{false, {},
                                "Access denied: " + m_workspace->GetError()};
        }
    }
    // --------------------------------

    if (!fs::exists(searchPath)) {
        return ToolResult{false, {}, "Path does not exist: " + path};
    }

    if (!fs::is_directory(searchPath)) {
        return ToolResult{false, {}, "Path is not a directory: " + path};
    }

    std::vector<SearchResult> allResults;

    // Recursively search files
    for (const auto& entry : fs::recursive_directory_iterator(searchPath)) {
        if (entry.is_regular_file()) {
            // Skip binary files and common non-text files
            const auto ext = entry.path().extension().string();
            if (ext == ".exe" || ext == ".dll" || ext == ".so" || ext == ".dylib" ||
                ext == ".o" || ext == ".obj" || ext == ".a" || ext == ".lib" ||
                ext == ".pdf" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                ext == ".gif" || ext == ".ico" || ext == ".zip" || ext == ".tar" ||
                ext == ".gz" || ext == ".7z" || ext == ".bin" || ext == ".dat" ||
                ext == ".class" || ext == ".jar" || ext == ".war" || ext == ".ear" ||
                ext == ".pyc" || ext == ".pyo" || ext == ".pyd" || ext == ".lock") {
                continue;
            }

            auto results = SearchInFile(entry.path(), pattern, caseSensitive);
            allResults.insert(allResults.end(), results.begin(), results.end());
        }
    }

    if (allResults.empty()) {
        return ToolResult{true, "No matches found for pattern: " + pattern, {}};
    }

    std::stringstream ss;
    ss << "Found " << allResults.size() << " match(es) for pattern: " << pattern << "\n\n";

    // Group by file for better readability
    std::string currentFile;
    for (const auto& result : allResults) {
        if (result.filePath != currentFile) {
            currentFile = result.filePath;
            ss << "=== " << currentFile << " ===\n";
        }
        ss << "  Line " << result.lineNumber << ": " << result.line << "\n";
    }

    return ToolResult{true, ss.str(), {}};
}

} // namespace zagent