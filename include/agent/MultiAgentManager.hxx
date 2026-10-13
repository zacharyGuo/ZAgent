#pragma once

#include "agent/AgentRole.hxx"
#include "tools/ToolRegistry.hxx"
#include "agent/EventBus.hxx"
#include "security/PermissionManager.hxx"
#include "context/IContextManager.hxx"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <optional>

namespace zagent {

/// Task allocation for multi-agent collaboration
struct TaskAllocation {
    std::string taskId;
    std::string assignedAgent;
    std::string taskDescription;
    std::string context;
    std::chrono::system_clock::time_point createdAt;
    bool completed = false;
    std::string result;
    std::vector<std::string> dependencies; // Other task IDs this depends on
};

/// Task execution result
struct TaskResult {
    std::string taskId;
    std::string agentName;
    std::string result;
    bool success = false;
    std::string error;
    std::chrono::system_clock::time_point completedAt;
};

/// Multi-agent manager for coordinating agent collaboration
class MultiAgentManager {
public:
    MultiAgentManager();
    ~MultiAgentManager();

    // Initialize with dependencies
    void SetToolRegistry(ToolRegistry& registry) { m_toolRegistry = &registry; }
    void SetPermissionManager(IPermissionManager* pm) { m_permissionManager = pm; }
    void SetContextManager(IContextManager* cm) { m_contextManager = cm; }
    void SetEventBus(EventBus* eb) { m_eventBus = eb; }

    // Agent management
    void RegisterAgent(const std::string& name, std::shared_ptr<IAgent> agent);
    void UnregisterAgent(const std::string& name);
    std::shared_ptr<IAgent> GetAgent(const std::string& name) const;
    std::vector<std::shared_ptr<IAgent>> GetAgents() const;
    std::vector<std::shared_ptr<IAgent>> GetAgentsByRole(AgentRole role) const;

    // Task coordination
    std::vector<TaskAllocation> PlanTask(const std::string& task, const std::string& context);
    std::vector<TaskResult> ExecuteTask(const std::string& task, const std::string& context);
    std::string ExecuteSimpleTask(const std::string& task, const std::string& context);

    // Coordination strategies
    enum class CoordinationStrategy {
        Sequential,     // Execute agents one after another
        Parallel,       // Execute agents in parallel where possible
        Collaborative   // Agents delegate to each other
    };

    void SetCoordinationStrategy(CoordinationStrategy strategy);

    // Get runtime state
    enum class State {
        Idle,
        Planning,
        Executing,
        Waiting,
        Completed,
        Failed
    };

    State GetState() const;
    void SetState(State state);

    // Task state management (for persistence)
    void SaveTaskState(const std::string& taskId, const TaskAllocation& task);
    std::optional<TaskAllocation> LoadTaskState(const std::string& taskId);

private:
    std::unordered_map<std::string, std::shared_ptr<IAgent>> m_agents;
    std::unordered_map<std::string, TaskAllocation> m_tasks;
    std::unordered_map<std::string, TaskResult> m_results;

    ToolRegistry* m_toolRegistry = nullptr;
    IPermissionManager* m_permissionManager = nullptr;
    IContextManager* m_contextManager = nullptr;
    EventBus* m_eventBus = nullptr;

    CoordinationStrategy m_strategy = CoordinationStrategy::Sequential;
    State m_state = State::Idle;
    std::string m_currentTask;

    mutable std::mutex m_mutex;

    // Helper methods
    std::shared_ptr<IAgent> SelectBestAgent(const std::string& task, const std::string& context);
    std::vector<std::string> DetectDependencies(const std::vector<TaskAllocation>& tasks);
    void UpdateEventBus(const TaskAllocation& task);
    void EmitTaskEvent(const std::string& eventType, const std::string& taskId,
                       const std::string& details = "");
};

/// Multi-Agent Runtime - implements IAgentRuntime interface
class MultiAgentRuntime : public IAgentRuntime {
public:
    static MultiAgentRuntime& Instance();

    // IAgentRuntime interface
    bool Initialize() override;
    void Shutdown() override;
    bool IsReady() const override;

    void RegisterAgent(std::shared_ptr<IAgent> agent) override;
    std::shared_ptr<IAgent> GetAgent(const std::string& name) const override;
    std::vector<std::shared_ptr<IAgent>> GetAgents() const override;
    std::vector<std::shared_ptr<IAgent>> GetAgentsByRole(AgentRole role) const override;

    std::string ExecuteTask(const std::string& task, const std::string& context) override;
    std::string CoordinateTask(const std::string& task, const std::string& context) override;

    zagent::IAgentRuntime::State GetRuntimeState() const override;

    // Multi-Agent specific
    std::vector<TaskAllocation> GetPendingTasks() const;
    std::vector<TaskResult> GetCompletedResults() const;
    void ClearCompletedResults();

private:
    MultiAgentRuntime();
    ~MultiAgentRuntime() override = default;

    MultiAgentManager m_manager;
    bool m_isReady = false;

    // Delete copy constructor and assignment operator
    MultiAgentRuntime(const MultiAgentRuntime&) = delete;
    MultiAgentRuntime& operator=(const MultiAgentRuntime&) = delete;
};

} // namespace zagent