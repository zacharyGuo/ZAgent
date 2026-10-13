#include "code/ClangdCodeIntelligence.hxx"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#include <handleapi.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif

namespace zagent {

// Cross-platform process ID
inline int GetCurrentProcessId() {
#ifdef _WIN32
    return GetCurrentProcessId(); // Windows API
#else
    return getpid(); // POSIX
#endif
}

using json = nlohmann::json;

// Helper: Create JSON LSP request
static std::string MakeLSPRequest(int id, const std::string& method, const json& params) {
    json req;
    req["jsonrpc"] = "2.0";
    req["id"] = id;
    req["method"] = method;
    req["params"] = params;
    return req.dump();
}

// Helper: Create JSON LSP notification (no response expected)
static std::string MakeLSPNotification(const std::string& method, const json& params) {
    json req;
    req["jsonrpc"] = "2.0";
    req["method"] = method;
    req["params"] = params;
    return req.dump();
}

// Helper: Extract Location from LSP Location object
static Location LSPLocationToLocation(const json& loc) {
    if (!loc.contains("uri") || !loc.contains("range")) {
        return Location();
    }

    std::string uri = loc["uri"].get<std::string>();
    std::string filePath = uri;
    if (filePath.rfind("file://", 0) == 0) {
        filePath = filePath.substr(7); // Remove "file://" prefix
    }

    // Handle Windows paths from LSP (e.g., "/C:/path" -> "C:/path")
    if (!filePath.empty() && filePath[0] == '/' && filePath.size() > 2 && filePath[2] == ':') {
        filePath = filePath.substr(1);
    }

    int line = loc["range"]["start"]["line"].get<int>() + 1; // LSP is 0-based
    int column = loc["range"]["start"]["character"].get<int>() + 1; // LSP is 0-based

    return Location(uri, filePath, line, column);
}

// Forward declaration
static std::string SymbolKindToString(int kind);

// Helper: Extract Symbol from LSP SymbolInformation
static Symbol LSPSymbolToSymbol(const json& sym) {
    Symbol result;
    result.name = sym.value("name", "");

    // Convert numeric SymbolKind to string
    int kindNum = sym.value("kind", 0);
    result.kind = SymbolKindToString(kindNum);
    result.qualifiedName = sym.value("containerName", "");

    if (sym.contains("location")) {
        result.definition = LSPLocationToLocation(sym["location"]);
    }

    return result;
}

// Convert SymbolKind enum to string
static std::string SymbolKindToString(int kind) {
    switch (kind) {
        case 1: return "File";
        case 2: return "Module";
        case 3: return "Namespace";
        case 4: return "Package";
        case 5: return "Class";
        case 6: return "Method";
        case 7: return "Property";
        case 8: return "Field";
        case 9: return "Constructor";
        case 10: return "Enum";
        case 11: return "Interface";
        case 12: return "Function";
        case 13: return "Variable";
        case 14: return "Constant";
        case 15: return "String";
        case 16: return "Number";
        case 17: return "Boolean";
        case 18: return "Array";
        case 19: return "Object";
        case 20: return "Key";
        case 21: return "Null";
        case 22: return "EnumMember";
        case 23: return "Struct";
        case 24: return "Event";
        case 25: return "Operator";
        case 26: return "TypeParameter";
        default: return "Unknown";
    }
}

ClangdCodeIntelligence::ClangdCodeIntelligence() = default;

ClangdCodeIntelligence::~ClangdCodeIntelligence() {
    Shutdown();
}

void ClangdCodeIntelligence::SetWorkspace(const std::string& workspaceRoot) {
    m_workspaceRoot = workspaceRoot;
}

std::string ClangdCodeIntelligence::GetWorkspace() const {
    return m_workspaceRoot;
}

bool ClangdCodeIntelligence::IsReady() const {
    return m_isReady && m_isInitialized;
}

bool ClangdCodeIntelligence::Initialize() {
    if (m_isInitialized) {
        return true;
    }

    if (m_workspaceRoot.empty()) {
        std::cerr << "ClangdCodeIntelligence: workspace root not set" << std::endl;
        return false;
    }

    // Start clangd process
    if (!StartClangdProcess()) {
        std::cerr << "ClangdCodeIntelligence: failed to start clangd process" << std::endl;
        return false;
    }

    // Send initialize request
    json initParams;
    initParams["processId"] = GetCurrentProcessId();
    initParams["rootUri"] = "file://" + m_workspaceRoot;
    initParams["capabilities"] = json::object(); // Client capabilities

    std::string initRequest = MakeLSPRequest(m_nextRequestId++, "initialize", initParams);
    if (!SendLSPMessage(initRequest)) {
        std::cerr << "ClangdCodeIntelligence: failed to send initialize request" << std::endl;
        StopClangdProcess();
        return false;
    }

    // Read initialize response
    auto initResponse = ReadLSPMessage();
    if (!initResponse.has_value()) {
        std::cerr << "ClangdCodeIntelligence: no initialize response from clangd" << std::endl;
        StopClangdProcess();
        return false;
    }

    // Parse initialize response
    try {
        json response = json::parse(*initResponse);
        if (response.contains("error")) {
            std::cerr << "ClangdCodeIntelligence: initialize error: " << response["error"].dump() << std::endl;
            StopClangdProcess();
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "ClangdCodeIntelligence: failed to parse initialize response: " << e.what() << std::endl;
        StopClangdProcess();
        return false;
    }

    // Send initialized notification
    std::string initializedNotif = MakeLSPNotification("initialized", json::object());
    if (!SendLSPMessage(initializedNotif)) {
        std::cerr << "ClangdCodeIntelligence: failed to send initialized notification" << std::endl;
        StopClangdProcess();
        return false;
    }

    // Wait a bit for clangd to index the workspace
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    m_isInitialized = true;
    m_isReady = true;
    return true;
}

void ClangdCodeIntelligence::Shutdown() {
    if (m_isInitialized) {
        // Send shutdown request
        std::string shutdownRequest = MakeLSPRequest(m_nextRequestId++, "shutdown", json::object());
        SendLSPMessage(shutdownRequest);

        // Read shutdown response
        ReadLSPMessage();

        // Send exit notification
        std::string exitNotif = MakeLSPNotification("exit", json::object());
        SendLSPMessage(exitNotif);
    }

    StopClangdProcess();
    m_isInitialized = false;
    m_isReady = false;
}

bool ClangdCodeIntelligence::StartClangdProcess() {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdinRead, stdinWrite;
    HANDLE stdoutRead, stdoutWrite;

    if (!CreatePipe(&stdinRead, &stdinWrite, &sa, 0)) {
        std::cerr << "Failed to create stdin pipe" << std::endl;
        return false;
    }
    if (!CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0)) {
        std::cerr << "Failed to create stdout pipe" << std::endl;
        CloseHandle(stdinRead);
        CloseHandle(stdinWrite);
        return false;
    }

    // Make child handles non-inheritable
    SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = { 0 };
    std::string clangdCmd = "clangd";

    if (!CreateProcessA(
            nullptr,
            const_cast<char*>(clangdCmd.c_str()),
            nullptr, nullptr,
            TRUE, // inherit handles
            0, nullptr, m_workspaceRoot.c_str(),
            &si, &pi)) {
        std::cerr << "Failed to start clangd process. Is clangd in PATH?" << std::endl;
        CloseHandle(stdinRead);
        CloseHandle(stdinWrite);
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);
        return false;
    }

