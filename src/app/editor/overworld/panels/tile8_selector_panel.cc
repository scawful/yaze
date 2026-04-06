#include "app/editor/overworld/panels/tile8_selector_panel.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

namespace yaze::editor {

void Tile8SelectorPanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;
  ow_editor->DrawTile8Selector();
}

REGISTER_PANEL(Tile8SelectorPanel);

}  // namespace yaze::editor
