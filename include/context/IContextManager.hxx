#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

#include "code/ICodeIntelligence.hxx"
#include "agent/Message.hxx"

namespace zagent {

/// Represents a repository context map entry.
struct ContextEntry {
    std::string id;                    // Unique identifier (e.g., "file:/path/to/file.cpp", "symbol:Foo")
    std::string type;                   // Type: "file", "symbol", "function", "class", "variable", etc.
    std::string name;                   // Human-readable name
    std::string qualifiedName;          // Fully qualified name if applicable
    std::string description;            // Description for LLM understanding
    double relevance;                   // Relevance score (0.0-1.0) for caching
    std::string filePath;               // Source file path (if applicable)
    int line;                           // Line number (if applicable)
    int column;                         // Column number (if applicable)
    std::vector<std::string> tags;      // Tags for categorization (e.g., "api", "internal", "template")
    std::vector<std::string> relatedTo; // Related context IDs

    ContextEntry() : relevance(0.0), line(-1), column(-1) {}

    // Check if this entry is valid
    bool IsValid() const {
        return !name.empty();
    }

    // Convert to JSON for LLM
    std::string ToJSON() const;

    // Load from JSON
    bool FromJSON(const std::string& json);
};

/// Abstract interface for repository context management.
class IContextManager {
public:
    virtual ~IContextManager() = default;

    // Initialize the context manager with a workspace
    virtual bool Initialize(const std::string& workspaceRoot) = 0;

    // Shutdown and cleanup
    virtual void Shutdown() = 0;

    // Get whether the context manager is ready
    virtual bool IsReady() const = 0;

    // Add a context entry to the context map
    virtual void AddContextEntry(const ContextEntry& entry) = 0;

    // Get context entry by ID
    virtual std::optional<ContextEntry> GetContextEntry(const std::string& id) const = 0;

    // Get all context entries of a specific type
    virtual std::vector<ContextEntry> GetContextEntriesByType(const std::string& type) const = 0;

    // Get context entries by tags
    virtual std::vector<ContextEntry> GetContextEntriesByTags(const std::vector<std::string>& tags) const = 0;

    // Get related context entries
    virtual std::vector<ContextEntry> GetRelatedContextEntries(const std::string& id) const = 0;

    // Generate context based on a user query/prompt
    virtual std::vector<ContextEntry> GenerateContext(const std::string& query, int maxResults = 10) = 0;

    // Build context map from workspace (if needed)
    virtual bool BuildContextMap() = 0;

    // Update context entry
    virtual void UpdateContextEntry(const ContextEntry& entry) = 0;

    // Remove context entry
    virtual void RemoveContextEntry(const std::string& id) = 0;

    // Clear all context entries
    virtual void Clear() = 0;

    // Get all context entries
    virtual std::vector<ContextEntry> GetAllContextEntries() const = 0;

    // Set the code intelligence backend for semantic queries
    virtual void SetCodeIntelligence(std::unique_ptr<ICodeIntelligence> ci) = 0;

    // Get current workspace
    virtual std::string GetWorkspace() const = 0;
};

} // namespace zagent
