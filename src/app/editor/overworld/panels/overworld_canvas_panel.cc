#include "app/editor/overworld/panels/overworld_canvas_panel.h"

#include "app/editor/overworld/overworld_editor.h"

namespace yaze {
namespace editor {

void OverworldCanvasPanel::Draw(bool* p_open) {
  if (editor_) {
    editor_->DrawOverworldCanvas();
  }
}

}  // namespace editor
}  // namespace yaze
