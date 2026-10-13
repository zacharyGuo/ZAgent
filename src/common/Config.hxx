#pragma once

#include <string>

namespace zagent {

struct Config {
    std::string workspace;
    std::string baseUrl;
    std::string apiKey;
    std::string model;
    bool webMode;
    int port;

    static Config FromEnvironment();
};

struct CommandLineOptions {
    bool helpRequested;
    Config config;
};

CommandLineOptions ParseCommandLine(int argc, char* argv[]);

} // namespace zagent
