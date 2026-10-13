#include "llm/OpenAIClient.hxx"

#include <curl/curl.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace zagent {
namespace {

using Json = nlohmann::json;

struct CurlDeleter {
    void operator()(CURL* curl) const {
        if (curl != nullptr) {
            curl_easy_cleanup(curl);
        }
    }
};

struct ListDeleter {
    void operator()(curl_slist* list) const {
        if (list != nullptr) {
            curl_slist_free_all(list);
        }
    }
};

void InitializeCurl() {
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        const CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (result != CURLE_OK) {
            throw std::runtime_error(
                "Failed to initialize libcurl: " +
                std::string(curl_easy_strerror(result)));
        }
    });
}

size_t WriteBodyCallback(
    char* data,
    size_t size,
    size_t count,
    void* userData) {
    const size_t bytes = size * count;
    auto* body = static_cast<std::string*>(userData);
    body->append(data, bytes);
    return bytes;
}

size_t WriteHeaderCallback(
    char* data,
    size_t size,
    size_t count,
    void* userData) {
    const size_t bytes = size * count;
    auto* headers = static_cast<std::string*>(userData);
    headers->append(data, bytes);
    return bytes;
}

std::string TrimAscii(std::string value) {
    const auto isSpace = [](unsigned char character) {
        return std::isspace(character) != 0;
    };

    value.erase(
        value.begin(),
        std::find_if_not(value.begin(), value.end(), isSpace));
    value.erase(
        std::find_if_not(value.rbegin(), value.rend(), isSpace).base(),
        value.end());
    return value;
}

std::string ExtractRequestId(const std::string& headers) {
    std::istringstream stream(headers);
    std::string line;

    while (std::getline(stream, line)) {
        const std::size_t separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        std::string name = TrimAscii(line.substr(0, separator));
        std::transform(
            name.begin(), name.end(), name.begin(), [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        if (name == "x-request-id") {
            return TrimAscii(line.substr(separator + 1));
        }
    }

    return {};
}

std::string JsonValueOrEmpty(
    const Json& object,
    const std::string& key) {
    if (!object.is_object() || !object.contains(key) || object[key].is_null()) {
        return {};
    }
    return object[key].get<std::string>();
}

} // namespace

OpenAIClient::OpenAIClient(
    std::string baseUrl,
    std::string apiKey,
    std::string model)
    : m_baseUrl(std::move(baseUrl)),
      m_apiKey(std::move(apiKey)),
      m_model(std::move(model)) {
    InitializeCurl();

    while (!m_baseUrl.empty() && m_baseUrl.back() == '/') {
        m_baseUrl.pop_back();
    }
    if (m_baseUrl.empty()) {
        throw std::invalid_argument("LLM base URL cannot be empty");
    }
    if (m_model.empty()) {
        throw std::invalid_argument("LLM model cannot be empty");
    }
}

LLMResponse OpenAIClient::Chat(
    const std::vector<Message>& messages,
    const std::vector<ToolDefinition>& tools) {
    if (messages.empty()) {
        return LLMResponse{};
    }

    try {
        const std::string requestBody = BuildRequestBody(messages, tools);
        std::string responseBody;
        std::string responseHeaders;
        long httpStatus = 0;

        std::unique_ptr<CURL, CurlDeleter> curl(curl_easy_init());
        if (!curl) {
            throw std::runtime_error("Failed to create a libcurl handle");
        }

        curl_slist* rawHeaders = nullptr;
        rawHeaders = curl_slist_append(rawHeaders, "Accept: application/json");
        rawHeaders = curl_slist_append(
            rawHeaders, "Content-Type: application/json");
        if (!m_apiKey.empty()) {
            const std::string authorization = "Authorization: Bearer " + m_apiKey;
            rawHeaders = curl_slist_append(rawHeaders, authorization.c_str());
        }
        if (rawHeaders == nullptr) {
            throw std::runtime_error("Failed to build HTTP headers");
        }
        std::unique_ptr<curl_slist, ListDeleter> headers(rawHeaders);

        curl_easy_setopt(curl.get(), CURLOPT_URL, BuildUrl().c_str());
        curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, requestBody.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, requestBody.size());
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteBodyCallback);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, WriteHeaderCallback);
        curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &responseHeaders);
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS, 15000L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, 120000L);
        curl_easy_setopt(
            curl.get(),
            CURLOPT_USERAGENT,
            "ZAgent/0.1.0");

        const CURLcode result = curl_easy_perform(curl.get());
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &httpStatus);

        if (result != CURLE_OK) {
            LLMResponse response;
            response.error = "LLM HTTP request failed: " +
                             std::string(curl_easy_strerror(result));
            return response;
        }

        return ParseResponse(
            httpStatus,
            responseBody,
            ExtractRequestId(responseHeaders));
    } catch (const std::exception& exception) {
        LLMResponse response;
        response.error = exception.what();
        return response;
    }
}

