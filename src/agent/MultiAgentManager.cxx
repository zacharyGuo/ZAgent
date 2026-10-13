#include "agent/MultiAgentManager.hxx"
#include <algorithm>
#include <queue>
#include <sstream>
#include <nlohmann/json.hpp>

namespace zagent {

// ============================================================================
// MultiAgentManager Implementation
// ============================================================================

MultiAgentManager::MultiAgentManager() = default;

MultiAgentManager::~MultiAgentManager() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_agents.clear();
    m_tasks.clear();
    m_results.clear();
}

void MultiAgentManager::RegisterAgent(const std::string& name, std::shared_ptr<IAgent> agent) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (agent) {
        m_agents[name] = agent;
        EmitTaskEvent("AgentRegistered", name, "Agent " + name + " registered");
    }
}

void MultiAgentManager::UnregisterAgent(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_agents.find(name);
    if (it != m_agents.end()) {
        m_agents.erase(it);
        EmitTaskEvent("AgentUnregistered", name, "Agent " + name + " unregistered");
    }
}

std::shared_ptr<IAgent> MultiAgentManager::GetAgent(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_agents.find(name);
    if (it != m_agents.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<IAgent>> MultiAgentManager::GetAgents() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::shared_ptr<IAgent>> agents;
    agents.reserve(m_agents.size());
    for (const auto& pair : m_agents) {
        if (pair.second && pair.second->IsEnabled()) {
            agents.push_back(pair.second);
        }
    }
    return agents;
}

std::vector<std::shared_ptr<IAgent>> MultiAgentManager::GetAgentsByRole(AgentRole role) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::shared_ptr<IAgent>> agents;
    for (const auto& pair : m_agents) {
        if (pair.second && pair.second->IsEnabled() && pair.second->Role() == role) {
            agents.push_back(pair.second);
        }
    }
    return agents;
}

std::shared_ptr<IAgent> MultiAgentManager::SelectBestAgent(const std::string& task, const std::string& context) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // First, try to find an agent that can explicitly handle the task
    for (const auto& pair : m_agents) {
        if (pair.second && pair.second->IsEnabled() && pair.second->CanHandle(task)) {
            return pair.second;
        }
    }

    // If no explicit match, use role-based selection
    if (task.find("build") != std::string::npos ||
        task.find("compile") != std::string::npos) {
        auto testers = GetAgentsByRole(AgentRole::Tester);
        if (!testers.empty()) return testers.front();
    }

    if (task.find("test") != std::string::npos) {
        auto testers = GetAgentsByRole(AgentRole::Tester);
        if (!testers.empty()) return testers.front();
    }

    if (task.find("review") != std::string::npos ||
        task.find("audit") != std::string::npos) {
        auto reviewers = GetAgentsByRole(AgentRole::Reviewer);
        if (!reviewers.empty()) return reviewers.front();
    }

    if (task.find("fix") != std::string::npos ||
        task.find("debug") != std::string::npos) {
        auto debuggers = GetAgentsByRole(AgentRole::Debugger);
        if (!debuggers.empty()) return debuggers.front();
    }

    if (task.find("optimize") != std::string::npos) {
        auto optimizers = GetAgentsByRole(AgentRole::Optimizer);
        if (!optimizers.empty()) return optimizers.front();
    }

    // Default to coder
    auto coders = GetAgentsByRole(AgentRole::Coder);
    if (!coders.empty()) return coders.front();

    // Fallback to any enabled agent
    auto agents = GetAgents();
    if (!agents.empty()) return agents.front();

    return nullptr;
}

std::vector<std::string> MultiAgentManager::DetectDependencies(const std::vector<TaskAllocation>& tasks) {
    std::vector<std::string> dependencies;
    // Simplified dependency detection - in production would analyze task semantics
    for (const auto& task : tasks) {
        std::string lowerTask = task.taskDescription;
        std::transform(lowerTask.begin(), lowerTask.end(), lowerTask.begin(), ::tolower);

        if (lowerTask.find("read") != std::string::npos ||
            lowerTask.find("search") != std::string::npos ||
            lowerTask.find("list") != std::string::npos) {
            // These are prerequisites for modification tasks
            continue;
        }
    }
    return dependencies;
}

