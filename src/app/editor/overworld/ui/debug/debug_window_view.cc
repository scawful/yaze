#include "app/editor/overworld/ui/debug/debug_window_view.h"

#include "app/editor/overworld/debug_window_card.h"
#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"

namespace yaze::editor {

void DebugWindowView::Draw(bool* p_open) {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  // Delegate to existing DebugWindowCard
  if (auto* card = ctx.editor->debug_window_card()) {
    card->Draw(p_open);
  }
}

REGISTER_PANEL(DebugWindowView);

}  // namespace yaze::editor
