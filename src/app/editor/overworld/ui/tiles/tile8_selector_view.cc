#include "app/editor/overworld/ui/tiles/tile8_selector_view.h"

#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"

namespace yaze::editor {

void Tile8SelectorView::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;
  ctx.editor->DrawTile8Selector();
}

REGISTER_PANEL(Tile8SelectorView);

}  // namespace yaze::editor
