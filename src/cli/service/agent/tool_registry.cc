#include "cli/service/agent/tool_registry.h"

#include <algorithm>
#include <string_view>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

std::string StripTokenDelimiters(std::string token) {
  while (!token.empty() && (token.front() == '[' || token.front() == '(' ||
                            token.front() == ',')) {
    token.erase(token.begin());
  }
  while (!token.empty() &&
         (token.back() == ']' || token.back() == ')' || token.back() == ',')) {
    token.pop_back();
  }
  return token;
}

std::vector<std::string> SplitUsageTokens(std::string_view usage) {
  std::vector<std::string> tokens;
  for (std::string_view token : absl::StrSplit(usage, ' ', absl::SkipEmpty())) {
    tokens.emplace_back(token);
  }
  return tokens;
}

std::vector<std::string> InferRequiredArgsFromUsage(const std::string& usage) {
  std::vector<std::string> required_args;
  absl::flat_hash_set<std::string> seen;
  const auto tokens = SplitUsageTokens(usage);

  int optional_depth = 0;
  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string& token = tokens[i];
    optional_depth +=
        static_cast<int>(std::count(token.begin(), token.end(), '['));
    const bool is_optional = optional_depth > 0;

    const std::string cleaned = StripTokenDelimiters(token);
    if (!is_optional && cleaned.rfind("--", 0) == 0) {
      std::string arg_name = cleaned.substr(2);
      const size_t equals_pos = arg_name.find('=');
      if (equals_pos != std::string::npos) {
        arg_name = arg_name.substr(0, equals_pos);
      } else if (i + 1 < tokens.size()) {
        const std::string next = StripTokenDelimiters(tokens[i + 1]);
        if (next.empty() || next.front() != '<') {
          arg_name.clear();
        }
      } else {
        arg_name.clear();
      }

      if (!arg_name.empty() && seen.insert(arg_name).second) {
        required_args.push_back(std::move(arg_name));
      }
    }

    optional_depth -=
        static_cast<int>(std::count(token.begin(), token.end(), ']'));
  }

  return required_args;
}

std::vector<std::string> InferFlagArgsFromUsage(const std::string& usage) {
  std::vector<std::string> flag_args;
  absl::flat_hash_set<std::string> seen;
  const auto tokens = SplitUsageTokens(usage);

  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string cleaned = StripTokenDelimiters(tokens[i]);
    if (cleaned.rfind("--", 0) != 0) {
      continue;
    }

    std::string arg_name = cleaned.substr(2);
    if (arg_name.find('=') != std::string::npos) {
      continue;
    }

    const bool has_value_token =
        i + 1 < tokens.size() && !StripTokenDelimiters(tokens[i + 1]).empty() &&
        StripTokenDelimiters(tokens[i + 1]).front() == '<';
    if (has_value_token) {
      continue;
    }

    if (seen.insert(arg_name).second) {
      flag_args.push_back(std::move(arg_name));
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
  if (def.required_args.empty()) {
    def.required_args = InferRequiredArgsFromUsage(def.usage);
  }
  if (def.flag_args.empty()) {
    def.flag_args = InferFlagArgsFromUsage(def.usage);
  }
  return def;
}

}  // namespace

ToolRegistry& ToolRegistry::Get() {
  static ToolRegistry instance;
  EnsureBuiltinAgentToolsRegistered();
  return instance;
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
