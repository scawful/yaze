#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_DEBUG_WINDOW_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_DEBUG_WINDOW_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class DebugWindowPanel
 * @brief Displays debug information for the Overworld Editor
 *
 * Uses ContentRegistry::Context to access the current OverworldEditor.
 * Self-registers via REGISTER_PANEL macro.
 */
class DebugWindowPanel : public EditorPanel {
 public:
  DebugWindowPanel() = default;

  // EditorPanel interface
  std::string GetId() const override { return "overworld.debug"; }
  std::string GetDisplayName() const override { return "Debug Window"; }
  std::string GetIcon() const override { return ICON_MD_BUG_REPORT; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_DEBUG_WINDOW_PANEL_H
