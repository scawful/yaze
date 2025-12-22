#include "app/editor/overworld/panels/scratch_space_panel.h"

#include "app/editor/overworld/overworld_editor.h"

namespace yaze {
namespace editor {

void ScratchSpacePanel::Draw(bool* p_open) {
  // Call the existing DrawScratchSpace implementation
  if (auto status = editor_->DrawScratchSpace(); !status.ok()) {
    LOG_ERROR("ScratchSpacePanel", "Failed to draw: %s", 
              status.ToString().c_str());
  }
}

}  // namespace editor
}  // namespace yaze
