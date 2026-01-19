#include "app/editor/overworld/panels/v3_settings_panel.h"

#include "app/editor/core/content_registry.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze::editor {

void V3SettingsPanel::Draw(bool* p_open) {
  auto* editor = ContentRegistry::Context::current_editor();
  if (!editor) return;

  if (auto* ow_editor = dynamic_cast<OverworldEditor*>(editor)) {
    ow_editor->DrawV3Settings();
  }
}

REGISTER_PANEL(V3SettingsPanel);

}  // namespace yaze::editor