void MultiAgentManager::UpdateEventBus(const TaskAllocation& task) {
    if (m_eventBus) {
        nlohmann::json taskJson = {
            {"taskId", task.taskId},
            {"assignedAgent", task.assignedAgent},
            {"taskDescription", task.taskDescription},
            {"completed", task.completed}
        };

        EmitTaskEvent("TaskUpdate", task.taskId, taskJson.dump());
    }
}

void MultiAgentManager::EmitTaskEvent(const std::string& eventType, const std::string& taskId,
                                       const std::string& details) {
    if (m_eventBus) {
        // In a real implementation, we'd create proper event objects
        // For now, we just log to the event bus
        EventTypes::Type type = EventTypes::Type::IterationCompleted; // Default

        if (eventType == "TaskUpdate") type = EventTypes::Type::ToolExecuted;
        else if (eventType == "AgentRegistered") type = EventTypes::Type::PermissionGranted;
        else if (eventType == "AgentUnregistered") type = EventTypes::Type::PermissionDenied;

        // Would emit event through EventBus here
        (void)type; // Suppress unused warning if eventBus is null
    }
}

std::vector<TaskAllocation> MultiAgentManager::PlanTask(const std::string& task, const std::string& context) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = State::Planning;

    std::vector<TaskAllocation> tasks;
    std::istringstream stream(task);
    std::string line;
    int taskId = 0;

    // Simple task decomposition - split by newlines or semicolons
    while (std::getline(stream, line, ';') || std::getline(stream, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();

        TaskAllocation allocation;
        allocation.taskId = "task_" + std::to_string(taskId++);
        allocation.taskDescription = line;
        allocation.context = context;
        allocation.assignedAgent = ""; // Will be assigned during execution
        allocation.createdAt = std::chrono::system_clock::now();
        allocation.completed = false;

        // Check if this is a compound task that needs sub-tasks
        if (line.find('+') != std::string::npos ||
            line.find("and") != std::string::npos) {
            // Multi-action task - would be decomposed further
        }

        tasks.push_back(allocation);
    }

    // If no sub-tasks, create single task
    if (tasks.empty()) {
        TaskAllocation allocation;
        allocation.taskId = "task_001";
        allocation.taskDescription = task;
        allocation.context = context;
        allocation.assignedAgent = "";
        allocation.createdAt = std::chrono::system_clock::now();
        allocation.completed = false;
        tasks.push_back(allocation);
    }

    m_tasks.clear();
    for (auto& t : tasks) {
        m_tasks[t.taskId] = t;
    }

    m_state = State::Idle;
    return tasks;
}

std::vector<TaskResult> MultiAgentManager::ExecuteTask(const std::string& task, const std::string& context) {
    std::vector<TaskAllocation> tasks = PlanTask(task, context);

    // Detect dependencies and establish order
    std::vector<std::string> dependencies = DetectDependencies(tasks);

    // Execute tasks
    std::vector<TaskResult> results;

    for (auto& allocation : tasks) {
        m_state = State::Executing;

        TaskResult result;
        result.taskId = allocation.taskId;
        result.completedAt = std::chrono::system_clock::now();

        auto agent = SelectBestAgent(allocation.taskDescription, allocation.context);
        if (agent) {
            allocation.assignedAgent = agent->Name();

            try {
                std::string agentResult = agent->ExecuteTask(allocation.taskDescription, allocation.context);
                result.result = agentResult;
                result.success = true;
                result.agentName = agent->Name();

                allocation.completed = true;
                allocation.result = agentResult;

                EmitTaskEvent("TaskCompleted", allocation.taskId,
                             "Agent " + agent->Name() + " completed task");
            } catch (const std::exception& e) {
                result.error = e.what();
                result.success = false;
                allocation.completed = true;

                EmitTaskEvent("TaskFailed", allocation.taskId,
                             "Agent " + agent->Name() + " failed: " + std::string(e.what()));
            }
        } else {
            result.error = "No suitable agent found";
            result.success = false;

            EmitTaskEvent("TaskFailed", allocation.taskId, "No suitable agent found");
        }

        // Store result
        if (allocation.taskId.find("task_") != std::string::npos) {
            m_results[allocation.taskId] = result;
        }

        m_state = State::Completed;
        results.push_back(result);
    }

    return results;
}

