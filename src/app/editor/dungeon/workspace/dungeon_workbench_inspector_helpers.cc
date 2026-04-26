#include "app/editor/dungeon/workspace/dungeon_workbench_inspector_helpers.h"

#include <algorithm>

#include "app/gui/core/style_guard.h"

namespace yaze::editor::workbench {

void DrawInspectorSectionHeader(const char* label) {
  ImGui::SeparatorText(label);
}

bool BeginInspectorSection(const char* label, bool default_open) {
  gui::StyleVarGuard frame_padding_guard(
      ImGuiStyleVar_FramePadding,
      ImVec2(ImGui::GetStyle().FramePadding.x,
             std::max(5.0f, ImGui::GetStyle().FramePadding.y + 1.0f)));
  return ImGui::CollapsingHeader(
      label, default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}

bool DrawActionButton(const char* label, const ImVec2& size) {
  gui::StyleVarGuard align_guard(ImGuiStyleVar_ButtonTextAlign,
                                 ImVec2(0.08f, 0.5f));
  return ImGui::Button(label, size);
}

}  // namespace yaze::editor::workbench
