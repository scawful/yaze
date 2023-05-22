#ifndef YAZE_APP_EDITOR_PALETTE_EDITOR_H
#define YAZE_APP_EDITOR_PALETTE_EDITOR_H

#include <imgui/imgui.h>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "app/rom.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

constexpr int kNumPalettes = 11;

static constexpr absl::string_view kPaletteCategoryNames[] = {
    "Sword",       "Shield",   "Clothes",  "World Colors",
    "Area Colors", "Enemies",  "Dungeons", "World Map",
    "Dungeon Map", "Triforce", "Crystal"};

static constexpr absl::string_view kPaletteGroupNames[] = {
    "swords",      "shields",        "armors",       "ow_main",
    "ow_aux",      "global_sprites", "dungeon_main", "ow_mini_map",
    "ow_mini_map", "3d_object",      "3d_object"};


class PaletteEditor {
 public:
  absl::Status Update();
  void DisplayPalette(gfx::SNESPalette& palette, bool loaded);

  auto SetupROM(ROM& rom) { rom_ = rom; }

 private:
  void DrawPaletteGroup(int i);

  ImVec4 saved_palette_[256] = {};
  ImVec4 current_color_;
  ROM rom_;

  ImGuiColorEditFlags palette_button_flags =
      ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip;
  ImGuiColorEditFlags palette_button_flags_2 = ImGuiColorEditFlags_NoAlpha |
                                               ImGuiColorEditFlags_NoPicker |
                                               ImGuiColorEditFlags_NoTooltip;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif