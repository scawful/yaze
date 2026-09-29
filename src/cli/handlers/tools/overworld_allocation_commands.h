#ifndef YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_ALLOCATION_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_ALLOCATION_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze::cli {

/**
 * @brief Read-only overworld area allocation map ($00-$BF) with an optional
 * TailMapExpansion.asm compatibility check.
 *
 * Reports, per area ID: used or free (with the evidence), parent, area size,
 * world, map pointer validity, and which per-area tables do not cover the
 * ID. `--tail-check` adds what applying TailMapExpansion.asm would do to
 * this ROM and a safe/unsafe verdict. `--project` adds area names and hack
 * manifest ownership; `--symbols` names the code at the patch targets.
 */
class OverworldAllocationCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-allocation"; }

  std::string GetUsage() const override {
    return "overworld-allocation --rom <path> [--project <file.yaze>] "
           "[--tail-check] [--symbols <file.sym>] [--format json|text]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Overworld Allocation"; }

  Descriptor Describe() const override {
    Descriptor d;
    d.display_name = "overworld-allocation";
    d.summary =
        "Map overworld area IDs $00-$BF (used/free, parent, size, map "
        "pointer, per-area table coverage) and check whether "
        "TailMapExpansion.asm is safe for this ROM.";
    return d;
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_ALLOCATION_COMMANDS_H
