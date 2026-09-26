#ifndef YAZE_CLI_HANDLERS_GRAPHICS_GFX_SHEET_INVENTORY_COMMANDS_H_
#define YAZE_CLI_HANDLERS_GRAPHICS_GFX_SHEET_INVENTORY_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {

/**
 * @brief Read-only inventory of graphics sheets: storage, empty 16x16
 * blocks, reserved/flagged sheets, and which blocksets, spritesets,
 * overworld areas and dungeon rooms use each sheet.
 *
 * JSON fields match Oracle's Scripts/Analysis/gfx_sheet_inventory.py.
 * Reserved and flagged sheets come from the project (`--project-context`:
 * `[graphics_sheets]` and the hack manifest) plus `--reserved`/`--flagged`.
 */
class GfxSheetInventoryCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "gfx-sheet-inventory"; }

  std::string GetUsage() const override {
    return "gfx-sheet-inventory [--reserved 0x7B,0x7C] [--flagged 0xD4,0xD6] "
           "[--labels-csv <Spritesets.csv>] [--out <file.json>] "
           "[--format json|text]";
  }

  std::string GetDefaultFormat() const override { return "json"; }

  std::string GetOutputTitle() const override {
    return "Graphics Sheet Inventory";
  }

  bool RequiresRom() const override { return true; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "gfx-sheet-inventory";
    desc.summary =
        "List graphics sheets with free 16x16 blocks, reserved sheets, and "
        "the blocksets, spritesets, areas and rooms that use them.";
    return desc;
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_GRAPHICS_GFX_SHEET_INVENTORY_COMMANDS_H_
