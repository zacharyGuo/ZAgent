#pragma once

#include <string>

#include "web/HttpServer.hxx"

namespace zagent {

// Static file server for serving web assets
class StaticFileServer {
public:
    StaticFileServer() = default;
    ~StaticFileServer() = default;

    // Delete copy, allow move
    StaticFileServer(const StaticFileServer&) = delete;
    StaticFileServer& operator=(const StaticFileServer&) = delete;
    StaticFileServer(StaticFileServer&&) = default;
    StaticFileServer& operator=(StaticFileServer&&) = default;

    // Register static file serving with the HTTP server
    void Register(HttpServer& server, const std::string& filesystemPath,
                  const std::string& urlPrefix = "/");
};

} // namespace zagent