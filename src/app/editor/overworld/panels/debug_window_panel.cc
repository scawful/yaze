#include "app/editor/overworld/panels/debug_window_panel.h"

#include "app/editor/core/content_registry.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/debug_window_card.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze::editor {

void DebugWindowPanel::Draw(bool* p_open) {
  auto* editor = ContentRegistry::Context::current_editor();
  if (!editor) return;

  if (auto* ow_editor = dynamic_cast<OverworldEditor*>(editor)) {
    // Delegate to existing DebugWindowCard
    if (auto* card = ow_editor->debug_window_card()) {
      card->Draw(p_open);
    }
  }
}

REGISTER_PANEL(DebugWindowPanel);

}  // namespace yaze::editor
