#ifndef YAZE_SRC_CLI_SERVICE_RESOURCES_COMMAND_HANDLER_H_
#define YAZE_SRC_CLI_SERVICE_RESOURCES_COMMAND_HANDLER_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/rom.h"
#include "cli/service/resources/command_context.h"

namespace yaze {
namespace cli {
namespace resources {

/**
 * @class CommandHandler
 * @brief Base class for CLI command handlers
 * 
 * Provides a consistent structure for implementing CLI commands with:
 * - Automatic argument parsing
 * - ROM context management
 * - Output formatting
 * - Error handling
 * 
 * Example usage:
 * ```cpp
 * class MyCommandHandler : public CommandHandler {
 *  protected:
 *   absl::Status ValidateArgs(const ArgumentParser& parser) override {
 *     return parser.RequireArgs({"required_arg"});
 *   }
 *   
 *   absl::Status Execute(Rom* rom, const ArgumentParser& parser, 
 *                        OutputFormatter& formatter) override {
 *     auto value = parser.GetString("required_arg").value();
 *     // ... business logic ...
 *     formatter.AddField("result", value);
 *     return absl::OkStatus();
 *   }
 * };
 * ```
 */
class CommandHandler {
 public:
  virtual ~CommandHandler() = default;

  /**
   * @brief Execute the command
   * 
   * This is the main entry point that orchestrates:
   * 1. Argument parsing
   * 2. Validation
   * 3. Context setup
   * 4. Business logic execution
   * 5. Output formatting
   */
  absl::Status Run(const std::vector<std::string>& args, Rom* rom_context);

 protected:
  /**
   * @brief Validate command arguments
   * 
   * Override this to check required arguments and perform custom validation.
   * Called before Execute().
   */
  virtual absl::Status ValidateArgs(const ArgumentParser& parser) = 0;

  /**
   * @brief Execute the command business logic
   * 
   * Override this to implement command-specific functionality.
   * The ROM is guaranteed to be loaded and labels initialized.
   */
  virtual absl::Status Execute(Rom* rom, const ArgumentParser& parser,
                              OutputFormatter& formatter) = 0;

  /**
   * @brief Get the command usage string
   */
  virtual std::string GetUsage() const = 0;

  /**
   * @brief Check if the command requires ROM labels
   * 
   * Override to return false if labels are not needed.
   */
  virtual bool RequiresLabels() const { return false; }

  /**
   * @brief Get the default output format ("json" or "text")
   */
  virtual std::string GetDefaultFormat() const { return "json"; }

  /**
   * @brief Get the output title for formatting
   */
  virtual std::string GetOutputTitle() const { return "Result"; }
};

/**
 * @brief Helper macro for creating simple command handlers
 * 
 * Usage:
 * ```cpp
 * DEFINE_COMMAND_HANDLER(ResourceList,
 *   "agent resource-list --type <type> [--format <table|json>]",
 *   { return parser.RequireArgs({"type"}); },
 *   {
 *     auto type = parser.GetString("type").value();
 *     ResourceContextBuilder builder(rom);
 *     auto labels = builder.GetLabels(type);
 *     for (const auto& [key, value] : *labels) {
 *       formatter.AddField(key, value);
 *     }
 *     return absl::OkStatus();
 *   }
 * )
 * ```
 */
#define DEFINE_COMMAND_HANDLER(name, usage_str, validate_body, execute_body) \
  class name##CommandHandler : public CommandHandler {                        \
   protected:                                                                 \
    std::string GetUsage() const override { return usage_str; }              \
    absl::Status ValidateArgs(const ArgumentParser& parser) override         \
      validate_body                                                           \
    absl::Status Execute(Rom* rom, const ArgumentParser& parser,             \
                        OutputFormatter& formatter) override                  \
      execute_body                                                            \
  };

}  // namespace resources
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_RESOURCES_COMMAND_HANDLER_H_

