#include "app/editor/overworld/panels/tile16_selector_panel.h"

#include "absl/status/status.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze {
namespace editor {

void Tile16SelectorPanel::Draw(bool* p_open) {
  // Call the existing DrawTile16Selector implementation
  if (auto status = editor_->DrawTile16Selector(); !status.ok()) {
    // Log error but don't crash the panel
    LOG_ERROR("Tile16SelectorPanel", "Failed to draw: %s", 
              status.ToString().c_str());
  }
}

}  // namespace editor
}  // namespace yaze
