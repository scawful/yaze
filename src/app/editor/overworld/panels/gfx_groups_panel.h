#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_GFX_GROUPS_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_GFX_GROUPS_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

class OverworldEditor;

/**
 * @class GfxGroupsPanel
 * @brief Graphics group configuration editor
 */
class GfxGroupsPanel : public EditorPanel {
 public:
  explicit GfxGroupsPanel(OverworldEditor* editor) : editor_(editor) {}

  // EditorPanel interface
  std::string GetId() const override { return "overworld.gfx_groups"; }
  std::string GetDisplayName() const override { return "Graphics Groups"; }
  std::string GetIcon() const override { return ICON_MD_COLLECTIONS; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;

 private:
  OverworldEditor* editor_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_GFX_GROUPS_PANEL_H
