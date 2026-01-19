#include "app/editor/overworld/panels/scratch_space_panel.h"

#include "app/editor/core/content_registry.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze::editor {

void ScratchSpacePanel::Draw(bool* p_open) {
  auto* editor = ContentRegistry::Context::current_editor();
  if (!editor) return;

  if (auto* ow_editor = dynamic_cast<OverworldEditor*>(editor)) {
    // Call the existing DrawScratchSpace implementation
    if (auto status = ow_editor->DrawScratchSpace(); !status.ok()) {
      LOG_ERROR("ScratchSpacePanel", "Failed to draw: %s",
                status.ToString().c_str());
    }
  }
}

REGISTER_PANEL(ScratchSpacePanel);

}  // namespace yaze::editor
