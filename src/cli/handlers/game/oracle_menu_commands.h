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

}  // namespace yaze::cli::handlers

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_ORACLE_MENU_COMMANDS_H_
