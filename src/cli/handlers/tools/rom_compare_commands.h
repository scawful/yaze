#ifndef YAZE_CLI_HANDLERS_TOOLS_ROM_COMPARE_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_ROM_COMPARE_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze::cli {

/**
 * @brief Compare two ROMs to identify differences and corruption
 *
 * Compares a target ROM against a baseline (e.g., vanilla.sfc) to:
 * - Identify version and feature differences
 * - Detect data corruption in critical regions
 * - Show regions that have been modified
 * - Verify integrity of pointer tables, tile data, and custom data
 *
 * Supports structured JSON output for agent consumption.
 */
class RomCompareCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "rom-compare"; }

  std::string GetDescription() const {
    return "Compare two ROMs to identify differences and corruption";
  }

  std::string GetUsage() const override {
    return "rom-compare --rom <path> --baseline <path> [--verbose] "
           "[--show-diff] [--format json|text]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "ROM Compare"; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "rom-compare";
    desc.summary = "Compare a target ROM against a baseline to identify "
                   "differences, detect corruption, and verify data integrity.";
    desc.todo_reference = "todo#rom-compare";
    return desc;
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    if (!parser.GetString("baseline").has_value()) {
      return absl::InvalidArgumentError("Missing required --baseline argument");
    }
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_HANDLERS_TOOLS_ROM_COMPARE_COMMANDS_H
