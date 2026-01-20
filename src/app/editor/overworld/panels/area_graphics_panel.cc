#include "app/editor/overworld/panels/area_graphics_panel.h"

#include "app/editor/core/content_registry.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze::editor {

void AreaGraphicsPanel::Draw(bool* p_open) {
  auto* editor = ContentRegistry::Context::current_editor();
  if (!editor) return;

  if (auto* ow_editor = dynamic_cast<OverworldEditor*>(editor)) {
    if (auto status = ow_editor->DrawAreaGraphics(); !status.ok()) {
      LOG_ERROR("AreaGraphicsPanel", "Failed to draw: %s",
                status.ToString().c_str());
    }
  }
}

REGISTER_PANEL(AreaGraphicsPanel);

}  // namespace yaze::editor
