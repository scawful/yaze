#include "app/editor/overworld/ui/tiles/tile16_editor_view.h"

#include <string>

#include "app/editor/overworld/tile16_editor.h"
#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"
#include "app/gui/widgets/empty_state.h"
#include "util/log.h"

namespace yaze::editor {

void Tile16EditorView::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx || !ctx.editor->IsRomLoaded()) {
    gui::DrawEmptyState(gui::EmptyNoRom());
    return;
  }

  // Log a failure once per distinct message; this runs every frame.
  static std::string last_error;
  if (auto status = ctx.editor->tile16_editor().UpdateAsPanel(); !status.ok()) {
    if (status.ToString() != last_error) {
      last_error = status.ToString();
      LOG_ERROR("Tile16EditorView", "Failed to draw: %s", last_error.c_str());
    }
  } else {
    last_error.clear();
  }
}

REGISTER_PANEL(Tile16EditorView);

}  // namespace yaze::editor
