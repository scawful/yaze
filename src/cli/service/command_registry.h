#ifndef YAZE_CLI_SERVICE_COMMAND_REGISTRY_H_
#define YAZE_CLI_SERVICE_COMMAND_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {

/**
 * @class CommandRegistry
 * @brief Single source of truth for all z3ed commands
 *
 * Serves as the central registry for:
 * - CLI command routing
 * - Agent tool calling
 * - Help text generation
 * - TUI menu generation
 * - Function schema export (for AI)
 *
 * Ensures consistency: if a command exists, it's available to both
 * human users (CLI) and AI agents (tool calling).
 */
class CommandRegistry {
 public:
  struct CommandMetadata {
    std::string name;         // e.g., "resource-list"
    std::string category;     // e.g., "resource", "emulator", "dungeon"
    std::string description;  // Short description
    std::string usage;        // Full usage string
    bool available_to_agent;  // Can AI call this via tool dispatch?
    bool requires_rom;        // Requires ROM context?
    bool requires_grpc;       // Requires gRPC/emulator running?
    std::vector<std::string> aliases;   // Alternative names
    std::vector<std::string> examples;  // Usage examples
    std::string todo_reference;  // TODO tracker reference (if incomplete)
  };

  static CommandRegistry& Instance();

  /**
   * @brief Register a command handler
   */
  void Register(std::unique_ptr<resources::CommandHandler> handler,
                const CommandMetadata& metadata);

  /**
   * @brief Get a command handler by name or alias
   */
  resources::CommandHandler* Get(const std::string& name) const;

  /**
   * @brief Get command metadata
   */
  const CommandMetadata* GetMetadata(const std::string& name) const;

  /**
   * @brief Get all commands in a category
   */
  std::vector<std::string> GetCommandsInCategory(
      const std::string& category) const;

  /**
   * @brief Get all categories
   */
  std::vector<std::string> GetCategories() const;

  /**
   * @brief Get all commands available to AI agents
   */
  std::vector<std::string> GetAgentCommands() const;

  /**
   * @brief Export function schemas for AI tool calling (JSON)
   */
  std::string ExportFunctionSchemas() const;

  /**
   * @brief Generate help text for a command
   */
  std::string GenerateHelp(const std::string& name) const;

  /**
   * @brief Generate category help text
   */
  std::string GenerateCategoryHelp(const std::string& category) const;

  /**
   * @brief Generate complete help text (all commands)
   */
  std::string GenerateCompleteHelp() const;

  /**
   * @brief Execute a command by name
   */
  absl::Status Execute(const std::string& name,
                       const std::vector<std::string>& args,
                       Rom* rom_context = nullptr);

  /**
   * @brief Check if command exists
   */
  bool HasCommand(const std::string& name) const;

  /**
   * @brief Get total command count
   */
  size_t Count() const { return handlers_.size(); }

 private:
  CommandRegistry() = default;

  // Storage
  std::map<std::string, std::unique_ptr<resources::CommandHandler>> handlers_;
  std::map<std::string, CommandMetadata> metadata_;
  std::map<std::string, std::string> aliases_;  // alias → canonical name

  // Auto-register all commands
  void RegisterAllCommands();
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_COMMAND_REGISTRY_H_
