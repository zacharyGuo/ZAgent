#include "tools/ApplyPatchTool.hxx"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace zagent {

namespace {

// Read entire file content
std::string ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Write content to a file
bool WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file << content;
    return file.good();
}

// Generate a simple unified diff
std::string GenerateDiff(const std::string& filePath,
                         const std::string& oldContent,
                         const std::string& newContent) {
    std::stringstream diff;
    diff << "--- " << filePath << " (original)\n";
    diff << "+++ " << filePath << " (modified)\n";

    // Simple line-by-line diff
    std::istringstream oldStream(oldContent);
    std::istringstream newStream(newContent);
    std::string oldLine, newLine;
    int oldLineNum = 0, newLineNum = 0;

    diff << "@@ -1," << oldContent.size() << " +1," << newContent.size() << " @@\n";

    while (std::getline(oldStream, oldLine) || std::getline(newStream, newLine)) {
        ++oldLineNum;
        ++newLineNum;
        if (oldLine != newLine) {
            if (!oldLine.empty()) {
                diff << "- " << oldLine << "\n";
            }
            if (!newLine.empty()) {
                diff << "+ " << newLine << "\n";
            }
        }
    }

    return diff.str();
}

} // namespace

std::string ApplyPatchTool::Name() const {
    return "apply_patch";
}

std::string ApplyPatchTool::Description() const {
    return "Apply a text patch to a file by replacing old_text with new_text. The old_text must exist exactly in the file.";
}

nlohmann::json ApplyPatchTool::Parameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "Path to the file to modify"},
            }},
            {"old_text", {
                {"type", "string"},
                {"description", "The text to find in the file (must match exactly)"},
            }},
            {"new_text", {
                {"type", "string"},
                {"description", "The replacement text to substitute"},
            }},
        }},
        {"required", {"path", "old_text", "new_text"}},
    };
}

ToolResult ApplyPatchTool::Execute(const nlohmann::json& arguments) {
    if (!arguments.contains("path") || !arguments["path"].is_string()) {
        return ToolResult{false, {}, "Missing required parameter: path"};
    }
    if (!arguments.contains("old_text") || !arguments["old_text"].is_string()) {
        return ToolResult{false, {}, "Missing required parameter: old_text"};
    }
    if (!arguments.contains("new_text") || !arguments["new_text"].is_string()) {
        return ToolResult{false, {}, "Missing required parameter: new_text"};
    }

    const std::string filePath = arguments["path"].get<std::string>();
    const std::string oldText = arguments["old_text"].get<std::string>();
    const std::string newText = arguments["new_text"].get<std::string>();

    namespace fs = std::filesystem;
    fs::path pathObj(filePath);

    // --- Workspace security check ---
    if (m_workspace != nullptr) {
        if (!m_workspace->IsAllowed(pathObj)) {
            return ToolResult{false, {},
                                "Access denied: " + m_workspace->GetError()};
        }
    }
    // --------------------------------

    if (!fs::exists(pathObj)) {
        return ToolResult{false, {}, "File does not exist: " + filePath};
    }

    if (fs::is_directory(pathObj)) {
        return ToolResult{false, {}, "Path is a directory, not a file: " + filePath};
    }

    const std::string content = ReadFile(filePath);
    if (content.empty()) {
        return ToolResult{false, {}, "File is empty or could not be read: " + filePath};
    }

    // Find the old_text in the file content
    const auto pos = content.find(oldText);
    if (pos == std::string::npos) {
        return ToolResult{
            false, {},
            "Could not find the specified text in the file. The old_text must match exactly."
        };
    }

    // Replace old_text with new_text
    std::string newContent = content.substr(0, pos) + newText +
                             content.substr(pos + oldText.size());

    if (!WriteFile(filePath, newContent)) {
        return ToolResult{false, {}, "Failed to write to file: " + filePath};
    }

    std::stringstream ss;
    ss << "Successfully patched: " << filePath << "\n\n";
    ss << "--- Old (" << oldText.size() << " chars)\n";
    ss << "+++ New (" << newText.size() << " chars)\n";
    ss << "Patch applied successfully.\n";

    return ToolResult{true, ss.str(), {}};
}

} // namespace zagent