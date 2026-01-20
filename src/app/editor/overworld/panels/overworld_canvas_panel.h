#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_CANVAS_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_CANVAS_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class OverworldCanvasPanel
 * @brief The main canvas panel for the Overworld Editor.
 *
 * Handles the display of the overworld map and associated tools.
 *
 * Uses ContentRegistry::Context to access the current OverworldEditor.
 * Self-registers via REGISTER_PANEL macro.
 */
class OverworldCanvasPanel : public EditorPanel {
 public:
  OverworldCanvasPanel() = default;

  // EditorPanel interface
  std::string GetId() const override { return "overworld.canvas"; }
  std::string GetDisplayName() const override { return "Overworld Canvas"; }
  std::string GetIcon() const override { return ICON_MD_MAP; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  std::string GetShortcutHint() const override { return "Ctrl+Shift+O"; }
  int GetPriority() const override { return 5; }  // Show first
  bool IsVisibleByDefault() const override { return true; }

  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_CANVAS_PANEL_H