    // Close unused handles
    CloseHandle(stdinRead);
    CloseHandle(stdoutWrite);
    CloseHandle(pi.hThread);

    m_clangdProcess = pi.hProcess;
    m_clangdStdinWrite = stdinWrite;
    m_clangdStdoutRead = stdoutRead;
#else
    int stdinPipe[2];
    int stdoutPipe[2];

    if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0) {
        std::cerr << "Failed to create pipes" << std::endl;
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "Failed to fork process" << std::endl;
        return false;
    }

    if (pid == 0) {
        // Child process
        close(stdinPipe[1]); // Close write end of stdin pipe
        close(stdoutPipe[0]); // Close read end of stdout pipe

        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);

        close(stdinPipe[0]);
        close(stdoutPipe[1]);

        // Change to workspace directory
        if (chdir(m_workspaceRoot.c_str()) != 0) {
            _exit(1);
        }

        // Execute clangd
        execlp("clangd", "clangd", nullptr);
        _exit(1);
    }

    // Parent process
    close(stdinPipe[0]); // Close read end of stdin pipe
    close(stdoutPipe[1]); // Close write end of stdout pipe

    m_clangdPid = pid;
    m_clangdStdinWrite = stdinPipe[1];
    m_clangdStdoutRead = stdoutPipe[0];
#endif

    return true;
}

void ClangdCodeIntelligence::StopClangdProcess() {
#ifdef _WIN32
    if (m_clangdProcess) {
        TerminateProcess(m_clangdProcess, 0);
        WaitForSingleObject(m_clangdProcess, 5000);
        CloseHandle(m_clangdProcess);
        m_clangdProcess = nullptr;
    }
    if (m_clangdStdinWrite) {
        CloseHandle(m_clangdStdinWrite);
        m_clangdStdinWrite = nullptr;
    }
    if (m_clangdStdoutRead) {
        CloseHandle(m_clangdStdoutRead);
        m_clangdStdoutRead = nullptr;
    }
#else
    if (m_clangdPid > 0) {
        kill(m_clangdPid, SIGTERM);
        int status;
        waitpid(m_clangdPid, &status, 0);
        m_clangdPid = -1;
    }
    if (m_clangdStdinWrite >= 0) {
        close(m_clangdStdinWrite);
        m_clangdStdinWrite = -1;
    }
    if (m_clangdStdoutRead >= 0) {
        close(m_clangdStdoutRead);
        m_clangdStdoutRead = -1;
    }
#endif
}

