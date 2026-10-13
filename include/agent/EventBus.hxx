#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <chrono>

namespace zagent {

class Session;
class ToolCall;
class ToolResult;

// Event types
namespace EventTypes {
enum class Type {
    // Session events
    SessionCreated,
    SessionDestroyed,
    SessionLoaded,
    SessionCleared,

    // Message events
    MessageSent,
    MessageReceived,
    MessageFailed,

    // Tool events
    ToolCalled,
    ToolExecuted,
    ToolFailed,

    // Permission events
    PermissionGranted,
    PermissionDenied,
    PermissionRequested,

    // Execution events
    IterationStarted,
    IterationCompleted,
    PlanGenerated,
    PlanExecuted,

    // Build/Test events
    BuildStarted,
    BuildCompleted,
    TestStarted,
    TestCompleted,

    // Code events
    SymbolFound,
    ContextUpdated,
    ErrorDetected,
    ErrorFixed,
    ErrorResolved
};
} // namespace EventTypes

using EventType = EventTypes::Type;

// Event base class
class Event {
public:
    explicit Event(EventType type, const std::string& source = "")
        : m_type(type), m_source(source),
          m_timestamp(std::chrono::system_clock::now()) {}

    EventType GetType() const { return m_type; }
    const std::string& GetSource() const { return m_source; }

    // Convert event type to string
    static std::string TypeToString(EventType type);

    // Timestamp when event was created
    std::chrono::system_clock::time_point GetTimestamp() const {
        return m_timestamp;
    }

    // Convert event to JSON for serialization
    virtual std::string ToJSON() const = 0;

