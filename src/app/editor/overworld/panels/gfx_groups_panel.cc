#include "app/editor/overworld/panels/gfx_groups_panel.h"

#include "app/editor/overworld/overworld_editor.h"

namespace yaze {
namespace editor {

void GfxGroupsPanel::Draw(bool* p_open) {
  // Delegate to gfx_group_editor Update method
  if (auto status = editor_->UpdateGfxGroupEditor(); !status.ok()) {
    LOG_ERROR("GfxGroupsPanel", "Failed to update: %s", 
              status.ToString().c_str());
  }
}

}  // namespace editor
}  // namespace yaze
