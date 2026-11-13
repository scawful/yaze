#include "cli/service/agent/tool_registry.h"

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "cli/service/agent/tool_registration.h"
#include "cli/service/usage_schema.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

std::vector<std::string> InferRequiredArgsFromUsage(
    const std::vector<UsageParameter>& parameters) {
  std::vector<std::string> required_args;
  for (const UsageParameter& parameter : parameters) {
    if (!parameter.positional && parameter.required && parameter.takes_value) {
      required_args.push_back(parameter.name);
    }
  }
  return required_args;
}

std::vector<std::string> InferFlagArgsFromUsage(
    const std::vector<UsageParameter>& parameters) {
  std::vector<std::string> flag_args;
  for (const UsageParameter& parameter : parameters) {
    if (!parameter.positional && !parameter.takes_value) {
      flag_args.push_back(parameter.name);
    }
  }
  return flag_args;
}

ToolAccess InferToolAccess(const std::string& tool_name) {
  static const absl::flat_hash_set<std::string> kMutatingTools = {
      "build-compile",
      "build-configure",
      "build-test",
      "dungeon-import-custom-collision-json",
      "dungeon-import-water-fill-json",
      "dungeon-set-palette-color",
      "dungeon-set-room-property",
      "emulator-clear-breakpoint",
      "emulator-pause",
      "emulator-reset",
      "emulator-run",
      "emulator-set-breakpoint",
      "emulator-step",
      "emulator-write-memory",
      "gui-click",
      "gui-place-tile",
      "gui-type",
      "message-source-sync",
      "overworld-set-tile",
      "project-export",
      "project-import",
      "project-restore",
      "project-snapshot",
      "tools-extract-golden",
      "tools-patch-v3",
  };

  return kMutatingTools.contains(tool_name) ? ToolAccess::kMutating
                                            : ToolAccess::kReadOnly;
}

ToolDefinition NormalizeDefinition(ToolDefinition def) {
  if (def.access == ToolAccess::kReadOnly) {
    def.access = InferToolAccess(def.name);
  }
  const std::vector<UsageParameter> parameters =
      ParseUsageParameters(def.usage);
  if (def.required_args.empty()) {
    def.required_args = InferRequiredArgsFromUsage(parameters);
  }
  if (def.flag_args.empty()) {
    def.flag_args = InferFlagArgsFromUsage(parameters);
  }
  return def;
}

}  // namespace

ToolRegistry& ToolRegistry::Get() {
  static ToolRegistry instance;
  instance.EnsureInitialized();
  return instance;
}

void ToolRegistry::EnsureInitialized() {
  std::call_once(init_once_, [this]() {
#ifndef __EMSCRIPTEN__
    // The builtin tool set (tool_registration.cc) pulls in the full CLI
    // command-handler layer, which is intentionally excluded from the minimal
    // WASM agent library (see cli/agent.cmake). Skipping it here keeps the
    // Emscripten link from requiring RegisterBuiltinAgentTools/tool_registration.cc.
    RegisterBuiltinAgentTools(*this);
#else
    (void)this;
#endif
  });
}

void ToolRegistry::RegisterTool(const ToolDefinition& def,
                                HandlerFactory factory) {
  ToolDefinition normalized = NormalizeDefinition(def);
  const std::string tool_name = normalized.name;
  tools_[tool_name] = {std::move(normalized), std::move(factory)};
}

std::vector<ToolDefinition> ToolRegistry::GetAllTools() const {
  std::vector<ToolDefinition> defs;
  defs.reserve(tools_.size());
  for (const auto& [name, entry] : tools_) {
    defs.push_back(entry.def);
  }
  return defs;
}

std::optional<ToolDefinition> ToolRegistry::GetToolDefinition(
    const std::string& name) const {
  auto it = tools_.find(name);
  if (it != tools_.end()) {
    return it->second.def;
  }
  return std::nullopt;
}

std::vector<ToolDefinition> ToolRegistry::GetToolsByCategory(
    const std::string& category) const {
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