std::string OpenAIClient::BuildUrl() const {
    return m_baseUrl + "/chat/completions";
}

std::string OpenAIClient::BuildRequestBody(
    const std::vector<Message>& messages,
    const std::vector<ToolDefinition>& tools) const {
    Json request;
    request["model"] = m_model;
    request["stream"] = false;

    request["messages"] = Json::array();
    for (const Message& message : messages) {
        Json jsonMessage;
        jsonMessage["role"] = ToString(message.role);
        jsonMessage["content"] = message.content;
        if (!message.name.empty()) {
            jsonMessage["name"] = message.name;
        }
        if (!message.toolCallId.empty()) {
            jsonMessage["tool_call_id"] = message.toolCallId;
        }
        request["messages"].push_back(std::move(jsonMessage));
    }

    if (!tools.empty()) {
        request["tools"] = Json::array();
        for (const ToolDefinition& tool : tools) {
            Json definition;
            definition["type"] = "function";
            definition["function"] = {
                {"name", tool.name},
                {"description", tool.description},
                {"parameters", tool.parameters}};
            request["tools"].push_back(std::move(definition));
        }
    }

    return request.dump();
}

LLMResponse OpenAIClient::ParseResponse(
    long httpStatus,
    const std::string& responseBody,
    const std::string& requestId) const {
    LLMResponse response;
    response.requestId = requestId;

    const Json body = Json::parse(responseBody, nullptr, false);
    if (body.is_discarded()) {
        response.error = "LLM returned invalid JSON";
        return response;
    }

    response.model = JsonValueOrEmpty(body, "model");

    if (httpStatus < 200 || httpStatus >= 300) {
        std::string error = "LLM HTTP request failed with status " +
                            std::to_string(httpStatus);
        if (body.contains("error")) {
            const Json& errorObject = body["error"];
            if (errorObject.is_object()) {
                const std::string message =
                    JsonValueOrEmpty(errorObject, "message");
                if (!message.empty()) {
                    error += ": " + message;
                }
            } else if (errorObject.is_string()) {
                error += ": " + errorObject.get<std::string>();
            }
        } else if (!responseBody.empty()) {
            error += ": " + TrimAscii(responseBody);
        }
        response.error = std::move(error);
        return response;
    }

    const Json choices = body.value("choices", Json::array());
    if (!choices.is_array() || choices.empty()) {
        response.error = "LLM response does not contain a choice";
        return response;
    }

    const Json& choice = choices.front();
    const Json message = choice.value("message", Json::object());
    response.content = JsonValueOrEmpty(message, "content");

    const Json toolCalls = message.value("tool_calls", Json::array());
    if (toolCalls.is_array()) {
        for (const Json& toolCall : toolCalls) {
            const std::string id = JsonValueOrEmpty(toolCall, "id");
            const Json function = toolCall.value("function", Json::object());
            const std::string name = JsonValueOrEmpty(function, "name");
            const std::string rawArguments =
                JsonValueOrEmpty(function, "arguments");

            const Json arguments = Json::parse(rawArguments, nullptr, false);
            if (arguments.is_discarded()) {
                response.error = "Tool call contains invalid JSON arguments";
                return response;
            }

            response.toolCalls.push_back(
                ToolCall{id, name, arguments});
        }
    }

    response.type = response.toolCalls.empty()
                        ? LLMResponseType::Text
                        : LLMResponseType::ToolCall;
    response.success = true;
    return response;
}

} // namespace zagent
