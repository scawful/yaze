#include "app/editor/overworld/panels/map_properties_panel.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

namespace yaze::editor {

void MapPropertiesPanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;
  // Call the existing map properties drawing
  ow_editor->DrawMapProperties();
}

REGISTER_PANEL(MapPropertiesPanel);

}  // namespace yaze::editor
