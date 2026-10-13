#pragma once

#include <string>
#include <vector>
#include <memory>

namespace zagent {

/// Agent role types for multi-agent collaboration
enum class AgentRole {
    // Core roles
    Coordinator,    // Coordinates other agents
    Planner,        // Plans tasks
    Coder,          // Writes code
    Reviewer,       // Reviews code
    Tester,         // Runs tests
    Debugger,       // Debugs issues

    // Specialized roles
    Architect,      // Designs architecture
    Optimizer,      // Optimizes performance
    Documenter,     // Generates documentation
    SecurityAuditor, // Security review

    // Custom role
    Custom
};

/// Convert AgentRole to string
std::string AgentRoleToString(AgentRole role);

/// Agent configuration
struct AgentConfig {
    std::string name;
    std::string description;
    AgentRole role;
    std::vector<std::string> capabilities;
    std::vector<std::string> toolWhitelist; // Tools this agent is allowed to use
    int maxIterations = 30;
    bool enabled = true;
    bool canDelegate = false; // Can this agent delegate to other agents?

    AgentConfig() : role(AgentRole::Custom) {}
    AgentConfig(const std::string& n, const std::string& d, AgentRole r)
        : name(n), description(d), role(r), enabled(true), canDelegate(false) {}
};

/// Abstract interface for agent
class IAgent {
public:
    virtual ~IAgent() = default;

    // Get agent name
    virtual const std::string& Name() const = 0;

    // Get agent role
    virtual AgentRole Role() const = 0;

    // Get agent description
    virtual const std::string& Description() const = 0;

    // Check if agent can handle a task
    virtual bool CanHandle(const std::string& task) const = 0;

    // Execute a task and return result
    virtual std::string ExecuteTask(const std::string& task, const std::string& context) = 0;

    // Get available tools
    virtual std::vector<std::string> GetAvailableTools() const = 0;

    // Check if agent is enabled
    virtual bool IsEnabled() const = 0;

    // Enable/disable agent
    virtual void SetEnabled(bool enabled) = 0;

    // Get current state
    enum class State {
        Idle,
        Running,
        Completed,
        Failed,
        Blocked
    };

    virtual State CurrentState() const = 0;
};

/// Agent runtime interface
class IAgentRuntime {
public:
    virtual ~IAgentRuntime() = default;

    /// Runtime state enumeration
    enum class State {
        Idle,
        Initializing,
        Running,
        Completed,
        Failed,
        Shutdown
    };

    // Initialize the runtime
    virtual bool Initialize() = 0;

    // Shutdown the runtime
    virtual void Shutdown() = 0;

    // Check if runtime is ready
    virtual bool IsReady() const = 0;

    // Register an agent
    virtual void RegisterAgent(std::shared_ptr<IAgent> agent) = 0;

    // Get agent by name
    virtual std::shared_ptr<IAgent> GetAgent(const std::string& name) const = 0;

    // Get all registered agents
    virtual std::vector<std::shared_ptr<IAgent>> GetAgents() const = 0;

    // Get agents by role
    virtual std::vector<std::shared_ptr<IAgent>> GetAgentsByRole(AgentRole role) const = 0;

    // Execute a task with the appropriate agent
    virtual std::string ExecuteTask(const std::string& task, const std::string& context) = 0;

    // Coordinate multiple agents for a complex task
    virtual std::string CoordinateTask(const std::string& task, const std::string& context) = 0;

    // Get runtime state
    virtual State GetRuntimeState() const = 0;
};

} // namespace zagent