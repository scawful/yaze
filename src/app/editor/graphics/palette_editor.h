#ifndef YAZE_APP_EDITOR_PALETTE_EDITOR_H
#define YAZE_APP_EDITOR_PALETTE_EDITOR_H

#include "absl/status/status.h"
#include "app/editor/graphics/gfx_group_editor.h"
#include "app/editor/utils/editor.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

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
  void RecordChange(const std::string& group_name, size_t palette_index,
                    size_t color_index, const gfx::SnesColor& original_color,
                    const gfx::SnesColor& new_color) {
    if (recent_changes_.size() >= kMaxHistorySize) {
      recent_changes_.pop_front();
    }

    recent_changes_.push_back(
        {group_name, palette_index, color_index, original_color, new_color});
  }

  gfx::SnesColor RestoreOriginalColor(const std::string& group_name,
                                      size_t palette_index,
                                      size_t color_index) const {
    for (const auto& change : recent_changes_) {
      if (change.group_name == group_name &&
          change.palette_index == palette_index &&
          change.color_index == color_index) {
        return change.original_color;
      }
    }
    return gfx::SnesColor();
  }

  auto size() const { return recent_changes_.size(); }

  gfx::SnesColor& GetModifiedColor(size_t index) {
    return recent_changes_[index].new_color;
  }
  gfx::SnesColor& GetOriginalColor(size_t index) {
    return recent_changes_[index].original_color;
  }

  const std::deque<PaletteChange>& GetRecentChanges() const {
    return recent_changes_;
  }

 private:
  std::deque<PaletteChange> recent_changes_;
  static const size_t kMaxHistorySize = 50;
};
}  // namespace palette_internal

absl::Status DisplayPalette(gfx::SnesPalette& palette, bool loaded);

/**
 * @class PaletteEditor
 * @brief Allows the user to view and edit in game palettes.
 */
class PaletteEditor : public SharedRom, public Editor {
 public:
  PaletteEditor() {
    type_ = EditorType::kPalette;
    custom_palette_.push_back(gfx::SnesColor(0x7FFF));
  }

  absl::Status Update() override;

  absl::Status Cut() override { return absl::OkStatus(); }
  absl::Status Copy() override { return absl::OkStatus(); }
  absl::Status Paste() override { return absl::OkStatus(); }
  absl::Status Undo() override { return absl::OkStatus(); }
  absl::Status Redo() override { return absl::OkStatus(); }
  absl::Status Find() override { return absl::OkStatus(); }

  void DisplayCategoryTable();

  absl::Status EditColorInPalette(gfx::SnesPalette& palette, int index);
  absl::Status ResetColorToOriginal(gfx::SnesPalette& palette, int index,
                                    const gfx::SnesPalette& originalPalette);
  absl::Status DrawPaletteGroup(int category, bool right_side = false);

  void DrawCustomPalette();

  void DrawModifiedColors();

 private:
  absl::Status HandleColorPopup(gfx::SnesPalette& palette, int i, int j, int n);

  absl::Status status_;
  gfx::SnesColor current_color_;

  GfxGroupEditor gfx_group_editor_;

  std::vector<gfx::SnesColor> custom_palette_;

  ImVec4 saved_palette_[256] = {};

  palette_internal::PaletteEditorHistory history_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif