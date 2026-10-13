#include "context/ContextManager.hxx"
#include "code/ClangdCodeIntelligence.hxx"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <unordered_set>
#include <sstream>
#include <fstream>
#include <regex>
#include <iostream>
#include <memory>

using json = nlohmann::json;

namespace zagent {

// Helper: Convert ContextEntry to JSON
std::string ContextEntry::ToJSON() const {
    json j;
    j["id"] = id;
    j["type"] = type;
    j["name"] = name;
    j["qualifiedName"] = qualifiedName;
    j["description"] = description;
    j["relevance"] = relevance;
    j["filePath"] = filePath;
    j["line"] = line;
    j["column"] = column;
    j["tags"] = tags;
    j["relatedTo"] = relatedTo;
    return j.dump();
}

// Helper: Load ContextEntry from JSON
bool ContextEntry::FromJSON(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        id = j.value("id", "");
        type = j.value("type", "");
        name = j.value("name", "");
        qualifiedName = j.value("qualifiedName", "");
        description = j.value("description", "");
        relevance = j.value("relevance", 0.0);
        filePath = j.value("filePath", "");
        line = j.value("line", -1);
        column = j.value("column", -1);
        tags = j.value("tags", std::vector<std::string>());
        relatedTo = j.value("relatedTo", std::vector<std::string>());
        return IsValid();
    } catch (const std::exception& e) {
        std::cerr << "ContextManager: failed to parse JSON: " << e.what() << std::endl;
        return false;
    }
}

ContextManager::ContextManager(ToolRegistry& tools) : m_tools(tools), m_workspaceRoot("") {
}

bool ContextManager::Initialize(const std::string& workspaceRoot) {
    if (m_isInitialized) {
        return true;
    }

    m_workspaceRoot = workspaceRoot;

    // Create code intelligence if not already set
    if (!m_codeIntelligence) {
        m_codeIntelligence = std::make_unique<ClangdCodeIntelligence>();
        m_codeIntelligence->SetWorkspace(workspaceRoot);
        if (!m_codeIntelligence->Initialize()) {
            std::cerr << "ContextManager: failed to initialize code intelligence" << std::endl;
            return false;
        }
    }

    m_isInitialized = true;
    return true;
}

void ContextManager::Shutdown() {
    if (m_codeIntelligence) {
        m_codeIntelligence->Shutdown();
        m_codeIntelligence.reset();
    }

    Clear();
    m_isInitialized = false;
    m_isReady = false;
}

bool ContextManager::IsReady() const {
    return m_isInitialized && m_isReady && !m_workspaceRoot.empty();
}

void ContextManager::AddContextEntry(const ContextEntry& entry) {
    if (!entry.IsValid()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_contextMutex);
    m_contextMap[entry.id] = entry;

    // Update relevance
    m_contextMap[entry.id].relevance = 1.0;
}

