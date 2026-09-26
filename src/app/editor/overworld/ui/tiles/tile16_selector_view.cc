#include "app/editor/overworld/ui/tiles/tile16_selector_view.h"

#include <string>

#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/overworld/ui/ui_constants.h"
#include "app/editor/registry/panel_registration.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "util/log.h"

namespace yaze::editor {

float Tile16SelectorView::GetPreferredWidth() const {
  gui::TileSelectorWidget::Config config;
  config.tile_size = 16;
  config.display_scale = kTile16SelectorScale;
  config.tiles_per_row = kTile16SelectorColumns;
  config.draw_offset = ImVec2(kTile16SelectorDrawOffsetX, 0.0f);
  return gui::TileSelectorWidget::PreferredViewportWidth(
             config, gui::TileSelectorWidget::CurrentScrollbarSize()) +
         2.0f * gui::PanelWindow::kWindowPadding;
}

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
