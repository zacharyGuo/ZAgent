#include "agent/EventBus.hxx"

#include <nlohmann/json.hpp>
#include <sstream>
#include <ctime>
#include <random>
#include <chrono>
#include <iostream>

using json = nlohmann::json;

namespace zagent {

// Static helper to convert EventType to string
std::string Event::TypeToString(EventType type) {
    switch (type) {
        // Session events
        case EventType::SessionCreated: return "SessionCreated";
        case EventType::SessionDestroyed: return "SessionDestroyed";
        case EventType::SessionLoaded: return "SessionLoaded";
        case EventType::SessionCleared: return "SessionCleared";

        // Message events
        case EventType::MessageSent: return "MessageSent";
        case EventType::MessageReceived: return "MessageReceived";
        case EventType::MessageFailed: return "MessageFailed";

        // Tool events
        case EventType::ToolCalled: return "ToolCalled";
        case EventType::ToolExecuted: return "ToolExecuted";
        case EventType::ToolFailed: return "ToolFailed";

        // Permission events
        case EventType::PermissionGranted: return "PermissionGranted";
        case EventType::PermissionDenied: return "PermissionDenied";
        case EventType::PermissionRequested: return "PermissionRequested";

        // Execution events
        case EventType::IterationStarted: return "IterationStarted";
        case EventType::IterationCompleted: return "IterationCompleted";
        case EventType::PlanGenerated: return "PlanGenerated";
        case EventType::PlanExecuted: return "PlanExecuted";

        // Build/Test events
        case EventType::BuildStarted: return "BuildStarted";
        case EventType::BuildCompleted: return "BuildCompleted";
        case EventType::TestStarted: return "TestStarted";
        case EventType::TestCompleted: return "TestCompleted";

        // Code events
        case EventType::SymbolFound: return "SymbolFound";
        case EventType::ContextUpdated: return "ContextUpdated";
        case EventType::ErrorDetected: return "ErrorDetected";
        case EventType::ErrorResolved: return "ErrorResolved";

        default: return "Unknown";
    }
}

// Helper: Format timestamp as ISO 8601 string
static std::string FormatTimePoint(std::chrono::system_clock::time_point tp) {
    auto in_time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf = *std::localtime(&in_time_t);
    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

// SessionCreatedEvent JSON
std::string SessionCreatedEvent::ToJSON() const {
    json j;
    j["type"] = "SessionCreated";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    return j.dump();
}

// SessionDestroyedEvent JSON
std::string SessionDestroyedEvent::ToJSON() const {
    json j;
    j["type"] = "SessionDestroyed";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    return j.dump();
}

// SessionLoadedEvent JSON
std::string SessionLoadedEvent::ToJSON() const {
    json j;
    j["type"] = "SessionLoaded";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["messageCount"] = messageCount;
    return j.dump();
}

// SessionClearedEvent JSON
std::string SessionClearedEvent::ToJSON() const {
    json j;
    j["type"] = "SessionCleared";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    return j.dump();
}

// MessageSentEvent JSON
std::string MessageSentEvent::ToJSON() const {
    json j;
    j["type"] = "MessageSent";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["content"] = content;
    j["isUser"] = isUser;
    return j.dump();
}

// MessageReceivedEvent JSON
std::string MessageReceivedEvent::ToJSON() const {
    json j;
    j["type"] = "MessageReceived";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["content"] = content;
    return j.dump();
}

// MessageFailedEvent JSON
std::string MessageFailedEvent::ToJSON() const {
    json j;
    j["type"] = "MessageFailed";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["error"] = error;
    return j.dump();
}

// ToolCalledEvent JSON
std::string ToolCalledEvent::ToJSON() const {
    json j;
    j["type"] = "ToolCalled";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["toolName"] = toolName;
    j["arguments"] = arguments;
    return j.dump();
}

// ToolExecutedEvent JSON
std::string ToolExecutedEvent::ToJSON() const {
    json j;
    j["type"] = "ToolExecuted";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["toolName"] = toolName;
    j["success"] = success;
    j["output"] = output;
    j["error"] = error;
    j["durationMs"] = durationMs;
    return j.dump();
}

// ToolFailedEvent JSON
std::string ToolFailedEvent::ToJSON() const {
    json j;
    j["type"] = "ToolFailed";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["toolName"] = toolName;
    j["error"] = error;
    return j.dump();
}

// PermissionGrantedEvent JSON
std::string PermissionGrantedEvent::ToJSON() const {
    json j;
    j["type"] = "PermissionGranted";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["requestId"] = requestId;
    j["category"] = category;
    j["target"] = target;
    return j.dump();
}

// PermissionDeniedEvent JSON
std::string PermissionDeniedEvent::ToJSON() const {
    json j;
    j["type"] = "PermissionDenied";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["requestId"] = requestId;
    j["category"] = category;
    j["target"] = target;
    return j.dump();
}

// PermissionRequestedEvent JSON
std::string PermissionRequestedEvent::ToJSON() const {
    json j;
    j["type"] = "PermissionRequested";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["requestId"] = requestId;
    j["category"] = category;
    j["operation"] = operation;
    j["target"] = target;
    return j.dump();
}

// IterationStartedEvent JSON
std::string IterationStartedEvent::ToJSON() const {
    json j;
    j["type"] = "IterationStarted";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["iteration"] = iteration;
    return j.dump();
}

// IterationCompletedEvent JSON
std::string IterationCompletedEvent::ToJSON() const {
    json j;
    j["type"] = "IterationCompleted";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["iteration"] = iteration;
    j["toolCalls"] = toolCalls;
    j["completed"] = completed;
    return j.dump();
}

// PlanGeneratedEvent JSON
std::string PlanGeneratedEvent::ToJSON() const {
    json j;
    j["type"] = "PlanGenerated";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["taskCount"] = taskCount;
    j["goal"] = goal;
    return j.dump();
}

// PlanExecutedEvent JSON
std::string PlanExecutedEvent::ToJSON() const {
    json j;
    j["type"] = "PlanExecuted";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["taskCount"] = taskCount;
    return j.dump();
}

// BuildStartedEvent JSON
std::string BuildStartedEvent::ToJSON() const {
    json j;
    j["type"] = "BuildStarted";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["command"] = command;
    return j.dump();
}

// BuildCompletedEvent JSON
std::string BuildCompletedEvent::ToJSON() const {
    json j;
    j["type"] = "BuildCompleted";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["exitCode"] = exitCode;
    j["success"] = success;
    j["durationMs"] = durationMs;
    return j.dump();
}

// TestStartedEvent JSON
std::string TestStartedEvent::ToJSON() const {
    json j;
    j["type"] = "TestStarted";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["filter"] = filter;
    return j.dump();
}

// TestCompletedEvent JSON
std::string TestCompletedEvent::ToJSON() const {
    json j;
    j["type"] = "TestCompleted";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["passed"] = passed;
    j["failed"] = failed;
    return j.dump();
}

// SymbolFoundEvent JSON
std::string SymbolFoundEvent::ToJSON() const {
    json j;
    j["type"] = "SymbolFound";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["symbolName"] = symbolName;
    j["symbolKind"] = symbolKind;
    return j.dump();
}

// ContextUpdatedEvent JSON
std::string ContextUpdatedEvent::ToJSON() const {
    json j;
    j["type"] = "ContextUpdated";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["entryCount"] = entryCount;
    return j.dump();
}

// ErrorDetectedEvent JSON
std::string ErrorDetectedEvent::ToJSON() const {
    json j;
    j["type"] = "ErrorDetected";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["errorType"] = errorType;
    j["message"] = message;
    return j.dump();
}

// ErrorResolvedEvent JSON
std::string ErrorResolvedEvent::ToJSON() const {
    json j;
    j["type"] = "ErrorResolved";
    j["source"] = m_source;
    j["timestamp"] = FormatTimePoint(m_timestamp);
    j["sessionId"] = sessionId;
    j["errorType"] = errorType;
    return j.dump();
}

// EventBus implementation

EventBus::EventBus() : m_nextSubscriptionId(0), m_eventCount(0),
                       m_asyncEnabled(false), m_running(false) {
}

EventBus::~EventBus() {
    m_running = false;
}

EventBus& EventBus::Instance() {
    static EventBus instance;
    return instance;
}

std::string EventBus::On(EventType type, std::function<void(const std::shared_ptr<Event>&)> handler) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string subscriptionId = "sub_" + std::to_string(m_nextSubscriptionId++);
    m_subscriptionsByType[type].emplace_back(subscriptionId, handler);
    return subscriptionId;
}

std::string EventBus::OnAll(std::function<void(const std::shared_ptr<Event>&)> handler) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string subscriptionId = "sub_" + std::to_string(m_nextSubscriptionId++);
    m_subscriptionsAll.emplace_back(subscriptionId, handler);
    return subscriptionId;
}

