#include "app/editor/overworld/ui/canvas/overworld_canvas_view.h"

#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"

namespace yaze::editor {

void OverworldCanvasView::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;
  ctx.editor->DrawOverworldCanvas();
}

REGISTER_PANEL(OverworldCanvasView);

}  // namespace yaze::editor
