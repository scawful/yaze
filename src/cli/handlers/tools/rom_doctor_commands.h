#ifndef YAZE_CLI_HANDLERS_TOOLS_ROM_DOCTOR_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_ROM_DOCTOR_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze::cli {

/**
 * @brief ROM doctor command for file integrity validation
 *
 * Diagnoses ROM-level issues:
 * - File size and header validation
 * - SNES checksum verification
 * - Expansion status detection
 * - Version marker checks
 * - Free space analysis
 *
 * Supports structured JSON output for agent consumption.
 */
class RomDoctorCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "rom-doctor"; }

  std::string GetDescription() const {
    return "Diagnose ROM file integrity, checksums, and expansion status";
  }

  std::string GetUsage() const override {
    return "rom-doctor --rom <path> [--format json|text] [--verbose] [--deep]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "ROM Doctor"; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "rom-doctor";
    desc.summary = "Diagnose ROM file integrity including checksums, "
                   "header validation, expansion status, and deep corruption "
                   "scans.";
    desc.todo_reference = "todo#rom-doctor";
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

#endif  // YAZE_CLI_HANDLERS_TOOLS_ROM_DOCTOR_COMMANDS_H

