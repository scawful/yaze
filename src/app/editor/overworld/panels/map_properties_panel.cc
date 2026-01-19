#include "app/editor/overworld/panels/map_properties_panel.h"

#include "app/editor/core/content_registry.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze::editor {

void MapPropertiesPanel::Draw(bool* p_open) {
  auto* editor = ContentRegistry::Context::current_editor();
  if (!editor) return;

  if (auto* ow_editor = dynamic_cast<OverworldEditor*>(editor)) {
    // Call the existing map properties drawing
    ow_editor->DrawMapProperties();
  }
}

REGISTER_PANEL(MapPropertiesPanel);

}  // namespace yaze::editor
