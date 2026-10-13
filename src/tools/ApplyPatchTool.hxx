#pragma once

#include "tools/ITool.hxx"

namespace zagent {

class ApplyPatchTool : public ITool {
public:
    std::string Name() const override;
    std::string Description() const override;
    nlohmann::json Parameters() const override;
    ToolResult Execute(const nlohmann::json& arguments) override;
};

} // namespace zagent