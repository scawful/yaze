#include "app/editor/overworld/panels/area_graphics_panel.h"

#include <string>

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

#include "util/log.h"

namespace yaze::editor {

void AreaGraphicsPanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;

  // Log a failure once per distinct message; this runs every frame.
  static std::string last_error;
  if (auto status = ow_editor->DrawAreaGraphics(); !status.ok()) {
    if (status.ToString() != last_error) {
      last_error = status.ToString();
      LOG_ERROR("AreaGraphicsPanel", "Failed to draw: %s", last_error.c_str());
    }
  } else {
    last_error.clear();
  }
}

REGISTER_PANEL(AreaGraphicsPanel);

}  // namespace yaze::editor
