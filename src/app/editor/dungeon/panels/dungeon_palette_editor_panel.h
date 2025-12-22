#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_PALETTE_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_PALETTE_EDITOR_PANEL_H_

#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "app/gui/widgets/palette_editor_widget.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonPaletteEditorPanel
 * @brief EditorPanel wrapper for PaletteEditorWidget in dungeon context
 *
 * This panel provides palette editing specifically for dungeon rooms,
 * wrapping the PaletteEditorWidget component.
 *
 * @see PaletteEditorWidget - The underlying component
 * @see EditorPanel - Base interface
 */
class DungeonPaletteEditorPanel : public EditorPanel {
 public:
  explicit DungeonPaletteEditorPanel(gui::PaletteEditorWidget* palette_editor)
      : palette_editor_(palette_editor) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.palette_editor"; }
  std::string GetDisplayName() const override { return "Palette Editor"; }
  std::string GetIcon() const override { return ICON_MD_PALETTE; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 70; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!palette_editor_) return;
    palette_editor_->Draw();
  }

  // ==========================================================================
  // Panel-Specific Methods
  // ==========================================================================

  gui::PaletteEditorWidget* palette_editor() const { return palette_editor_; }

  /**
   * @brief Set the current palette ID based on the active room
   * @param palette_id The palette ID from the room
   */
  void SetCurrentRoomPalette(int palette_id) {
    if (palette_editor_) {
      palette_editor_->SetCurrentPaletteId(palette_id);
    }
  }

 private:
  gui::PaletteEditorWidget* palette_editor_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_PALETTE_EDITOR_PANEL_H_
