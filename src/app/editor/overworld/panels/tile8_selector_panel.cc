#include "app/editor/overworld/panels/tile8_selector_panel.h"

#include "app/editor/core/content_registry.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze::editor {

void Tile8SelectorPanel::Draw(bool* p_open) {
  auto* editor = ContentRegistry::Context::current_editor();
  if (!editor) return;

  if (auto* ow_editor = dynamic_cast<OverworldEditor*>(editor)) {
    ow_editor->DrawTile8Selector();
  }
}

REGISTER_PANEL(Tile8SelectorPanel);

}  // namespace yaze::editor
