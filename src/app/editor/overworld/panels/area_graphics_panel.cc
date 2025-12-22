#include "app/editor/overworld/panels/area_graphics_panel.h"

#include "absl/status/status.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze {
namespace editor {

void AreaGraphicsPanel::Draw(bool* p_open) {
  // Call the existing DrawAreaGraphics implementation
  // This delegates to OverworldEditor's method which handles all the logic
  if (auto status = editor_->DrawAreaGraphics(); !status.ok()) {
    // Log error but don't crash the panel
    LOG_ERROR("AreaGraphicsPanel", "Failed to draw: %s", 
              status.ToString().c_str());
  }
}

}  // namespace editor
}  // namespace yaze
