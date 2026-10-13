#pragma once

#include <string>

#include "tools/ITool.hxx"

namespace zagent {

class BuildTool : public ITool {
public:
    std::string Name() const override;
    std::string Description() const override;
    nlohmann::json Parameters() const override;
    ToolResult Execute(const nlohmann::json& arguments) override;

private:
    std::string AnalyzeBuildErrors(const std::string& errorOutput) const;
};

} // namespace zagent