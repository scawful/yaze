#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_SCRATCH_SPACE_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_SCRATCH_SPACE_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

class OverworldEditor;

/**
 * @class ScratchSpacePanel
 * @brief Provides a scratch workspace for layout planning and clipboard operations
 * 
 * Offers a temporary canvas for experimenting with tile arrangements
 * before committing them to the actual overworld maps.
 */
class ScratchSpacePanel : public EditorPanel {
 public:
  explicit ScratchSpacePanel(OverworldEditor* editor) : editor_(editor) {}

  // EditorPanel interface
  std::string GetId() const override { return "overworld.scratch"; }
  std::string GetDisplayName() const override { return "Scratch Workspace"; }
  std::string GetIcon() const override { return ICON_MD_DRAW; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;

 private:
  OverworldEditor* editor_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_SCRATCH_SPACE_PANEL_H
