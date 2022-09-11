#ifndef YAZE_APP_EDITOR_PALETTE_EDITOR_H
#define YAZE_APP_EDITOR_PALETTE_EDITOR_H

#include <imgui/imgui.h>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "app/rom.h"
#include "gui/canvas.h"
#include "gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

static constexpr absl::string_view kPaletteCategoryNames[] = {
    "Sword",       "Shield",   "Clothes",  "World Colors",
    "Area Colors", "Enemies",  "Dungeons", "World Map",
    "Dungeon Map", "Triforce", "Crystal"};

static constexpr absl::string_view kPaletteGroupNames[] = {
    "swords",       "shields",   "armors",  "ow_main",
    "ow_aux", "global_sprites",  "dungeon_main", "ow_mini_map",
    "ow_mini_map", "3d_object", "3d_object"};

class PaletteEditor {
 public:
  absl::Status Update();
  absl::Status DisplayPalette(gfx::SNESPalette& palette, bool loaded);

  auto SetupROM(ROM& rom) { rom_ = rom; }

 private:
  ImVec4 current_color_;
  ROM rom_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif