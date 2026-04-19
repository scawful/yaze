#include "app/editor/overworld/ui/tiles/tile16_editor_view.h"

#include "app/editor/overworld/tile16_editor.h"
#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"
#include "util/log.h"

namespace yaze::editor {

void Tile16EditorView::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  if (auto status = ctx.editor->tile16_editor().UpdateAsPanel(); !status.ok()) {
    LOG_ERROR("Tile16EditorView", "Failed to draw: %s",
              status.ToString().c_str());
  }
}

REGISTER_PANEL(Tile16EditorView);

}  // namespace yaze::editor
