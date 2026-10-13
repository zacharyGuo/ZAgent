#include "web/HttpServer.hxx"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace zagent {

// Cross-platform socket type
#ifdef _WIN32
using SocketType = SOCKET;
static const SocketType INVALID_SOCKET_VALUE = INVALID_SOCKET;
static const int SOCKET_ERROR_VALUE = SOCKET_ERROR;
#else
using SocketType = int;
static const SocketType INVALID_SOCKET_VALUE = -1;
static const int SOCKET_ERROR_VALUE = -1;
#endif

struct HttpServer::Impl {
    std::string host;
    int port;
    SocketType serverSocket;
    std::atomic<bool> running;
    std::thread serverThread;

    struct Route {
        HttpMethod method;
        std::string path;
        RequestHandler handler;
    };

    std::vector<Route> routes;
    std::vector<std::pair<std::string, std::string>> staticFiles;

    ~Impl() {
        if (serverSocket != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            closesocket(serverSocket);
#else
            close(serverSocket);
#endif
        }
    }
};

// Helper to read file content
static std::string ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Helper to get MIME type
static std::string GetMimeType(const std::string& path) {
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") return "text/html";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") return "text/css";
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") return "application/javascript";
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".json") return "application/json";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".png") return "image/png";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".jpg") return "image/jpeg";
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".jpeg") return "image/jpeg";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".gif") return "image/gif";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".svg") return "image/svg+xml";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".ico") return "image/x-icon";
    return "application/octet-stream";
}

// Helper: split string
static std::vector<std::string> SplitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Helper: URL decode
static std::string UrlDecode(const std::string& encoded) {
    std::string decoded;
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '+') {
            decoded += ' ';
        } else if (encoded[i] == '%' && i + 2 < encoded.size()) {
            std::string hex = encoded.substr(i + 1, 2);
            char decodedChar = static_cast<char>(
                std::strtol(hex.c_str(), nullptr, 16));
            decoded += decodedChar;
            i += 2;
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}

// Parse HTTP request from buffer
static HttpRequest ParseRequest(const std::string& rawRequest) {
    HttpRequest request;
    std::istringstream stream(rawRequest);
    std::string line;

    // Parse request line
    std::getline(stream, line);
    auto requestParts = SplitString(line, ' ');
    if (requestParts.size() >= 3) {
        // Standard HTTP request line: METHOD PATH VERSION
        std::string method = requestParts[0];
        request.version = requestParts[2];
        if (method == "GET") request.method = HttpMethod::Get;
        else if (method == "POST") request.method = HttpMethod::Post;
        else if (method == "PUT") request.method = HttpMethod::Put;
        else if (method == "DELETE") request.method = HttpMethod::Delete;
        else request.method = HttpMethod::Options;

        // Parse path and query params
        std::string fullPath = requestParts[1];
        size_t queryPos = fullPath.find('?');
        if (queryPos != std::string::npos) {
            request.path = fullPath.substr(0, queryPos);
            std::string query = fullPath.substr(queryPos + 1);
            auto params = SplitString(query, '&');
            for (auto& param : params) {
                size_t eqPos = param.find('=');
                if (eqPos != std::string::npos) {
                    std::string key = UrlDecode(param.substr(0, eqPos));
                    std::string value = UrlDecode(param.substr(eqPos + 1));
                    request.queryParams[key] = value;
                }
            }
        } else {
            request.path = fullPath;
        }
    }

    // Parse headers
    while (std::getline(stream, line) && line != "\r" && line != "\n" && !line.empty()) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string name = line.substr(0, colonPos);
            std::string value = (colonPos + 1 < line.size()) ? line.substr(colonPos + 1) : "";
            // Trim whitespace from value
            if (!value.empty() && value[0] == ' ') value = value.substr(1);
            if (!value.empty() && value.back() == '\r') value.pop_back();
            request.headers[name] = value;
        }
    }

    // Parse body (remaining content)
    std::string body;
    std::getline(stream, body, '\0');
    request.body = body;

    return request;
}

// Create HTTP response
static std::string BuildResponse(const HttpResponse& response) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << response.statusCode << " " << response.statusText << "\r\n";

    // Add headers
    std::string contentType = "text/plain";
    auto headerIt = response.headers.find("Content-Type");
    if (headerIt != response.headers.end()) {
        contentType = headerIt->second;
    }

    oss << "Content-Type: " << contentType << "\r\n";
    oss << "Content-Length: " << response.body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << response.body;

    return oss.str();
}

