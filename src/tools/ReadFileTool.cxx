#include "tools/ReadFileTool.hxx"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace zagent {

namespace {

// Read entire file content into a string
std::string ReadFileContent(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace

std::string ReadFileTool::Name() const {
    return "read_file";
}

std::string ReadFileTool::Description() const {
    return "Read a text file and return its contents. Returns the full file content.";
}

nlohmann::json ReadFileTool::Parameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "Path to the file to read"},
            }},
        }},
        {"required", {"path"}},
    };
}

ToolResult ReadFileTool::Execute(const nlohmann::json& arguments) {
    if (!arguments.contains("path") || !arguments["path"].is_string()) {
        return ToolResult{false, {}, "Missing required parameter: path"};
    }

    const std::string path = arguments["path"].get<std::string>();

    namespace fs = std::filesystem;
    fs::path filePath(path);

    // --- Workspace security check ---
    if (m_workspace != nullptr) {
        if (!m_workspace->IsAllowed(filePath)) {
            return ToolResult{false, {},
                                "Access denied: " + m_workspace->GetError()};
        }
    }
    // --------------------------------

    if (!fs::exists(filePath)) {
        return ToolResult{false, {}, "File does not exist: " + path};
    }

    if (fs::is_directory(filePath)) {
        return ToolResult{false, {}, "Path is a directory, not a file: " + path};
    }

    const std::string content = ReadFileContent(path);
    if (content.empty()) {
        return ToolResult{false, {}, "File is empty or could not be read: " + path};
    }

    return ToolResult{true, content, {}};
}

} // namespace zagent