#include "app/editor/overworld/ui/properties/v3_settings_view.h"

#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"

namespace yaze::editor {

void V3SettingsView::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;
  ctx.editor->DrawV3Settings();
}

REGISTER_PANEL(V3SettingsView);

}  // namespace yaze::editor
