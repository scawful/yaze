#include "app/editor/overworld/ui/graphics/area_graphics_view.h"

#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"
#include "util/log.h"

namespace yaze::editor {

void AreaGraphicsView::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  if (auto status = ctx.editor->DrawAreaGraphics(); !status.ok()) {
    LOG_ERROR("AreaGraphicsView", "Failed to draw: %s",
              status.ToString().c_str());
  }
}

REGISTER_PANEL(AreaGraphicsView);

}  // namespace yaze::editor