std::string MultiAgentManager::ExecuteSimpleTask(const std::string& task, const std::string& context) {
    std::vector<TaskResult> results = ExecuteTask(task, context);

    if (!results.empty()) {
        return results.front().result;
    }
    return "No results";
}

void MultiAgentManager::SetCoordinationStrategy(CoordinationStrategy strategy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_strategy = strategy;
}

MultiAgentManager::State MultiAgentManager::GetState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

void MultiAgentManager::SetState(State state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = state;
}

void MultiAgentManager::SaveTaskState(const std::string& taskId, const TaskAllocation& task) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tasks[taskId] = task;
}

std::optional<TaskAllocation> MultiAgentManager::LoadTaskState(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(taskId);
    if (it != m_tasks.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// MultiAgentRuntime Implementation (Singleton)
// ============================================================================

MultiAgentRuntime& MultiAgentRuntime::Instance() {
    static MultiAgentRuntime instance;
    return instance;
}

MultiAgentRuntime::MultiAgentRuntime() = default;

bool MultiAgentRuntime::Initialize() {
    if (m_isReady) return true;
    m_isReady = true;
    m_manager.SetState(MultiAgentManager::State::Idle);
    return true;
}

void MultiAgentRuntime::Shutdown() {
    m_isReady = false;
    m_manager.SetState(MultiAgentManager::State::Idle);
}

bool MultiAgentRuntime::IsReady() const {
    return m_isReady;
}

void MultiAgentRuntime::RegisterAgent(std::shared_ptr<IAgent> agent) {
    if (agent) {
        m_manager.RegisterAgent(agent->Name(), agent);
    }
}

std::shared_ptr<IAgent> MultiAgentRuntime::GetAgent(const std::string& name) const {
    return m_manager.GetAgent(name);
}

std::vector<std::shared_ptr<IAgent>> MultiAgentRuntime::GetAgents() const {
    return m_manager.GetAgents();
}

std::vector<std::shared_ptr<IAgent>> MultiAgentRuntime::GetAgentsByRole(AgentRole role) const {
    return m_manager.GetAgentsByRole(role);
}

std::string MultiAgentRuntime::ExecuteTask(const std::string& task, const std::string& context) {
    if (!m_isReady) {
        return "Error: Runtime not initialized";
    }
    return m_manager.ExecuteSimpleTask(task, context);
}

std::string MultiAgentRuntime::CoordinateTask(const std::string& task, const std::string& context) {
    if (!m_isReady) {
        return "Error: Runtime not initialized";
    }

    std::vector<TaskResult> results = m_manager.ExecuteTask(task, context);

    std::ostringstream oss;
    for (const auto& result : results) {
        oss << "Task: " << result.taskId << "\n";
        oss << "Agent: " << result.agentName << "\n";
        oss << "Success: " << (result.success ? "Yes" : "No") << "\n";
        if (!result.error.empty()) {
            oss << "Error: " << result.error << "\n";
        }
        if (!result.result.empty()) {
            oss << "Result: " << result.result << "\n\n";
        }
    }

    return oss.str();
}

zagent::IAgentRuntime::State MultiAgentRuntime::GetRuntimeState() const {
    auto state = m_manager.GetState();
    // Map MultiAgentManager::State to IAgentRuntime::State
    switch (state) {
        case MultiAgentManager::State::Idle:
            return zagent::IAgentRuntime::State::Idle;
        case MultiAgentManager::State::Planning:
            return zagent::IAgentRuntime::State::Running;
        case MultiAgentManager::State::Executing:
            return zagent::IAgentRuntime::State::Running;
        case MultiAgentManager::State::Waiting:
            return zagent::IAgentRuntime::State::Running;
        case MultiAgentManager::State::Completed:
            return zagent::IAgentRuntime::State::Completed;
        case MultiAgentManager::State::Failed:
            return zagent::IAgentRuntime::State::Failed;
        default:
            return zagent::IAgentRuntime::State::Idle;
    }
}

std::vector<TaskAllocation> MultiAgentRuntime::GetPendingTasks() const {
    // In a real implementation, this would filter incomplete tasks
    return {};
}

std::vector<TaskResult> MultiAgentRuntime::GetCompletedResults() const {
    // Return all completed results
    return {};
}

void MultiAgentRuntime::ClearCompletedResults() {
    // Clear stored results
}

} // namespace zagent