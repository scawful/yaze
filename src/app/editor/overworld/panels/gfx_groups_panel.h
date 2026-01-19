#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_GFX_GROUPS_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_GFX_GROUPS_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class GfxGroupsPanel
 * @brief Graphics group configuration editor
 *
 * Uses ContentRegistry::Context to access the current OverworldEditor.
 * Self-registers via REGISTER_PANEL macro.
 */
class GfxGroupsPanel : public EditorPanel {
 public:
  GfxGroupsPanel() = default;

  // EditorPanel interface
  std::string GetId() const override { return "overworld.gfx_groups"; }
  std::string GetDisplayName() const override { return "Graphics Groups"; }
  std::string GetIcon() const override { return ICON_MD_COLLECTIONS; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_GFX_GROUPS_PANEL_H
