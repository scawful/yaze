#include "app/editor/overworld/ui/properties/map_properties_view.h"

#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"

namespace yaze::editor {

void MapPropertiesView::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;
  ctx.editor->DrawMapProperties();
}

REGISTER_PANEL(MapPropertiesView);

}  // namespace yaze::editor
