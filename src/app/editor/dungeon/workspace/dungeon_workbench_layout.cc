#include "app/editor/dungeon/workspace/dungeon_workbench_layout.h"
#include "util/i18n/tr.h"

#include <algorithm>

#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_config.h"

namespace yaze::editor {

namespace {

float ClampWorkbenchPaneWidth(float desired_width, float min_width,
                              float max_width) {
  return std::clamp(desired_width, min_width, std::max(min_width, max_width));
}

}  // namespace

bool DrawDungeonWorkbenchVerticalSplitter(const char* id, float height,
                                          float* pane_width, float min_width,
                                          float max_width,
                                          bool resize_from_left_edge,
                                          float collapse_threshold) {
  if (!pane_width) {
    return false;
  }

  bool collapse_requested = false;
  const float splitter_width = gui::UIConfig::kSplitterWidth;
  const ImVec2 splitter_pos = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, ImVec2(splitter_width, std::max(height, 1.0f)));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  if (hovered || active) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  }
  if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    *pane_width = ClampWorkbenchPaneWidth(*pane_width, min_width, max_width);
  }
  if (active) {
    const float delta = ImGui::GetIO().MouseDelta.x;
    const float proposed =
        resize_from_left_edge ? (*pane_width - delta) : (*pane_width + delta);
    if (proposed < collapse_threshold) {
      collapse_requested = true;
      *pane_width = min_width;
      ImGui::SetTooltip(tr("Collapse pane"));
    } else {
      *pane_width = ClampWorkbenchPaneWidth(proposed, min_width, max_width);
      ImGui::SetTooltip(tr("Width: %.0f px"), *pane_width);
    }
  }

  ImVec4 splitter_color = gui::GetOutlineVec4();
  splitter_color.w = active ? 0.95f : (hovered ? 0.72f : 0.35f);
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(splitter_pos.x + splitter_width * 0.5f, splitter_pos.y),
      ImVec2(splitter_pos.x + splitter_width * 0.5f, splitter_pos.y + height),
      ImGui::GetColorU32(splitter_color), active ? 2.0f : 1.0f);
  return collapse_requested;
}

}  // namespace yaze::editor
