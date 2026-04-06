#include "app/editor/overworld/panels/gfx_groups_panel.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

namespace yaze::editor {

void GfxGroupsPanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;

  // Delegate to gfx_group_editor Update method
  if (auto status = ow_editor->UpdateGfxGroupEditor(); !status.ok()) {
    LOG_ERROR("GfxGroupsPanel", "Failed to update: %s",
              status.ToString().c_str());
  }
}

REGISTER_PANEL(GfxGroupsPanel);

}  // namespace yaze::editor
