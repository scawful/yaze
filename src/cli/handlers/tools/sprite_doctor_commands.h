#ifndef YAZE_CLI_HANDLERS_TOOLS_SPRITE_DOCTOR_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_SPRITE_DOCTOR_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {

/**
 * @brief Sprite doctor command for validating sprite tables and data
 *
 * Validates:
 * - Sprite pointer table integrity
 * - Spriteset sheet references
 * - Sprite IDs in rooms
 * - Common sprite data issues
 */
class SpriteDoctorCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "sprite-doctor"; }

  std::string GetUsage() const override {
    return "sprite-doctor [--room <id>] [--all] [--verbose] [--format json|text]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Sprite Doctor"; }

  bool RequiresRom() const override { return true; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "sprite-doctor";
    desc.summary =
        "Validate sprite tables, graphics pointers, and common issues.";
    return desc;
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_TOOLS_SPRITE_DOCTOR_COMMANDS_H
