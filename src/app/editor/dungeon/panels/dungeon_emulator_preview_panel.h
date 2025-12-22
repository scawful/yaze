#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_EMULATOR_PREVIEW_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_EMULATOR_PREVIEW_PANEL_H_

#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "app/gui/widgets/dungeon_object_emulator_preview.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonEmulatorPreviewPanel
 * @brief EditorPanel wrapper for DungeonObjectEmulatorPreview
 *
 * This panel provides a SNES emulator-based preview of dungeon objects,
 * showing how they will appear in-game.
 *
 * @see DungeonObjectEmulatorPreview - The underlying component
 * @see EditorPanel - Base interface
 */
class DungeonEmulatorPreviewPanel : public EditorPanel {
 public:
  explicit DungeonEmulatorPreviewPanel(
      gui::DungeonObjectEmulatorPreview* preview)
      : preview_(preview) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.emulator_preview"; }
  std::string GetDisplayName() const override { return "SNES Object Preview"; }
  std::string GetIcon() const override { return ICON_MD_MONITOR; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 65; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!preview_) return;

    preview_->set_visible(true);
    preview_->Render();

    // Sync visibility back if user closed via X button
    if (!preview_->is_visible() && p_open) {
      *p_open = false;
    }
  }

  void OnClose() override {
    if (preview_) {
      preview_->set_visible(false);
    }
  }

  // ==========================================================================
  // Panel-Specific Methods
  // ==========================================================================

  gui::DungeonObjectEmulatorPreview* preview() const { return preview_; }

 private:
  gui::DungeonObjectEmulatorPreview* preview_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_EMULATOR_PREVIEW_PANEL_H_
