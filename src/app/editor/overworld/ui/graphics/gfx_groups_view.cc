#include "app/editor/overworld/ui/graphics/gfx_groups_view.h"

#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"
#include "util/log.h"

namespace yaze::editor {

void GfxGroupsView::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  // Delegate to gfx_group_editor Update method
  if (auto status = ctx.editor->UpdateGfxGroupEditor(); !status.ok()) {
    LOG_ERROR("GfxGroupsView", "Failed to update: %s",
              status.ToString().c_str());
  }
}

REGISTER_PANEL(GfxGroupsView);

}  // namespace yaze::editor
