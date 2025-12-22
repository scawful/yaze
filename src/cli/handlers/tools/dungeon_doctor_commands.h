#ifndef YAZE_CLI_HANDLERS_TOOLS_DUNGEON_DOCTOR_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_DUNGEON_DOCTOR_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze::cli {

/**
 * @brief Dungeon doctor command for room data integrity
 *
 * Diagnoses issues in dungeon room data:
 * - Room header and pointer validation
 * - Object counts and bounds checking
 * - Sprite counts and limits
 * - Chest count conflicts
 * - Door and staircase validation
 *
 * Supports structured JSON output for agent consumption.
 */
class DungeonDoctorCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-doctor"; }

  std::string GetDescription() const {
    return "Diagnose dungeon room data integrity and validate limits";
  }

  std::string GetUsage() const override {
    return "dungeon-doctor --rom <path> [--room <id>] [--all] "
           "[--format json|text] [--verbose]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Dungeon Doctor"; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "dungeon-doctor";
    desc.summary = "Diagnose dungeon room data including object counts, "
                   "sprite limits, chest conflicts, and bounds checking.";
    desc.todo_reference = "todo#dungeon-doctor";
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

#endif  // YAZE_CLI_HANDLERS_TOOLS_DUNGEON_DOCTOR_COMMANDS_H

