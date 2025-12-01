#include "app/editor/overworld/panels/tile8_selector_panel.h"

#include "app/editor/overworld/overworld_editor.h"

namespace yaze {
namespace editor {

void Tile8SelectorPanel::Draw(bool* p_open) {
  editor_->DrawTile8Selector();
}

}  // namespace editor
}  // namespace yaze
