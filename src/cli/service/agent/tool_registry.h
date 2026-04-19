#ifndef YAZE_CLI_SERVICE_AGENT_TOOL_REGISTRY_H_
#define YAZE_CLI_SERVICE_AGENT_TOOL_REGISTRY_H_

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/ai/common.h"  // For ToolCall
#include "cli/service/resources/command_handler.h"

namespace yaze {
class Rom;
namespace cli {
namespace agent {

enum class ToolAccess {
  kReadOnly,
  kMutating,
};

/**
 * @brief Metadata describing a tool for the LLM
 */
struct ToolDefinition {
  std::string name;
  std::string category;
  std::string description;
  std::string usage;
  std::vector<std::string> examples;
  bool requires_rom = false;
  bool requires_project = false;  // New: distinguish project vs ROM dependency
  ToolAccess access = ToolAccess::kReadOnly;
  std::vector<std::string> required_args;
  std::vector<std::string> flag_args;
};

/**
 * @class ToolRegistry
 * @brief Centralized registry for all agent tools
 *
 * Replaces the hardcoded switch/case in ToolDispatcher.
 * Allows tools to self-register and provides a unified way to
 * discover, query, and instantiate tools.
 */
class ToolRegistry {
 public:
  // Factory function to create a handler instance
  using HandlerFactory =
      std::function<std::unique_ptr<resources::CommandHandler>()>;

  static ToolRegistry& Get();

  // Registration
  void RegisterTool(const ToolDefinition& def, HandlerFactory factory);

  // Discovery
  std::vector<ToolDefinition> GetAllTools() const;
  std::optional<ToolDefinition> GetToolDefinition(
      const std::string& name) const;
  std::vector<ToolDefinition> GetToolsByCategory(
      const std::string& category) const;

  // Execution
  absl::StatusOr<std::unique_ptr<resources::CommandHandler>> CreateHandler(
      const std::string& tool_name) const;

 private:
  ToolRegistry() = default;
  void EnsureInitialized();

  struct ToolEntry {
    ToolDefinition def;
    HandlerFactory factory;
  };

  std::once_flag init_once_;
  std::map<std::string, ToolEntry> tools_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AGENT_TOOL_REGISTRY_H_
