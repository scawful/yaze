#include "app/editor/overworld/panels/debug_window_panel.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/debug_window_card.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

namespace yaze::editor {

void DebugWindowPanel::Draw(bool* p_open) {
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;

  // Delegate to existing DebugWindowCard
  if (auto* card = ow_editor->debug_window_card()) {
    card->Draw(p_open);
  }
}

REGISTER_PANEL(DebugWindowPanel);

}  // namespace yaze::editor
