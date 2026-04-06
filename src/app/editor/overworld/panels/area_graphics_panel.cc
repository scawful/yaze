#include "app/editor/overworld/panels/area_graphics_panel.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

namespace yaze::editor {

void AreaGraphicsPanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;

  if (auto status = ow_editor->DrawAreaGraphics(); !status.ok()) {
    LOG_ERROR("AreaGraphicsPanel", "Failed to draw: %s",
              status.ToString().c_str());
  }
}

REGISTER_PANEL(AreaGraphicsPanel);

}  // namespace yaze::editor