// Handle a client connection
static void HandleClientConnection(SocketType clientSocket,
                                   const std::vector<HttpServer::Impl::Route>& routes,
                                   const std::vector<std::pair<std::string, std::string>>& staticFiles) {
    // Read request
    std::string requestBuffer;
    char buffer[4096];

    while (true) {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) break;
        requestBuffer.append(buffer, bytesRead);

        // Check if we've read the full request headers (look for \r\n\r\n)
        if (requestBuffer.find("\r\n\r\n") != std::string::npos) break;
    }

    // Parse request
    HttpRequest request = ParseRequest(requestBuffer);
    std::cerr << "[HttpServer] Received request: " << static_cast<int>(request.method) << " " << request.path << "\n";

    // Find handler
    HttpResponse response;
    bool handled = false;

    // Check routes first
    for (const auto& route : routes) {
        if (request.method == route.method && request.path == route.path) {
            response = route.handler(request);
            handled = true;
            break;
        }
    }
    // Debug: print route matching info
    if (!handled) {
        std::cerr << "[HttpServer] No route match for " << static_cast<int>(request.method)
                  << " " << request.path << " (routes: " << routes.size() << ")\n";
    }

    // Then check static files
    if (!handled) {
        for (const auto& [urlPrefix, fsPath] : staticFiles) {
            // Skip static file handling for API routes to let route handlers process them
            if (urlPrefix == "/" && request.path.size() > 4 && request.path.substr(0, 5) == "/api/") {
                continue;
            }
            if (request.path.substr(0, urlPrefix.size()) == urlPrefix) {
                std::string relativePath = request.path.substr(urlPrefix.size());
                if (relativePath.empty() || relativePath == "/") {
                    relativePath = "/index.html";
                }
                std::string filePath = fsPath + relativePath;
                std::string content = ReadFile(filePath);
                if (!content.empty()) {
                    response.statusCode = 200;
                    response.statusText = "OK";
                    response.headers["Content-Type"] = GetMimeType(filePath);
                    response.body = content;
                    handled = true;
                    break;
                }
                // If file doesn't exist, continue to fall through to 404 below
                continue;
            }
        }
    }

    // Default 404
    if (!handled) {
        response.statusCode = 404;
        response.statusText = "Not Found";
        response.body = "Not Found";
    }

    // Build and send response
    std::string responseStr = BuildResponse(response);
    send(clientSocket, responseStr.c_str(),
         static_cast<int>(responseStr.size()), 0);
}

HttpServer::HttpServer(const std::string& host, int port)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->host = host;
    m_impl->port = port;
    m_impl->serverSocket = INVALID_SOCKET_VALUE;
    m_impl->running = false;

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("Failed to initialize Winsock");
    }
#endif
}

HttpServer::~HttpServer() {
    Stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

void HttpServer::AddRoute(HttpMethod method, const std::string& path, RequestHandler handler) {
    m_impl->routes.push_back({method, path, std::move(handler)});
}

void HttpServer::AddStaticFiles(const std::string& urlPrefix, const std::string& filesystemPath) {
    m_impl->staticFiles.push_back({urlPrefix, filesystemPath});
}

void HttpServer::Start() {
    if (m_impl->running) return;

    // Create socket
    m_impl->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_impl->serverSocket == INVALID_SOCKET_VALUE) {
        throw std::runtime_error("Failed to create socket");
    }

    // Set socket options
    int opt = 1;
    setsockopt(m_impl->serverSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    // Bind socket
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(m_impl->port));

    if (bind(m_impl->serverSocket, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        throw std::runtime_error("Failed to bind socket");
    }

    // Listen
    if (listen(m_impl->serverSocket, 10) < 0) {
        throw std::runtime_error("Failed to listen on socket");
    }

    m_impl->running = true;
    m_impl->serverThread = std::thread([this]() {
        std::cout << "[HttpServer] Listening on " << m_impl->host << ":" << m_impl->port << std::endl;

        // Accept connections
        while (m_impl->running) {
            struct sockaddr_in clientAddress;
            socklen_t clientLength = sizeof(clientAddress);

            SocketType clientSocket =
                accept(m_impl->serverSocket, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientLength);

            if (clientSocket == SOCKET_ERROR_VALUE) {
                if (m_impl->running) continue;
                break;
            }

            // Handle request in a separate thread
            std::thread([this, clientSocket]() {
                HandleClientConnection(
                    clientSocket,
                    m_impl->routes,
                    m_impl->staticFiles);
#ifdef _WIN32
                closesocket(clientSocket);
#else
                close(clientSocket);
#endif
            }).detach();
        }
    });
}

void HttpServer::Stop() {
    m_impl->running = false;
    if (m_impl->serverThread.joinable()) {
        m_impl->serverThread.join();
    }
}

bool HttpServer::IsRunning() const {
    return m_impl->running;
}

} // namespace zagent