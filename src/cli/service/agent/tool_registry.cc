#include "cli/service/agent/tool_registry.h"

#include "absl/strings/str_cat.h"

namespace yaze {
namespace cli {
namespace agent {

ToolRegistry& ToolRegistry::Get() {
  static ToolRegistry instance;
  return instance;
}

void ToolRegistry::RegisterTool(const ToolDefinition& def, HandlerFactory factory) {
  tools_[def.name] = {def, std::move(factory)};
}

std::vector<ToolDefinition> ToolRegistry::GetAllTools() const {
  std::vector<ToolDefinition> defs;
  defs.reserve(tools_.size());
  for (const auto& [name, entry] : tools_) {
    defs.push_back(entry.def);
  }
  return defs;
}

std::optional<ToolDefinition> ToolRegistry::GetToolDefinition(const std::string& name) const {
  auto it = tools_.find(name);
  if (it != tools_.end()) {
    return it->second.def;
  }
  return std::nullopt;
}

std::vector<ToolDefinition> ToolRegistry::GetToolsByCategory(const std::string& category) const {
  std::vector<ToolDefinition> defs;
  for (const auto& [name, entry] : tools_) {
    if (entry.def.category == category) {
      defs.push_back(entry.def);
    }
  }
  return defs;
}

absl::StatusOr<std::unique_ptr<resources::CommandHandler>> 
ToolRegistry::CreateHandler(const std::string& tool_name) const {
  auto it = tools_.find(tool_name);
  if (it == tools_.end()) {
    return absl::NotFoundError(absl::StrCat("Tool not found: ", tool_name));
  }
  return it->second.factory();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
