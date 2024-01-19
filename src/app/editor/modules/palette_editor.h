#ifndef YAZE_APP_EDITOR_PALETTE_EDITOR_H
#define YAZE_APP_EDITOR_PALETTE_EDITOR_H

#include <imgui/imgui.h>

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
  gfx::SNESColor newColor;
};

class PaletteEditorHistory {
 public:
  // Record a change in the palette editor
  void RecordChange(const std::string& groupName, size_t paletteIndex,
                    size_t colorIndex, const gfx::SNESColor& originalColor,
                    const gfx::SNESColor& newColor) {
    // Check size and remove the oldest if necessary
    if (recentChanges.size() >= maxHistorySize) {
      recentChanges.pop_front();
    }

    // Push the new change
    recentChanges.push_back(
        {groupName, paletteIndex, colorIndex, originalColor, newColor});
  }

  // Get recent changes for display in the palette editor
  const std::deque<PaletteChange>& GetRecentChanges() const {
    return recentChanges;
  }

  // Restore the original color
  gfx::SNESColor GetOriginalColor(const std::string& groupName,
                                  size_t paletteIndex,
                                  size_t colorIndex) const {
    for (const auto& change : recentChanges) {
      if (change.groupName == groupName &&
          change.paletteIndex == paletteIndex &&
          change.colorIndex == colorIndex) {
        return change.originalColor;
      }
    }
    // Handle error or return default (this is just an example,
    // handle as appropriate for your application)
    return gfx::SNESColor();
  }

 private:
  std::deque<PaletteChange> recentChanges;
  static const size_t maxHistorySize = 50;  // or any other number you deem fit
};

class PaletteEditor : public SharedROM {
 public:
  absl::Status Update();
  absl::Status DrawPaletteGroups();

  void EditColorInPalette(gfx::SNESPalette& palette, int index);
  void ResetColorToOriginal(gfx::SNESPalette& palette, int index,
                            const gfx::SNESPalette& originalPalette);
  void DisplayPalette(gfx::SNESPalette& palette, bool loaded);
  void DrawPortablePalette(gfx::SNESPalette& palette);
  absl::Status DrawPaletteGroup(int category);

 private:
  absl::Status HandleColorPopup(gfx::SNESPalette& palette, int i, int j, int n);

  void InitializeSavedPalette(const gfx::SNESPalette& palette) {
    for (int n = 0; n < palette.size(); n++) {
      saved_palette_[n].x = palette.GetColor(n).GetRGB().x / 255;
      saved_palette_[n].y = palette.GetColor(n).GetRGB().y / 255;
      saved_palette_[n].z = palette.GetColor(n).GetRGB().z / 255;
      saved_palette_[n].w = 255;  // Alpha
    }
  }

  absl::Status status_;

  PaletteEditorHistory history_;

  ImVec4 saved_palette_[256] = {};
  ImVec4 current_color_;

  ImGuiColorEditFlags color_popup_flags =
      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha;
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