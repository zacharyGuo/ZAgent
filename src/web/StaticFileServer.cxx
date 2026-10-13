#include "web/StaticFileServer.hxx"
#include <algorithm>
#include <string>

namespace zagent {

void StaticFileServer::Register(HttpServer& server, const std::string& filesystemPath,
                                const std::string& urlPrefix) {
    server.AddStaticFiles(urlPrefix, filesystemPath);
}

} // namespace zagent