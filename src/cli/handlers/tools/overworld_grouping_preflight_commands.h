#ifndef YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_GROUPING_PREFLIGHT_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_GROUPING_PREFLIGHT_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze::cli {

/**
 * @brief Read-only tall/wide/large grouping preflight before Save.
 *
 * Reports proposed vs current footprint, foreign-parent refusals, and
 * graphics/palette/animation mismatches. Contact labels are echoed as design
 * labels only (not collision proof). Never writes grouping.
 */
class OverworldGroupingPreflightCommandHandler
    : public resources::CommandHandler {
 public:
  std::string GetName() const override {
    return "overworld-grouping-preflight";
  }

  std::string GetUsage() const override {
    return "overworld-grouping-preflight --rom <path> --parent <hex> "
           "--size <large|wide|tall> --members <hex,hex,...> "
           "[--contact <id:label>]... [--project <file.yaze>] "
           "[--experimental-page-tile16] [--format json|text]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override {
    return "Overworld Grouping Preflight";
  }

  Descriptor Describe() const override {
    Descriptor d;
    d.display_name = "overworld-grouping-preflight";
    d.summary =
        "Read-only preflight for a proposed tall/wide/large area grouping: "
        "footprint, foreign-parent refusal, and graphics/palette mismatches. "
        "Contact labels are design-only.";
    return d;
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"parent", "size", "members"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_HANDLERS_TOOLS_OVERWORLD_GROUPING_PREFLIGHT_COMMANDS_H
