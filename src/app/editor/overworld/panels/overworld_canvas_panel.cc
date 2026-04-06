#include "app/editor/overworld/panels/overworld_canvas_panel.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

namespace yaze::editor {

void OverworldCanvasPanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;
  ow_editor->DrawOverworldCanvas();
}

REGISTER_PANEL(OverworldCanvasPanel);

}  // namespace yaze::editor
