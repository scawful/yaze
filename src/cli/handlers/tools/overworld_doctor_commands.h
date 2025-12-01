#ifndef YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_DOCTOR_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_DOCTOR_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze::cli {

/**
 * @brief ROM doctor command for overworld data integrity
 * 
 * Diagnoses and optionally repairs issues in overworld data:
 * - Tile16 region corruption (expanded tile16 at 0x1E8000-0x1F0000)
 * - Map32 pointer table issues
 * - ZSCustomOverworld version and feature detection
 * - Comparison against baseline ROM for corruption detection
 */
class OverworldDoctorCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-doctor"; }
  std::string GetDescription() const {
    return "Diagnose and repair overworld data corruption";
  }
  std::string GetUsage() const override {
    return "overworld-doctor --rom <path> [--baseline <path>] [--fix] [--output <path>] [--verbose]";
  }
  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_DOCTOR_COMMANDS_H

