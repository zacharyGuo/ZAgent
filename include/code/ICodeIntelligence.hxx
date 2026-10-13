#pragma once

#include <string>
#include <vector>

namespace zagent {

/// Represents a source code location (file + line + column).
struct Location {
    std::string uri;        // File URI (e.g., "file:///path/to/file.cpp")
    std::string filePath;   // Local file path
    int line;               // 1-based line number
    int column;             // 1-based column number

    Location() : line(-1), column(-1) {}

    Location(std::string uri, std::string filePath, int line, int column)
        : uri(std::move(uri)), filePath(std::move(filePath)), line(line), column(column) {}

    // Convert location to a human-readable string
    std::string ToString() const {
        return filePath + ":" + std::to_string(line) + ":" + std::to_string(column);
    }

    bool operator==(const Location& other) const {
        return filePath == other.filePath && line == other.line && column == other.column;
    }

    bool IsValid() const {
        return !filePath.empty() && line > 0 && column > 0;
    }
};

/// Represents a code symbol (class, function, variable, etc.).
struct Symbol {
    std::string name;             // Symbol name (e.g., "Foo", "bar()")
    std::string kind;             // Symbol kind (e.g., "class", "function", "variable")
    std::string qualifiedName;    // Fully qualified name if applicable
    Location definition;          // Location where the symbol is defined
    Location container;           // Container of the symbol (e.g., enclosing class)

    Symbol() = default;

    Symbol(std::string name, std::string kind)
        : name(std::move(name)), kind(std::move(kind)) {}

    // Convert symbol to a human-readable string
    std::string ToString() const {
        std::string result = name;
        if (!kind.empty()) {
            result += " [" + kind + "]";
        }
        if (!qualifiedName.empty()) {
            result += " :: " + qualifiedName;
        }
        if (definition.IsValid()) {
            result += " @ " + definition.ToString();
        }
        return result;
    }

    bool operator==(const Symbol& other) const {
        return name == other.name && kind == other.kind && qualifiedName == other.qualifiedName;
    }

    bool IsValid() const {
        return !name.empty();
    }
};

/// Abstract interface for C++ code intelligence.
///
/// Provides semantic understanding of C++ code using clangd/LSP backend.
/// This is more powerful than simple grep-based search - it understands
/// the actual AST and symbol relationships.
class ICodeIntelligence {
public:
    virtual ~ICodeIntelligence() = default;

    // Find all symbols matching the given name.
    // @param name The symbol name to search for
    // @return Vector of matching symbols
    virtual std::vector<Symbol> FindSymbols(const std::string& name) = 0;

    // Find all references to the given symbol.
    // @param symbol The symbol to find references for
    // @return Vector of locations where the symbol is referenced
    virtual std::vector<Location> FindReferences(const Symbol& symbol) = 0;

    // Find the definition location of the given symbol.
    // @param symbol The symbol to find the definition for
    // @return Location of the definition, or empty if not found
    virtual Location FindDefinition(const Symbol& symbol) = 0;

    // Set the workspace root for the code intelligence backend.
    // @param workspaceRoot The absolute path to the workspace root
    virtual void SetWorkspace(const std::string& workspaceRoot) = 0;

    // Get the current workspace root.
    virtual std::string GetWorkspace() const = 0;

    // Check if the backend is ready/initialized.
    virtual bool IsReady() const = 0;

    // Initialize the backend (e.g., start clangd process).
    // @return true if initialization succeeded
    virtual bool Initialize() = 0;

    // Shutdown the backend.
    virtual void Shutdown() = 0;
};

} // namespace zagent