void EventBus::Off(const std::string& subscriptionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove from typed subscriptions
    for (auto& [type, subs] : m_subscriptionsByType) {
        subs.erase(std::remove_if(subs.begin(), subs.end(),
                                  [&subscriptionId](const auto& pair) {
                                      return pair.first == subscriptionId;
                                  }),
                   subs.end());
    }

    // Remove from all-type subscriptions
    m_subscriptionsAll.erase(
        std::remove_if(m_subscriptionsAll.begin(), m_subscriptionsAll.end(),
                       [&subscriptionId](const auto& pair) {
                           return pair.first == subscriptionId;
                       }),
        m_subscriptionsAll.end());
}

void EventBus::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subscriptionsByType.clear();
    m_subscriptionsAll.clear();
}

void EventBus::Emit(std::shared_ptr<Event> event) {
    if (!event) {
        return;
    }

    m_eventCount++;

    std::vector<std::function<void(const std::shared_ptr<Event>&)>> handlers;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Collect handlers for this event type
        auto typeIt = m_subscriptionsByType.find(event->GetType());
        if (typeIt != m_subscriptionsByType.end()) {
            for (const auto& pair : typeIt->second) {
                handlers.push_back(pair.second);
            }
        }

        // Collect all-type handlers
        for (const auto& pair : m_subscriptionsAll) {
            handlers.push_back(pair.second);
        }
    }

    // Call handlers outside of lock
    for (auto& handler : handlers) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            std::cerr << "EventBus: handler error: " << e.what() << std::endl;
        }
    }
}

void EventBus::EmitAsync(std::shared_ptr<Event> event) {
    if (!event) {
        return;
    }

    if (!m_asyncEnabled) {
        // Fall back to synchronous emission
        Emit(event);
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingEvents.push_back(event);
}

size_t EventBus::GetEventCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_eventCount;
}

size_t EventBus::GetSubscriptionCount(EventType type) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_subscriptionsByType.find(type);
    if (it != m_subscriptionsByType.end()) {
        return it->second.size();
    }
    return 0;
}

bool EventBus::HasSubscribers(EventType type) const {
    return GetSubscriptionCount(type) > 0;
}

} // namespace zagent
