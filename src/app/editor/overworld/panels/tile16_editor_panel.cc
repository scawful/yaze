#include "app/editor/overworld/panels/tile16_editor_panel.h"

#include "app/editor/overworld/tile16_editor.h"
#include "util/log.h"

namespace yaze {
namespace editor {

void Tile16EditorPanel::Draw(bool* p_open) {
  // Call the panel-friendly update method (no MenuBar required)
  if (auto status = editor_->UpdateAsPanel(); !status.ok()) {
    LOG_ERROR("Tile16EditorPanel", "Failed to draw: %s",
              status.ToString().c_str());
  }
}

}  // namespace editor
}  // namespace yaze