bool ClangdCodeIntelligence::SendLSPMessage(const std::string& message) {
#ifdef _WIN32
    if (!m_clangdStdinWrite) return false;

    std::string fullMessage = "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n" + message;
    DWORD written;
    return WriteFile(m_clangdStdinWrite, fullMessage.c_str(), static_cast<DWORD>(fullMessage.size()), &written, nullptr);
#else
    if (m_clangdStdinWrite < 0) return false;

    std::string fullMessage = "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n" + message;
    ssize_t written = write(m_clangdStdinWrite, fullMessage.c_str(), fullMessage.size());
    return written == static_cast<ssize_t>(fullMessage.size());
#endif
}

std::optional<std::string> ClangdCodeIntelligence::ReadLSPMessage() {
    const int maxAttempts = 100;
    const int delayMs = 50;

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
#ifdef _WIN32
        if (!m_clangdStdoutRead) return std::nullopt;

        // Read header
        char header[4096];
        DWORD totalRead = 0;
        bool headerComplete = false;

        while (!headerComplete && totalRead < sizeof(header) - 1) {
            DWORD bytesRead = 0;
            if (!ReadFile(m_clangdStdoutRead, header + totalRead, 1, &bytesRead, nullptr) || bytesRead == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
                break;
            }
            totalRead += bytesRead;
            header[totalRead] = '\0';

            if (totalRead >= 4 && memcmp(header + totalRead - 4, "\r\n\r\n", 4) == 0) {
                headerComplete = true;
            }
        }

        if (!headerComplete) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            continue;
        }

        // Parse Content-Length
        std::string headerStr(header);
        size_t contentLengthPos = headerStr.find("Content-Length:");
        if (contentLengthPos == std::string::npos) continue;

        size_t colonPos = headerStr.find(':', contentLengthPos);
        size_t endPos = headerStr.find('\r', colonPos);
        std::string lengthStr = headerStr.substr(colonPos + 1, endPos - colonPos - 1);
        int contentLength = std::stoi(lengthStr);

        // Read body
        std::string body;
        body.resize(contentLength);
        DWORD bodyRead = 0;
        while (bodyRead < static_cast<DWORD>(contentLength)) {
            DWORD bytesRead = 0;
            if (!ReadFile(m_clangdStdoutRead, &body[bodyRead], contentLength - bodyRead, &bytesRead, nullptr) || bytesRead == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
                break;
            }
            bodyRead += bytesRead;
        }

        if (bodyRead == static_cast<DWORD>(contentLength)) {
            return body;
        }
#else
        if (m_clangdStdoutRead < 0) return std::nullopt;

        // Read header
        std::string header;
        char ch;
        while (header.size() < 4096) {
            ssize_t n = read(m_clangdStdoutRead, &ch, 1);
            if (n <= 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
                break;
            }
            header += ch;
            if (header.size() >= 4 && header.substr(header.size() - 4) == "\r\n\r\n") {
                break;
            }
        }

        if (header.size() < 4 || header.substr(header.size() - 4) != "\r\n\r\n") {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            continue;
        }

        // Parse Content-Length
        size_t contentLengthPos = header.find("Content-Length:");
        if (contentLengthPos == std::string::npos) continue;

        size_t colonPos = header.find(':', contentLengthPos);
        size_t endPos = header.find('\r', colonPos);
        std::string lengthStr = header.substr(colonPos + 1, endPos - colonPos - 1);
        int contentLength = std::stoi(lengthStr);

        // Read body
        std::string body;
        body.resize(contentLength);
        ssize_t bodyRead = 0;
        while (bodyRead < contentLength) {
            ssize_t n = read(m_clangdStdoutRead, &body[bodyRead], contentLength - bodyRead);
            if (n <= 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
                break;
            }
            bodyRead += n;
        }

        if (bodyRead == contentLength) {
            return body;
        }
#endif
    }

    return std::nullopt;
}

