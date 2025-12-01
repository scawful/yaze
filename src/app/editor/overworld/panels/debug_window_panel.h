#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_DEBUG_WINDOW_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_DEBUG_WINDOW_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

class OverworldEditor;

/**
 * @class DebugWindowPanel
 * @brief Displays debug information for the Overworld Editor
 */
class DebugWindowPanel : public EditorPanel {
 public:
  explicit DebugWindowPanel(OverworldEditor* editor) : editor_(editor) {}

  // EditorPanel interface
  std::string GetId() const override { return "overworld.debug"; }
  std::string GetDisplayName() const override { return "Debug Window"; }
  std::string GetIcon() const override { return ICON_MD_BUG_REPORT; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;

 private:
  OverworldEditor* editor_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_DEBUG_WINDOW_PANEL_H
