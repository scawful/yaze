#include "app/editor/overworld/panels/tile16_selector_panel.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

namespace yaze::editor {

void Tile16SelectorPanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;

  if (auto status = ow_editor->DrawTile16Selector(); !status.ok()) {
    LOG_ERROR("Tile16SelectorPanel", "Failed to draw: %s",
              status.ToString().c_str());
  }
}

REGISTER_PANEL(Tile16SelectorPanel);

}  // namespace yaze::editor
