#include "app/editor/overworld/ui/tiles/tile16_selector_view.h"

#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"
#include "util/log.h"

namespace yaze::editor {

void Tile16SelectorView::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  if (auto status = ctx.editor->DrawTile16Selector(); !status.ok()) {
    LOG_ERROR("Tile16SelectorView", "Failed to draw: %s",
              status.ToString().c_str());
  }
}

REGISTER_PANEL(Tile16SelectorView);

}  // namespace yaze::editor
