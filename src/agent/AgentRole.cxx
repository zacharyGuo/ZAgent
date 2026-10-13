#include "agent/AgentRole.hxx"

namespace zagent {

std::string AgentRoleToString(AgentRole role) {
    switch (role) {
    case AgentRole::Coordinator: return "Coordinator";
    case AgentRole::Planner: return "Planner";
    case AgentRole::Coder: return "Coder";
    case AgentRole::Reviewer: return "Reviewer";
    case AgentRole::Tester: return "Tester";
    case AgentRole::Debugger: return "Debugger";
    case AgentRole::Architect: return "Architect";
    case AgentRole::Optimizer: return "Optimizer";
    case AgentRole::Documenter: return "Documenter";
    case AgentRole::SecurityAuditor: return "SecurityAuditor";
    case AgentRole::Custom: return "Custom";
    default: return "Unknown";
    }
}

} // namespace zagent
