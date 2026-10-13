#include "common/Config.hxx"

#include <charconv>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace zagent {
namespace {

const char* GetEnvironmentValue(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? value : "";
}

std::string RequireValue(
    int argc,
    char* argv[],
    int& index,
    std::string_view option) {
    if (index + 1 >= argc) {
        throw std::invalid_argument(
            "Missing value for " + std::string(option));
    }

    ++index;
    const std::string value = argv[index];
    if (value.empty()) {
        throw std::invalid_argument(
            "Value for " + std::string(option) + " cannot be empty");
    }
    return value;
}

int ParsePort(const std::string& value) {
    int port = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto result = std::from_chars(begin, end, port);

    if (result.ec != std::errc() || result.ptr != end || port < 1 ||
        port > 65535) {
        throw std::invalid_argument(
            "Port must be an integer between 1 and 65535");
    }

    return port;
}

} // namespace

Config Config::FromEnvironment() {
    Config config;
    config.workspace = GetEnvironmentValue("AI_WORKSPACE");
    config.baseUrl = GetEnvironmentValue("AI_BASE_URL");
    config.apiKey = GetEnvironmentValue("AI_API_KEY");
    config.model = GetEnvironmentValue("AI_MODEL");
    config.webMode = false;
    config.port = 8080;

    if (config.baseUrl.empty()) {
        config.baseUrl = "http://localhost:8000/v1";
    }
    if (config.model.empty()) {
        config.model = "qwen";
    }

    return config;
}

CommandLineOptions ParseCommandLine(int argc, char* argv[]) {
    CommandLineOptions options{false, Config::FromEnvironment()};

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "--help" || argument == "-h") {
            options.helpRequested = true;
        } else if (argument == "--workspace") {
            options.config.workspace = RequireValue(
                argc, argv, i, argument);
        } else if (argument == "--base-url") {
            options.config.baseUrl = RequireValue(
                argc, argv, i, argument);
        } else if (argument == "--api-key") {
            options.config.apiKey = RequireValue(
                argc, argv, i, argument);
        } else if (argument == "--model") {
            options.config.model = RequireValue(
                argc, argv, i, argument);
        } else if (argument == "--web") {
            options.config.webMode = true;
        } else if (argument == "--port") {
            options.config.port = ParsePort(
                RequireValue(argc, argv, i, argument));
        } else {
            throw std::invalid_argument("Unknown option: " + argument);
        }
    }

    return options;
}

} // namespace zagent
