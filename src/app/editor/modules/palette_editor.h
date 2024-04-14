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

namespace palette_internal {
struct PaletteChange {
  std::string group_name;
  size_t palette_index;
  size_t color_index;
  gfx::SnesColor original_color;
  gfx::SnesColor new_color;
};

class PaletteEditorHistory {
 public:
  // Record a change in the palette editor
  void RecordChange(const std::string& groupName, size_t paletteIndex,
                    size_t colorIndex, const gfx::SnesColor& originalColor,
                    const gfx::SnesColor& newColor) {
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
  gfx::SnesColor GetOriginalColor(const std::string& groupName,
                                  size_t paletteIndex,
                                  size_t colorIndex) const {
    for (const auto& change : recentChanges) {
      if (change.group_name == groupName &&
          change.palette_index == paletteIndex &&
          change.color_index == colorIndex) {
        return change.original_color;
      }
    }
    // Handle error or return default (this is just an example,
    // handle as appropriate for your application)
    return gfx::SnesColor();
  }

 private:
  std::deque<PaletteChange> recentChanges;
  static const size_t maxHistorySize = 50;  // or any other number you deem fit
};
}  // namespace palette_internal

class PaletteEditor : public SharedROM {
 public:
  absl::Status Update();
  absl::Status DrawPaletteGroups();

  absl::Status EditColorInPalette(gfx::SnesPalette& palette, int index);
  absl::Status ResetColorToOriginal(gfx::SnesPalette& palette, int index,
                                    const gfx::SnesPalette& originalPalette);
  void DisplayPalette(gfx::SnesPalette& palette, bool loaded);
  void DrawPortablePalette(gfx::SnesPalette& palette);
  absl::Status DrawPaletteGroup(int category);

 private:
  absl::Status HandleColorPopup(gfx::SnesPalette& palette, int i, int j, int n);

  absl::Status InitializeSavedPalette(const gfx::SnesPalette& palette) {
    for (int n = 0; n < palette.size(); n++) {
      ASSIGN_OR_RETURN(auto color, palette.GetColor(n));
      saved_palette_[n].x = color.rgb().x / 255;
      saved_palette_[n].y = color.rgb().y / 255;
      saved_palette_[n].z = color.rgb().z / 255;
      saved_palette_[n].w = 255;  // Alpha
    }
    return absl::OkStatus();
  }

  absl::Status status_;

  palette_internal::PaletteEditorHistory history_;

  ImVec4 saved_palette_[256] = {};
  gfx::SnesColor current_color_;

  ImGuiColorEditFlags color_popup_flags =
      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha;
  ImGuiColorEditFlags palette_button_flags = ImGuiColorEditFlags_NoAlpha;
  ImGuiColorEditFlags palette_button_flags_2 = ImGuiColorEditFlags_NoAlpha |
                                               ImGuiColorEditFlags_NoPicker |
                                               ImGuiColorEditFlags_NoTooltip;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif