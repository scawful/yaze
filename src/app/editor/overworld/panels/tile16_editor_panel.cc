#include "app/editor/overworld/panels/tile16_editor_panel.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

namespace yaze::editor {

void Tile16EditorPanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;

  if (auto status = ow_editor->tile16_editor().UpdateAsPanel(); !status.ok()) {
    LOG_ERROR("Tile16EditorPanel", "Failed to draw: %s",
              status.ToString().c_str());
  }
}

REGISTER_PANEL(Tile16EditorPanel);

}  // namespace yaze::editor
