#ifndef YAZE_APP_EDITOR_PALETTE_EDITOR_H
#define YAZE_APP_EDITOR_PALETTE_EDITOR_H

#include <imgui/imgui.h>

#include <stack>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"


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

struct PaletteChange {
  std::string groupName;
  size_t paletteIndex;
  size_t colorIndex;
  gfx::SNESColor originalColor;
};

class PaletteEditor : public SharedROM {
 public:
  absl::Status Update();

  void EditColorInPalette(gfx::SNESPalette& palette, int index);
  void ResetColorToOriginal(gfx::SNESPalette& palette, int index,
                            const gfx::SNESPalette& originalPalette);

  void DisplayPalette(gfx::SNESPalette& palette, bool loaded);

  void DrawPortablePalette(gfx::SNESPalette& palette);

 private:
  absl::Status DrawPaletteGroup(int i);

 private:
  void InitializeSavedPalette(const gfx::SNESPalette& palette) {
    for (int n = 0; n < palette.size(); n++) {
      saved_palette_[n].x = palette.GetColor(n).GetRGB().x / 255;
      saved_palette_[n].y = palette.GetColor(n).GetRGB().y / 255;
      saved_palette_[n].z = palette.GetColor(n).GetRGB().z / 255;
      saved_palette_[n].w = 255;  // Alpha
    }
  }

  std::stack<PaletteChange> changeHistory;

  ImVec4 saved_palette_[256] = {};
  ImVec4 current_color_;

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