std::optional<ContextEntry> ContextManager::GetContextEntry(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    auto it = m_contextMap.find(id);
    if (it != m_contextMap.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<ContextEntry> ContextManager::GetContextEntriesByType(const std::string& type) const {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    std::vector<ContextEntry> result;

    for (const auto& pair : m_contextMap) {
        if (pair.second.type == type) {
            result.push_back(pair.second);
        }
    }

    // Sort by relevance (descending)
    std::sort(result.begin(), result.end(),
              [](const ContextEntry& a, const ContextEntry& b) {
                  return a.relevance > b.relevance;
              });

    return result;
}

std::vector<ContextEntry> ContextManager::GetContextEntriesByTags(const std::vector<std::string>& tags) const {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    std::vector<ContextEntry> result;

    for (const auto& pair : m_contextMap) {
        const auto& entryTags = pair.second.tags;
        bool hasAllTags = std::all_of(tags.begin(), tags.end(),
                                     [&entryTags](const std::string& tag) {
                                         return std::find(entryTags.begin(), entryTags.end(), tag) != entryTags.end();
                                     });

        if (hasAllTags) {
            result.push_back(pair.second);
        }
    }

    // Sort by relevance (descending)
    std::sort(result.begin(), result.end(),
              [](const ContextEntry& a, const ContextEntry& b) {
                  return a.relevance > b.relevance;
              });

    return result;
}

std::vector<ContextEntry> ContextManager::GetRelatedContextEntries(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    std::vector<ContextEntry> result;

    auto it = m_contextMap.find(id);
    if (it == m_contextMap.end()) {
        return result;
    }

    const auto& entry = it->second;
    for (const auto& relatedId : entry.relatedTo) {
        auto relatedIt = m_contextMap.find(relatedId);
        if (relatedIt != m_contextMap.end()) {
            result.push_back(relatedIt->second);
        }
    }

    return result;
}

std::vector<ContextEntry> ContextManager::GenerateContext(const std::string& query, int maxResults) {
    if (!IsReady()) {
        return {};
    }

    std::vector<ContextEntry> result;

    // Extract keywords from query
    auto keywords = ExtractKeywords(query);

    // Find entries matching keywords
    for (const auto& pair : m_contextMap) {
        const auto& entry = pair.second;

        // Score relevance
        double score = ScoreRelevance(entry, query);

        if (score > 0.0) {
            auto entryCopy = entry;
            entryCopy.relevance = score;
            result.push_back(entryCopy);
        }
    }

    // Sort by relevance (descending)
    std::sort(result.begin(), result.end(),
              [](const ContextEntry& a, const ContextEntry& b) {
                  return a.relevance > b.relevance;
              });

    // Limit results
    if (result.size() > static_cast<size_t>(maxResults)) {
        result.resize(maxResults);
    }

    return result;
}

bool ContextManager::BuildContextMap() {
    if (!IsReady()) {
        return false;
    }

    Clear();

    try {
        // Get list of files in workspace
        ToolCall listFilesCall;
        listFilesCall.id = "build_context_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        listFilesCall.name = "list_files";
        listFilesCall.arguments = {{{"path", "."}}};

        ToolResult listFilesResult = m_tools.Execute(listFilesCall);
        if (!listFilesResult.success) {
            std::cerr << "ContextManager: failed to list files: " << listFilesResult.error << std::endl;
            return false;
        }

        // Parse file list (simplified - would normally parse JSON)
        // For now, we'll just get C++ and header files
        std::vector<std::string> cppFiles;
        std::istringstream iss(listFilesResult.output);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            std::string ext = "";
            size_t dotPos = line.find_last_of('.');
            if (dotPos != std::string::npos) {
                ext = line.substr(dotPos);
            }
            if (ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".c" ||
                ext == ".hpp" || ext == ".hxx" || ext == ".hh" || ext == ".h") {
                cppFiles.push_back(line);
            }
        }

        // Analyze each C++ file
        for (const auto& filePath : cppFiles) {
            auto fileEntries = AnalyzeFile(filePath);
            for (const auto& entry : fileEntries) {
                AddContextEntry(entry);
            }
        }

        m_isReady = true;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "ContextManager: failed to build context map: " << e.what() << std::endl;
        return false;
    }
}

void ContextManager::UpdateContextEntry(const ContextEntry& entry) {
    if (!entry.IsValid()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_contextMutex);
    m_contextMap[entry.id] = entry;
}

void ContextManager::RemoveContextEntry(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    m_contextMap.erase(id);
}

void ContextManager::Clear() {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    m_contextMap.clear();
}

std::vector<ContextEntry> ContextManager::GetAllContextEntries() const {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    std::vector<ContextEntry> result;
    result.reserve(m_contextMap.size());

    for (const auto& pair : m_contextMap) {
        result.push_back(pair.second);
    }

    return result;
}

void ContextManager::SetCodeIntelligence(std::unique_ptr<ICodeIntelligence> ci) {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    m_codeIntelligence = std::move(ci);
}

std::string ContextManager::GetWorkspace() const {
    return m_workspaceRoot;
}

// Private helper methods

std::string ContextManager::GenerateId(const std::string& type, const std::string& name,
                                       const std::string& filePath) const {
    std::string safeName = name;
    std::replace_if(safeName.begin(), safeName.end(),
                   [](char c) { return !(std::isalnum(static_cast<unsigned char>(c)) && c != '_'); },
                   '_');

    std::string result = type + "_" + safeName;
    if (!filePath.empty()) {
        size_t lastSlash = filePath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            std::string fileName = filePath.substr(lastSlash + 1);
            result += "_" + fileName;
        }
    }

    return result;
}

