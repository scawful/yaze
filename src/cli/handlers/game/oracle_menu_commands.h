#ifndef YAZE_SRC_CLI_HANDLERS_GAME_ORACLE_MENU_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_ORACLE_MENU_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {

class OracleMenuIndexCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "oracle-menu-index"; }
  std::string GetUsage() const override {
    return "oracle-menu-index [--project <path>] [--table <label>] "
           "[--draw-filter <text>] [--missing-bins] [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }

  Descriptor Describe() const override {
    Descriptor descriptor;
    descriptor.display_name = "Oracle Menu Index";
    descriptor.summary =
        "Scan Oracle menu ASM for incbin assets, draw routines, and "
        "menu_offset component tables.";
    descriptor.todo_reference = "todo#oracle-menu-tooling";
    return descriptor;
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class OracleMenuSetOffsetCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "oracle-menu-set-offset"; }
  std::string GetUsage() const override {
    return "oracle-menu-set-offset --asm <path> --table <label> --index <n> "
           "--row <n> --col <n> [--project <path>] [--write] "
           "[--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }

  Descriptor Describe() const override {
    Descriptor descriptor;
    descriptor.display_name = "Oracle Menu Set Offset";
    descriptor.summary =
        "Update a single menu_offset(row,col) entry in a component table. "
        "Dry-run by default; use --write to apply.";
    descriptor.todo_reference = "todo#oracle-menu-tooling";
    return descriptor;
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"asm", "table", "index", "row", "col"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class OracleMenuValidateCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "oracle-menu-validate"; }
  std::string GetUsage() const override {
    return "oracle-menu-validate [--project <path>] [--max-row <n>] "
           "[--max-col <n>] [--strict] [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }

  Descriptor Describe() const override {
    Descriptor descriptor;
    descriptor.display_name = "Oracle Menu Validate";
    descriptor.summary =
        "Validate Oracle menu bins and component tables; exits non-zero on "
        "errors.";
    descriptor.todo_reference = "todo#oracle-menu-tooling";
    return descriptor;
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

// ---------------------------------------------------------------------------
// dungeon-oracle-preflight
//
// Single consolidated ROM safety check that runs all Oracle-specific
// preflight validators and emits one structured JSON report.  Intended as
// the "one command to verify all three workflows" before any write path.
// ---------------------------------------------------------------------------
class DungeonOraclePreflightCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-oracle-preflight"; }
  std::string GetUsage() const override {
    return "dungeon-oracle-preflight [--required-collision-rooms <hex,hex,...>] "
           "[--require-write-support] [--skip-collision-maps] "
           "[--report <path>] [--format <json|text>]";
  }

  Descriptor Describe() const override {
    Descriptor descriptor;
    descriptor.display_name = "Dungeon Oracle Preflight";
    descriptor.summary =
        "Run all Oracle ROM safety checks (water-fill region/table, custom "
        "collision maps, required-room collision) and emit one JSON report. "
        "Returns non-zero on any error.";
    descriptor.todo_reference = "todo#oracle-testing-infra";
    descriptor.entries = {
        {"--required-collision-rooms",
         "Comma-separated hex room IDs that must have authored custom "
         "collision data (e.g. 0x25,0x27 for D4 water gates)",
         ""},
        {"--require-write-support",
         "Also require expanded custom collision write region (needed for "
         "dungeon-import-custom-collision-json)",
         ""},
        {"--skip-collision-maps",
         "Skip per-room collision pointer validation (faster, for ROM doctor)",
         ""},
        {"--report",
         "Write the JSON report to this path in addition to stdout", ""},
    };
    return descriptor;
  }

  // Probes --report path writability before the formatter starts.
  // Called by CommandHandler::Run() before formatter.BeginObject(), so a
  // failure here produces zero stdout output (pure stderr + non-zero exit).
  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_ORACLE_MENU_COMMANDS_H_
