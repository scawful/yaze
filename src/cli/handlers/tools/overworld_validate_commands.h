#ifndef YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_VALIDATE_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_VALIDATE_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze::cli {

/**
 * @brief Validate overworld map32 pointers and decompression
 *
 * Checks each map's pointer table entries and attempts to decompress
 * the referenced data to verify integrity. Useful for detecting
 * ROM corruption or invalid pointer modifications.
 *
 * Supports structured JSON output for agent consumption.
 */
class OverworldValidateCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-validate"; }

  std::string GetDescription() const {
    return "Validate overworld map32 pointers and decompression";
  }

  std::string GetUsage() const override {
    return "overworld-validate --rom <path> [--include-tail] [--check-tile16] "
           "[--format json|text] [--verbose]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Overworld Validation"; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "overworld-validate";
    desc.summary = "Validate map32 pointer tables and decompression for all "
                   "overworld maps. Detects corruption and pointer issues.";
    desc.todo_reference = "todo#overworld-validate";
    return desc;
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_VALIDATE_COMMANDS_H