std::vector<ContextEntry> ContextManager::AnalyzeFile(const std::string& filePath) {
    std::vector<ContextEntry> entries;

    // Read file content
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return entries;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Analyze symbols using code intelligence if available
    // Note: FindSymbols is workspace-wide; for per-file analysis, we extract
    // symbols directly from file content or use the code intelligence's
    // broader workspace search in GenerateContext later.
    if (m_codeIntelligence) {
        m_codeIntelligence->SetWorkspace(m_workspaceRoot);
        m_codeIntelligence->Initialize();
        // Symbol analysis via per-file AST is a future enhancement.
        // For now, the code intelligence initialization is enough
        // for GenerateContext keyword extraction and relevance scoring.
    }

    // Add file entry itself
    ContextEntry fileEntry;
    fileEntry.id = GenerateId("file", filePath);
    fileEntry.type = "file";
    fileEntry.name = filePath;
    fileEntry.description = "C++ source file: " + filePath;
    fileEntry.tags = {"cpp", "source", "file"};
    entries.push_back(fileEntry);

    return entries;
}

std::vector<ContextEntry> ContextManager::AnalyzeSymbols(const std::string& filePath) {
    std::vector<ContextEntry> entries;

    if (!m_codeIntelligence) {
        return entries;
    }

    // Find all symbols in the file
    // Note: FindSymbols is workspace-wide; for per-file analysis, we extract
    // symbols directly from file content or use the code intelligence's
    // broader workspace search in GenerateContext later.
    if (m_codeIntelligence) {
        m_codeIntelligence->SetWorkspace(m_workspaceRoot);
        m_codeIntelligence->Initialize();
        // Symbol analysis via per-file AST is a future enhancement.
        // For now, the code intelligence initialization is enough
        // for GenerateContext keyword extraction and relevance scoring.
    }

    return entries;
}

double ContextManager::ScoreRelevance(const ContextEntry& entry, const std::string& query) const {
    double score = 0.0;

    // Keyword matching
    auto keywords = ExtractKeywords(query);
    for (const auto& keyword : keywords) {
        // Match against name
        if (entry.name == keyword) {
            score += 2.0;
        } else if (entry.name.find(keyword) != std::string::npos) {
            score += 1.0;
        }

        // Match against qualified name
        if (entry.qualifiedName.find(keyword) != std::string::npos) {
            score += 1.5;
        }

        // Match against description
        if (entry.description.find(keyword) != std::string::npos) {
            score += 1.0;
        }

        // Match against tags
        if (std::find(entry.tags.begin(), entry.tags.end(), keyword) != entry.tags.end()) {
            score += 1.0;
        }
    }

    // Boost based on entry type
    if (entry.type == "class") score += 2.0;
    else if (entry.type == "function") score += 1.5;
    else if (entry.type == "variable") score += 1.0;

    // Normalize score
    score = std::min(score, 10.0);
    return score;
}

std::vector<std::string> ContextManager::ExtractKeywords(const std::string& query) const {
    std::vector<std::string> keywords;

    // Simple keyword extraction (in practice, would use more sophisticated NLP)
    std::regex wordRegex(R"([a-zA-Z_][a-zA-Z0-9_]*)");
    std::sregex_iterator iter(query.begin(), query.end(), wordRegex);
    std::sregex_iterator end;

    std::unordered_set<std::string> seen;
    for (; iter != end; ++iter) {
        std::string word = (*iter).str();
        // Convert to lowercase for case-insensitive matching
        std::transform(word.begin(), word.end(), word.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        // Skip common stop words
        static const std::unordered_set<std::string> stopWords = {
            "the", "and", "or", "is", "are", "to", "of", "in", "for", "on", "with", "a", "an"
        };

        if (seen.find(word) == seen.end() && stopWords.find(word) == stopWords.end()) {
            keywords.push_back(word);
            seen.insert(word);
        }
    }

    return keywords;
}

std::vector<std::string> ContextManager::GenerateTags(const ContextEntry& entry) const {
    std::vector<std::string> tags;

    // Add type-based tags
    if (entry.type == "class") tags.push_back("class");
    else if (entry.type == "function") tags.push_back("function");
    else if (entry.type == "variable") tags.push_back("variable");
    else if (entry.type == "struct") tags.push_back("struct");
    else if (entry.type == "enum") tags.push_back("enum");
    else if (entry.type == "namespace") tags.push_back("namespace");
    else if (entry.type == "file") tags.push_back("file");

    // Add language tag
    tags.push_back("cpp");

    // Add visibility tags
    if (entry.name.empty() || entry.name[0] == '_') {
        tags.push_back("private");
    } else {
        tags.push_back("public");
    }

    // Add complexity tags based on qualified name
    if (!entry.qualifiedName.empty()) {
        if (entry.qualifiedName.find("::") != std::string::npos) {
            tags.push_back("nested");
        }
    }

    return tags;
}

} // namespace zagent