std::vector<Symbol> ClangdCodeIntelligence::FindSymbols(const std::string& name) {
    if (!IsReady() || m_workspaceRoot.empty()) {
        return {};
    }

    // Check cache first
    auto it = m_symbolCache.find(name);
    if (it != m_symbolCache.end()) {
        return it->second;
    }

    // Send workspace/symbol request
    json params;
    params["query"] = name;

    int requestId = m_nextRequestId++;
    std::string request = MakeLSPRequest(requestId, "workspace/symbol", params);
    m_pendingRequests[requestId] = "workspace/symbol";

    if (!SendLSPMessage(request)) {
        return {};
    }

    auto response = ReadLSPMessage();
    if (!response.has_value()) {
        return {};
    }

    std::vector<Symbol> symbols = ParseSymbolsResponse(*response);
    m_symbolCache[name] = symbols;
    return symbols;
}

std::vector<Location> ClangdCodeIntelligence::FindReferences(const Symbol& symbol) {
    if (!IsReady() || !symbol.definition.IsValid()) {
        return {};
    }

    std::string cacheKey = symbol.name + "@" + symbol.definition.filePath + ":" + std::to_string(symbol.definition.line);

    // Check cache first
    auto it = m_referenceCache.find(cacheKey);
    if (it != m_referenceCache.end()) {
        return it->second;
    }

    // Send textDocument/references request
    json params;
    params["textDocument"]["uri"] = "file://" + symbol.definition.filePath;
    params["position"]["line"] = symbol.definition.line - 1;
    params["position"]["character"] = symbol.definition.column - 1;
    params["context"]["includeDeclaration"] = true;

    int requestId = m_nextRequestId++;
    std::string request = MakeLSPRequest(requestId, "textDocument/references", params);
    m_pendingRequests[requestId] = "textDocument/references";

    if (!SendLSPMessage(request)) {
        return {};
    }

    auto response = ReadLSPMessage();
    if (!response.has_value()) {
        return {};
    }

    std::vector<Location> refs = ParseReferencesResponse(*response);
    m_referenceCache[cacheKey] = refs;
    return refs;
}

Location ClangdCodeIntelligence::FindDefinition(const Symbol& symbol) {
    if (!IsReady() || !symbol.definition.IsValid()) {
        return Location();
    }

    std::string cacheKey = symbol.name + "@" + symbol.definition.filePath + ":" + std::to_string(symbol.definition.line);

    // Check cache first
    auto it = m_definitionCache.find(cacheKey);
    if (it != m_definitionCache.end()) {
        return it->second;
    }

    // Send textDocument/definition request
    json params;
    params["textDocument"]["uri"] = "file://" + symbol.definition.filePath;
    params["position"]["line"] = symbol.definition.line - 1;
    params["position"]["character"] = symbol.definition.column - 1;

    int requestId = m_nextRequestId++;
    std::string request = MakeLSPRequest(requestId, "textDocument/definition", params);
    m_pendingRequests[requestId] = "textDocument/definition";

    if (!SendLSPMessage(request)) {
        return Location();
    }

    auto response = ReadLSPMessage();
    if (!response.has_value()) {
        return Location();
    }

    Location def = ParseDefinitionResponse(*response);
    if (def.IsValid()) {
        m_definitionCache[cacheKey] = def;
    }
    return def;
}

std::vector<Symbol> ClangdCodeIntelligence::ParseSymbolsResponse(const std::string& jsonStr) {
    std::vector<Symbol> symbols;
    try {
        json response = json::parse(jsonStr);
        if (!response.contains("result") || !response["result"].is_array()) {
            return symbols;
        }

        for (const auto& item : response["result"]) {
            Symbol sym = LSPSymbolToSymbol(item);
            if (sym.IsValid()) {
                // Convert numeric kind to string
                sym.kind = SymbolKindToString(item.value("kind", 0));
                symbols.push_back(sym);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "ClangdCodeIntelligence: parse symbols error: " << e.what() << std::endl;
    }
    return symbols;
}

std::vector<Location> ClangdCodeIntelligence::ParseReferencesResponse(const std::string& jsonStr) {
    std::vector<Location> locations;
    try {
        json response = json::parse(jsonStr);
        if (!response.contains("result") || !response["result"].is_array()) {
            return locations;
        }

        for (const auto& item : response["result"]) {
            Location loc = LSPLocationToLocation(item);
            if (loc.IsValid()) {
                locations.push_back(loc);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "ClangdCodeIntelligence: parse references error: " << e.what() << std::endl;
    }
    return locations;
}

Location ClangdCodeIntelligence::ParseDefinitionResponse(const std::string& jsonStr) {
    try {
        json response = json::parse(jsonStr);
        if (!response.contains("result")) {
            return Location();
        }

        // Definition can be a single Location or array of Locations
        if (response["result"].is_array() && !response["result"].empty()) {
            return LSPLocationToLocation(response["result"][0]);
        } else if (response["result"].is_object()) {
            return LSPLocationToLocation(response["result"]);
        }
    } catch (const std::exception& e) {
        std::cerr << "ClangdCodeIntelligence: parse definition error: " << e.what() << std::endl;
    }
    return Location();
}

} // namespace zagent