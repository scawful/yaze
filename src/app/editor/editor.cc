#include "editor.h"

#include "app/core/constants.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

absl::Status DrawEditor(EditorLayoutParams *params) {
  if (params->editor == nullptr) {
    return absl::InternalError("Editor is not initialized");
  }

  // Draw the editors based on h_split and v_split in a recursive manner
  if (params->v_split) {
    ImGui::BeginTable("##VerticalSplitTable", 2);

    ImGui::TableNextColumn();
    if (params->left)
      RETURN_IF_ERROR(DrawEditor(params->left));

    ImGui::TableNextColumn();
    if (params->right)
      RETURN_IF_ERROR(DrawEditor(params->right));

    ImGui::EndTable();
  } else if (params->h_split) {
    ImGui::BeginTable("##HorizontalSplitTable", 1);

    ImGui::TableNextColumn();
    if (params->top)
      RETURN_IF_ERROR(DrawEditor(params->top));

    ImGui::TableNextColumn();
    if (params->bottom)
      RETURN_IF_ERROR(DrawEditor(params->bottom));

    ImGui::EndTable();
  } else {
    // No split, just draw the single editor
    ImGui::Text("%s Editor",
                kEditorNames[static_cast<int>(params->editor->type())]);
    RETURN_IF_ERROR(params->editor->Update());
  }

  return absl::OkStatus();
}

} // namespace editor
} // namespace app
} // namespace yaze
