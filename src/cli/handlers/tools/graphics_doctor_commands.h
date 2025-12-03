#ifndef YAZE_CLI_HANDLERS_TOOLS_GRAPHICS_DOCTOR_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_GRAPHICS_DOCTOR_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {

/**
 * @brief Graphics doctor command for validating graphics sheets and blocksets
 *
 * Validates:
 * - Graphics pointer table integrity
 * - Compression of graphics sheets
 * - Blockset references (main and room)
 * - Sheet integrity (empty/corrupted detection)
 */
class GraphicsDoctorCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "graphics-doctor"; }

  std::string GetUsage() const override {
    return "graphics-doctor [--sheet <id>] [--all] [--verbose] [--format json|text]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Graphics Doctor"; }

  bool RequiresRom() const override { return true; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "graphics-doctor";
    desc.summary =
        "Validate graphics sheets, compression, and palette references.";
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

#endif  // YAZE_CLI_HANDLERS_TOOLS_GRAPHICS_DOCTOR_COMMANDS_H
