#include "tools/ListFilesTool.hxx"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace zagent {

std::string ListFilesTool::Name() const {
    return "list_files";
}

std::string ListFilesTool::Description() const {
    return "List files and directories in a given path. Returns the contents of a directory.";
}

nlohmann::json ListFilesTool::Parameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "Directory path to list (default: current directory)"},
            }},
        }},
        {"required", {}},
    };
}

ToolResult ListFilesTool::Execute(const nlohmann::json& arguments) {
    std::string path = ".";
    if (arguments.contains("path") && arguments["path"].is_string()) {
        path = arguments["path"].get<std::string>();
    }

    namespace fs = std::filesystem;
    fs::path directoryPath(path);

    // --- Workspace security check ---
    if (m_workspace != nullptr) {
        if (!m_workspace->IsAllowed(directoryPath)) {
            return ToolResult{false, {},
                              "Access denied: " + m_workspace->GetError()};
        }
    }
    // --------------------------------

    if (!fs::exists(directoryPath)) {
        return ToolResult{false, {}, "Path does not exist: " + path};
    }

    if (!fs::is_directory(directoryPath)) {
        return ToolResult{false, {}, "Path is not a directory: " + path};
    }

    std::stringstream ss;
    ss << "Contents of " << directoryPath << ":\n\n";

    std::vector<std::string> entries;
    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        entries.push_back(entry.path().filename().string());
    }

    std::sort(entries.begin(), entries.end());

    for (const auto& entryName : entries) {
        ss << "  " << entryName << "\n";
    }

    return ToolResult{true, ss.str(), {}};
}

} // namespace zagent