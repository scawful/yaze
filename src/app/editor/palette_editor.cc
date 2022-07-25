#include "palette_editor.h"

#include <imgui/imgui.h>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "gui/canvas.h"
#include "gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

absl::Status PaletteEditor::Update() {
  for (const auto &name : kPaletteCategoryNames) {
    if (ImGui::TreeNode(name.data())) {

      ImGui::SameLine();
      if (ImGui::SmallButton("button")) {
      }
      ImGui::TreePop();
    }
  }
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze