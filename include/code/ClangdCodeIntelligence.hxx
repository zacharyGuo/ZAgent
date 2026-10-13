#pragma once

#include "code/ICodeIntelligence.hxx"
#include "workspace/Workspace.hxx"

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <optional>

#ifdef _WIN32
#include <process.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace zagent {

/// Clangd-based implementation of C++ code intelligence.
///
/// This implementation uses clangd (the language server for C/C++ based on LLVM/Clang)
/// to provide semantic code intelligence including symbol lookup, references, and definitions.
///
/// It communicates with clangd via stdin/stdout pipes using the Language Server Protocol (LSP).
class ClangdCodeIntelligence : public ICodeIntelligence {
public:
    explicit ClangdCodeIntelligence();
    ~ClangdCodeIntelligence() override;

    // ICodeIntelligence interface
    std::vector<Symbol> FindSymbols(const std::string& name) override;
    std::vector<Location> FindReferences(const Symbol& symbol) override;
    Location FindDefinition(const Symbol& symbol) override;

    // Workspace management
    void SetWorkspace(const std::string& workspaceRoot) override;
    std::string GetWorkspace() const override;

    // Backend lifecycle
    bool IsReady() const override;
    bool Initialize() override;
    void Shutdown() override;

private:
    // LSP message handling
    struct LSPRequest {
        int id;
        std::string method;
        std::string jsonParams;
    };

    struct LSPResponse {
        int id;
        bool success;
        std::string result;
        std::string error;
    };

    struct LSPNotification {
        std::string method;
        std::string jsonParams;
    };

    // Start and manage clangd process
    bool StartClangdProcess();
    void StopClangdProcess();
    bool SendLSPMessage(const std::string& message);
    std::optional<std::string> ReadLSPMessage();

    // Parse LSP responses
    std::vector<Symbol> ParseSymbolsResponse(const std::string& json);
    std::vector<Location> ParseReferencesResponse(const std::string& json);
    Location ParseDefinitionResponse(const std::string& json);

    // Workspace and initialization state
    std::string m_workspaceRoot;
    bool m_isInitialized = false;
    bool m_isReady = false;

    // clangd process handle
#ifdef _WIN32
    void* m_clangdProcess = nullptr;
    void* m_clangdStdinWrite = nullptr;
    void* m_clangdStdoutRead = nullptr;
#else
    int m_clangdPid = -1;
    int m_clangdStdinWrite = -1;
    int m_clangdStdoutRead = -1;
#endif

    // Request tracking
    int m_nextRequestId = 1;
    std::map<int, std::string> m_pendingRequests; // id -> method

    // Symbol cache (simple in-memory cache for performance)
    std::map<std::string, std::vector<Symbol>> m_symbolCache;
    std::map<std::string, Location> m_definitionCache;
    std::map<std::string, std::vector<Location>> m_referenceCache;
};

} // namespace zagent
