#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace zagent {

// HTTP method types
enum class HttpMethod {
    Get,
    Post,
    Put,
    Delete,
    Options
};

// HTTP request structure
struct HttpRequest {
    HttpMethod method = HttpMethod::Get;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::unordered_map<std::string, std::string> queryParams;
};

// HTTP response structure
struct HttpResponse {
    int statusCode = 200;
    std::string statusText = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

// Function type for request handlers
using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
public:
    HttpServer(const std::string& host = "0.0.0.0", int port = 8080);
    ~HttpServer();

    // Delete copy, allow move
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) = default;
    HttpServer& operator=(HttpServer&&) = default;

    // Register a route handler
    void AddRoute(HttpMethod method, const std::string& path, RequestHandler handler);

    // Add static file serving
    void AddStaticFiles(const std::string& urlPrefix, const std::string& filesystemPath);

    // Start the server (blocking)
    void Start();

    // Stop the server
    void Stop();

    // Check if server is running
    bool IsRunning() const;

public:
    struct Impl;

private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace zagent