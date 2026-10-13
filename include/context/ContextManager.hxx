#pragma once

#include "IContextManager.hxx"
#include "code/ICodeIntelligence.hxx"
#include "tools/ToolRegistry.hxx"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <optional>

namespace zagent {

/// Concrete implementation of repository context management.
///
/// Maintains a context map of repository symbols, files, and relationships
/// to provide better context to the LLM for understanding the codebase.
class ContextManager : public IContextManager {
public:
    explicit ContextManager(ToolRegistry& tools);
    ~ContextManager() override = default;

    // IContextManager interface
    bool Initialize(const std::string& workspaceRoot) override;
    void Shutdown() override;
    bool IsReady() const override;

    void AddContextEntry(const ContextEntry& entry) override;
    std::optional<ContextEntry> GetContextEntry(const std::string& id) const override;
    std::vector<ContextEntry> GetContextEntriesByType(const std::string& type) const override;
    std::vector<ContextEntry> GetContextEntriesByTags(const std::vector<std::string>& tags) const override;
    std::vector<ContextEntry> GetRelatedContextEntries(const std::string& id) const override;
    std::vector<ContextEntry> GenerateContext(const std::string& query, int maxResults = 10) override;
    bool BuildContextMap() override;
    void UpdateContextEntry(const ContextEntry& entry) override;
    void RemoveContextEntry(const std::string& id) override;
    void Clear() override;
    std::vector<ContextEntry> GetAllContextEntries() const override;
    void SetCodeIntelligence(std::unique_ptr<ICodeIntelligence> ci) override;
    std::string GetWorkspace() const override;

private:
    // Context storage
    std::unordered_map<std::string, ContextEntry> m_contextMap;
    mutable std::mutex m_contextMutex;

    // Tool registry for repository operations
    ToolRegistry& m_tools;

    // Code intelligence for semantic understanding
    std::unique_ptr<ICodeIntelligence> m_codeIntelligence;

    // Workspace root
    std::string m_workspaceRoot;

    // State
    bool m_isInitialized = false;
    bool m_isReady = false;

    // Generate unique ID for context entries
    std::string GenerateId(const std::string& type, const std::string& name, const std::string& filePath = "") const;

    // Analyze a file and extract context entries
    std::vector<ContextEntry> AnalyzeFile(const std::string& filePath);

    // Analyze symbols in a file using code intelligence
    std::vector<ContextEntry> AnalyzeSymbols(const std::string& filePath);

    // Score relevance of context entry for a query
    double ScoreRelevance(const ContextEntry& entry, const std::string& query) const;

    // Extract keywords from query for matching
    std::vector<std::string> ExtractKeywords(const std::string& query) const;

    // Tag entry based on its characteristics
    std::vector<std::string> GenerateTags(const ContextEntry& entry) const;
};

} // namespace zagent