    virtual ~Event() = default;

protected:
    EventType m_type;
    std::string m_source;
    std::chrono::system_clock::time_point m_timestamp;
};

// Specific event classes

// Session events
class SessionCreatedEvent : public Event {
public:
    std::string sessionId;
    explicit SessionCreatedEvent(const std::string& id)
        : Event(EventType::SessionCreated, "Session"), sessionId(id) {}
    std::string ToJSON() const override;
};

class SessionDestroyedEvent : public Event {
public:
    std::string sessionId;
    explicit SessionDestroyedEvent(const std::string& id)
        : Event(EventType::SessionDestroyed, "Session"), sessionId(id) {}
    std::string ToJSON() const override;
};

class SessionLoadedEvent : public Event {
public:
    std::string sessionId;
    int messageCount;
    explicit SessionLoadedEvent(const std::string& id, int count = 0)
        : Event(EventType::SessionLoaded, "Session"), sessionId(id), messageCount(count) {}
    std::string ToJSON() const override;
};

class SessionClearedEvent : public Event {
public:
    std::string sessionId;
    explicit SessionClearedEvent(const std::string& id)
        : Event(EventType::SessionCleared, "Session"), sessionId(id) {}
    std::string ToJSON() const override;
};

// Message events
class MessageSentEvent : public Event {
public:
    std::string sessionId;
    std::string content;
    bool isUser;
    explicit MessageSentEvent(const std::string& id, const std::string& content,
                              bool user = true)
        : Event(EventType::MessageSent, "Agent"), sessionId(id),
          content(content), isUser(user) {}
    std::string ToJSON() const override;
};

class MessageReceivedEvent : public Event {
public:
    std::string sessionId;
    std::string content;
    explicit MessageReceivedEvent(const std::string& id, const std::string& content)
        : Event(EventType::MessageReceived, "LLM"), sessionId(id), content(content) {}
    std::string ToJSON() const override;
};

class MessageFailedEvent : public Event {
public:
    std::string sessionId;
    std::string error;
    explicit MessageFailedEvent(const std::string& id, const std::string& error)
        : Event(EventType::MessageFailed, "LLM"), sessionId(id), error(error) {}
    std::string ToJSON() const override;
};

// Tool events
class ToolCalledEvent : public Event {
public:
    std::string sessionId;
    std::string toolName;
    std::string arguments; // JSON
    explicit ToolCalledEvent(const std::string& id, const std::string& name,
                             const std::string& args)
        : Event(EventType::ToolCalled, "ToolRegistry"), sessionId(id),
          toolName(name), arguments(args) {}
    std::string ToJSON() const override;
};

class ToolExecutedEvent : public Event {
public:
    std::string sessionId;
    std::string toolName;
    bool success;
    std::string output;
    std::string error;
    long long durationMs;
    explicit ToolExecutedEvent(const std::string& id, const std::string& name,
                               bool success, const std::string& output,
                               const std::string& error, long long duration)
        : Event(EventType::ToolExecuted, "ToolRegistry"), sessionId(id),
          toolName(name), success(success), output(output), error(error),
          durationMs(duration) {}
    std::string ToJSON() const override;
};

class ToolFailedEvent : public Event {
public:
    std::string sessionId;
    std::string toolName;
    std::string error;
    explicit ToolFailedEvent(const std::string& id, const std::string& name,
                             const std::string& error)
        : Event(EventType::ToolFailed, "ToolRegistry"), sessionId(id),
          toolName(name), error(error) {}
    std::string ToJSON() const override;
};

// Permission events
class PermissionGrantedEvent : public Event {
public:
    std::string requestId;
    std::string category;
    std::string target;
    explicit PermissionGrantedEvent(const std::string& id, const std::string& cat,
                                    const std::string& tgt)
        : Event(EventType::PermissionGranted, "PermissionManager"),
          requestId(id), category(cat), target(tgt) {}
    std::string ToJSON() const override;
};

class PermissionDeniedEvent : public Event {
public:
    std::string requestId;
    std::string category;
    std::string target;
    explicit PermissionDeniedEvent(const std::string& id, const std::string& cat,
                                   const std::string& tgt)
        : Event(EventType::PermissionDenied, "PermissionManager"),
          requestId(id), category(cat), target(tgt) {}
    std::string ToJSON() const override;
};

class PermissionRequestedEvent : public Event {
public:
    std::string requestId;
    std::string category;
    std::string operation;
    std::string target;
    explicit PermissionRequestedEvent(const std::string& id, const std::string& cat,
                                      const std::string& op, const std::string& tgt)
        : Event(EventType::PermissionRequested, "PermissionManager"),
          requestId(id), category(cat), operation(op), target(tgt) {}
    std::string ToJSON() const override;
};

// Execution events
class IterationStartedEvent : public Event {
public:
    std::string sessionId;
    int iteration;
    explicit IterationStartedEvent(const std::string& id, int iter)
        : Event(EventType::IterationStarted, "Agent"), sessionId(id), iteration(iter) {}
    std::string ToJSON() const override;
};

class IterationCompletedEvent : public Event {
public:
    std::string sessionId;
    int iteration;
    int toolCalls;
    bool completed;
    explicit IterationCompletedEvent(const std::string& id, int iter, int calls, bool done)
        : Event(EventType::IterationCompleted, "Agent"), sessionId(id), iteration(iter),
          toolCalls(calls), completed(done) {}
    std::string ToJSON() const override;
};

class PlanGeneratedEvent : public Event {
public:
    std::string sessionId;
    int taskCount;
    std::string goal;
    explicit PlanGeneratedEvent(const std::string& id, int count, const std::string& g)
        : Event(EventType::PlanGenerated, "Planner"), sessionId(id), taskCount(count), goal(g) {}
    std::string ToJSON() const override;
};

class PlanExecutedEvent : public Event {
public:
    std::string sessionId;
    int taskCount;
    explicit PlanExecutedEvent(const std::string& id, int count)
        : Event(EventType::PlanExecuted, "Planner"), sessionId(id), taskCount(count) {}
    std::string ToJSON() const override;
};

// Build/Test events
class BuildStartedEvent : public Event {
public:
    std::string sessionId;
    std::string command;
    explicit BuildStartedEvent(const std::string& id, const std::string& cmd)
        : Event(EventType::BuildStarted, "BuildTool"), sessionId(id), command(cmd) {}
    std::string ToJSON() const override;
};

class BuildCompletedEvent : public Event {
public:
    std::string sessionId;
    int exitCode;
    bool success;
    long long durationMs;
    explicit BuildCompletedEvent(const std::string& id, int code, bool success,
                                 long long duration)
        : Event(EventType::BuildCompleted, "BuildTool"), sessionId(id), exitCode(code),
          success(success), durationMs(duration) {}
    std::string ToJSON() const override;
};

class TestStartedEvent : public Event {
public:
    std::string sessionId;
    std::string filter;
    explicit TestStartedEvent(const std::string& id, const std::string& filter)
        : Event(EventType::TestStarted, "TestTool"), sessionId(id), filter(filter) {}
    std::string ToJSON() const override;
};

class TestCompletedEvent : public Event {
public:
    std::string sessionId;
    int passed;
    int failed;
    explicit TestCompletedEvent(const std::string& id, int p, int f)
        : Event(EventType::TestCompleted, "TestTool"), sessionId(id), passed(p), failed(f) {}
    std::string ToJSON() const override;
};

// Code events
class SymbolFoundEvent : public Event {
public:
    std::string sessionId;
    std::string symbolName;
    std::string symbolKind;
    explicit SymbolFoundEvent(const std::string& id, const std::string& name,
                              const std::string& kind = "")
        : Event(EventType::SymbolFound, "CodeIntelligence"), sessionId(id),
          symbolName(name), symbolKind(kind) {}
    std::string ToJSON() const override;
};

class ContextUpdatedEvent : public Event {
public:
    std::string sessionId;
    int entryCount;
    explicit ContextUpdatedEvent(const std::string& id, int count = 0)
        : Event(EventType::ContextUpdated, "ContextManager"), sessionId(id),
          entryCount(count) {}
    std::string ToJSON() const override;
};

class ErrorDetectedEvent : public Event {
public:
    std::string sessionId;
    std::string errorType;
    std::string message;
    explicit ErrorDetectedEvent(const std::string& id, const std::string& type,
                                const std::string& msg)
        : Event(EventType::ErrorDetected, "ErrorTracker"), sessionId(id),
          errorType(type), message(msg) {}
    std::string ToJSON() const override;
};

class ErrorResolvedEvent : public Event {
public:
    std::string sessionId;
    std::string errorType;
    explicit ErrorResolvedEvent(const std::string& id, const std::string& type)
        : Event(EventType::ErrorResolved, "ErrorTracker"), sessionId(id),
          errorType(type) {}
    std::string ToJSON() const override;
};

// Abstract interface for the event bus
class IEventBus {
public:
    virtual ~IEventBus() = default;

