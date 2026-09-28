#include "app/editor/overworld/panels/scratch_space_panel.h"

#include <string>

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

#include "util/log.h"

namespace yaze::editor {

void ScratchSpacePanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor)
    return;

  // Call the existing DrawScratchSpace implementation
  // Log a failure once per distinct message; this runs every frame.
  static std::string last_error;
  if (auto status = ow_editor->DrawScratchSpace(); !status.ok()) {
    if (status.ToString() != last_error) {
      last_error = status.ToString();
      LOG_ERROR("ScratchSpacePanel", "Failed to draw: %s", last_error.c_str());
    }
  } else {
    last_error.clear();
  }
}

REGISTER_PANEL(ScratchSpacePanel);

}  // namespace yaze::editor
