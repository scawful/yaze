#ifndef YAZE_CLI_HANDLERS_TOOLS_ROM_COMPARE_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_ROM_COMPARE_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze::cli {

/**
 * @brief Compare two ROMs to identify differences and corruption
 * 
 * Compares a target ROM against a baseline (e.g., vanilla.sfc) to:
 * - Identify version and feature differences
 * - Detect data corruption
 * - Show regions that have been modified
 * - Verify integrity of critical data structures
 */
class RomCompareCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "rom-compare"; }
  std::string GetDescription() const {
    return "Compare two ROMs to identify differences and corruption";
  }
  std::string GetUsage() const override {
    return "rom-compare --rom <path> --baseline <path> [--verbose] [--show-diff]";
  }
  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_HANDLERS_TOOLS_ROM_COMPARE_COMMANDS_H

