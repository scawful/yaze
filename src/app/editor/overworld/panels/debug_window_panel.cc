#include "app/editor/overworld/panels/debug_window_panel.h"

#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/debug_window_card.h"

namespace yaze {
namespace editor {

void DebugWindowPanel::Draw(bool* p_open) {
  // Delegate to existing debug_window_card
  if (auto* card = editor_->debug_window_card()) {
    card->Draw(p_open);
  }
}

}  // namespace editor
}  // namespace yaze
