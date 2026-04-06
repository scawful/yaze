#include "app/editor/overworld/panels/scratch_space_panel.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

namespace yaze::editor {

void ScratchSpacePanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;

  // Call the existing DrawScratchSpace implementation
  if (auto status = ow_editor->DrawScratchSpace(); !status.ok()) {
    LOG_ERROR("ScratchSpacePanel", "Failed to draw: %s",
              status.ToString().c_str());
  }
}

REGISTER_PANEL(ScratchSpacePanel);

}  // namespace yaze::editor
