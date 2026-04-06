#include "app/editor/overworld/panels/v3_settings_panel.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

namespace yaze::editor {

void V3SettingsPanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;
  ow_editor->DrawV3Settings();
}

REGISTER_PANEL(V3SettingsPanel);

}  // namespace yaze::editor
