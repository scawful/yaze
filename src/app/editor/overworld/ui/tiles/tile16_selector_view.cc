#include "app/editor/overworld/ui/tiles/tile16_selector_view.h"

#include <string>

#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"
#include "util/log.h"

namespace yaze::editor {

void Tile16SelectorView::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  // Log a failure once per distinct message; this runs every frame.
  static std::string last_error;
  if (auto status = ctx.editor->DrawTile16Selector(); !status.ok()) {
    if (status.ToString() != last_error) {
      last_error = status.ToString();
      LOG_ERROR("Tile16SelectorView", "Failed to draw: %s", last_error.c_str());
    }
  } else {
    last_error.clear();
  }
}

REGISTER_PANEL(Tile16SelectorView);

}  // namespace yaze::editor
