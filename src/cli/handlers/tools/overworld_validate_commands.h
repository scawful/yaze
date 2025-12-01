#ifndef YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_VALIDATE_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_VALIDATE_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze::cli {

class OverworldValidateCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-validate"; }
  std::string GetDescription() const {
    return "Validate overworld map32 pointers and decompression";
  }
  std::string GetUsage() const override {
    return "overworld-validate [--include-tail] [--check-tile16] [--json] [--rom <path>]";
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
