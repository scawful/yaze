#include "app/editor/overworld/ui/workspace/scratch_space_view.h"

#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/registry/panel_registration.h"
#include "util/log.h"

namespace yaze::editor {

void ScratchSpaceView::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  // Call the existing DrawScratchSpace implementation
  if (auto status = ctx.editor->DrawScratchSpace(); !status.ok()) {
    LOG_ERROR("ScratchSpaceView", "Failed to draw: %s",
              status.ToString().c_str());
  }
}

REGISTER_PANEL(ScratchSpaceView);

}  // namespace yaze::editor
