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
 *
 * Supports structured JSON output for agent consumption and human-readable
 * text output with ASCII art summaries.
 */
class OverworldDoctorCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-doctor"; }

  std::string GetDescription() const {
    return "Diagnose and repair overworld data corruption";
  }

  std::string GetUsage() const override {
    return "overworld-doctor --rom <path> [--baseline <path>] [--fix] "
           "[--apply-tail-expansion] [--dry-run] [--output <path>] "
           "[--format json|text] [--verbose]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Overworld Doctor"; }

  Descriptor Describe() const override {
    Descriptor d;
    d.display_name = "overworld-doctor";
    d.summary = "Diagnose and repair overworld data corruption including "
                "tile16 corruption, map pointer issues, and ZSCustomOverworld "
                "feature detection.";
    d.todo_reference = "todo#overworld-doctor";
    return d;
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    // No required args - ROM is loaded via context
    // Optional: baseline, output, fix, dry-run, verbose, format
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_DOCTOR_COMMANDS_H
