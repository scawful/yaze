#include "app/editor/overworld/panels/gfx_groups_panel.h"

#include "app/editor/core/content_registry.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze::editor {

void GfxGroupsPanel::Draw(bool* p_open) {
  auto* editor = ContentRegistry::Context::current_editor();
  if (!editor) return;

  if (auto* ow_editor = dynamic_cast<OverworldEditor*>(editor)) {
    // Delegate to gfx_group_editor Update method
    if (auto status = ow_editor->UpdateGfxGroupEditor(); !status.ok()) {
      LOG_ERROR("GfxGroupsPanel", "Failed to update: %s",
                status.ToString().c_str());
    }
  }
}

REGISTER_PANEL(GfxGroupsPanel);

}  // namespace yaze::editor