    // Subscribe to events of a specific type
    // @param type Event type to subscribe to
    // @param handler Event handler function
    // @return Subscription ID for later unsubscription
    virtual std::string On(EventType type, std::function<void(const std::shared_ptr<Event>&)> handler) = 0;

    // Subscribe to all events regardless of type
    // @param handler Event handler function
    // @return Subscription ID for later unsubscription
    virtual std::string OnAll(std::function<void(const std::shared_ptr<Event>&)> handler) = 0;

    // Unsubscribe from a specific subscription
    // @param subscriptionId Subscription ID returned by On()
    virtual void Off(const std::string& subscriptionId) = 0;

    // Unsubscribe from all subscriptions
    virtual void Clear() = 0;

    // Emit an event to all subscribers of its type
    // @param event The event to emit
    virtual void Emit(std::shared_ptr<Event> event) = 0;

    // Emit an event asynchronously (on a background thread)
    // @param event The event to emit
    virtual void EmitAsync(std::shared_ptr<Event> event) = 0;

    // Get event count (useful for debugging)
    virtual size_t GetEventCount() const = 0;

    // Get active subscriptions for a type
    virtual size_t GetSubscriptionCount(EventType type) const = 0;

    // Check if a type has subscribers
    virtual bool HasSubscribers(EventType type) const = 0;
};

// Concrete event bus implementation
class EventBus : public IEventBus {
public:
    EventBus();
    ~EventBus() override;

    std::string On(EventType type, std::function<void(const std::shared_ptr<Event>&)> handler) override;
    std::string OnAll(std::function<void(const std::shared_ptr<Event>&)> handler) override;
    void Off(const std::string& subscriptionId) override;
    void Clear() override;
    void Emit(std::shared_ptr<Event> event) override;
    void EmitAsync(std::shared_ptr<Event> event) override;
    size_t GetEventCount() const override;
    size_t GetSubscriptionCount(EventType type) const override;
    bool HasSubscribers(EventType type) const override;

    // Singleton pattern
    static EventBus& Instance();

private:
    std::unordered_map<EventType, std::vector<std::pair<std::string, std::function<void(const std::shared_ptr<Event>&)>>>>
        m_subscriptionsByType;
    std::vector<std::pair<std::string, std::function<void(const std::shared_ptr<Event>&)>>>
        m_subscriptionsAll;
    size_t m_nextSubscriptionId = 0;
    size_t m_eventCount = 0;
    mutable std::mutex m_mutex;
    bool m_asyncEnabled = false;
    bool m_running = false;
    std::vector<std::shared_ptr<Event>> m_pendingEvents; // For async events
};

} // namespace zagent
