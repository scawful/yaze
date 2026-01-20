#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE16_EDITOR_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE16_EDITOR_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class Tile16EditorPanel
 * @brief EditorPanel wrapper for Tile16Editor
 *
 * Provides a dockable panel interface for the Tile16 editor.
 * Menu items from the original MenuBar are available through a context menu.
 *
 * Uses ContentRegistry::Context to access the current OverworldEditor,
 * then accesses its tile16_editor() member.
 * Self-registers via REGISTER_PANEL macro.
 */
class Tile16EditorPanel : public EditorPanel {
 public:
  Tile16EditorPanel() = default;

  // EditorPanel interface
  std::string GetId() const override { return "overworld.tile16_editor"; }
  std::string GetDisplayName() const override { return "Tile16 Editor"; }
  std::string GetIcon() const override { return ICON_MD_EDIT; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  int GetPriority() const override { return 15; }  // After selector (10)
  float GetPreferredWidth() const override { return 500.0f; }

  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE16_EDITOR_PANEL_H
