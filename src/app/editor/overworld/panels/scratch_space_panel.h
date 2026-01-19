#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_SCRATCH_SPACE_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_SCRATCH_SPACE_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class ScratchSpacePanel
 * @brief Provides a scratch workspace for layout planning and clipboard operations
 *
 * Offers a temporary canvas for experimenting with tile arrangements
 * before committing them to the actual overworld maps.
 *
 * Uses ContentRegistry::Context to access the current OverworldEditor.
 * Self-registers via REGISTER_PANEL macro.
 */
class ScratchSpacePanel : public EditorPanel {
 public:
  ScratchSpacePanel() = default;

  // EditorPanel interface
  std::string GetId() const override { return "overworld.scratch"; }
  std::string GetDisplayName() const override { return "Scratch Workspace"; }
  std::string GetIcon() const override { return ICON_MD_DRAW; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_SCRATCH_SPACE_PANEL_H
