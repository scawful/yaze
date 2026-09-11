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

DungeonWorkbenchToolDrawerLayout ResolveDungeonWorkbenchToolDrawerLayout(
    float total_height, float splitter_height, float preferred_drawer_ratio,
    float min_canvas_height, float min_drawer_height, bool want_drawer) {
  DungeonWorkbenchToolDrawerLayout layout;
  const float safe_total_height = std::max(total_height, 1.0f);
  layout.canvas_height = safe_total_height;
  if (!want_drawer) {
    return layout;
  }

  layout.show_drawer = true;
  const float safe_splitter_height = std::clamp(
      splitter_height, 0.0f, std::max(safe_total_height - 1.0f, 0.0f));
  layout.available_height =
      std::max(safe_total_height - safe_splitter_height, 1.0f);
  const float safe_min_canvas_height = std::max(min_canvas_height, 1.0f);
  const float safe_min_drawer_height = std::max(min_drawer_height, 1.0f);

  layout.compact =
      layout.available_height < safe_min_canvas_height + safe_min_drawer_height;
  if (layout.compact) {
    // Preserve useful space for both surfaces. The tool remains visible even
    // in a short Workbench instead of silently falling back to a side pane.
    layout.min_drawer_height = layout.available_height * 0.4f;
    layout.max_drawer_height = layout.min_drawer_height;
  } else {
    layout.min_drawer_height = safe_min_drawer_height;
    layout.max_drawer_height = layout.available_height - safe_min_canvas_height;
  }

  const float safe_drawer_ratio =
      std::clamp(preferred_drawer_ratio, 0.0f, 1.0f);
  layout.drawer_height =
      std::clamp(layout.available_height * safe_drawer_ratio,
                 layout.min_drawer_height, layout.max_drawer_height);
  layout.canvas_height =
      std::max(layout.available_height - layout.drawer_height, 1.0f);
  return layout;
}

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

bool DrawDungeonWorkbenchHorizontalSplitter(const char* id, float width,
                                            float* pane_height,
                                            float min_height, float max_height,
                                            float collapse_threshold) {
  if (!pane_height) {
    return false;
  }

  bool collapse_requested = false;
  const float splitter_height = gui::UIConfig::kSplitterWidth;
  const ImVec2 splitter_pos = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, ImVec2(std::max(width, 1.0f), splitter_height));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  if (hovered || active) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  }
  if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    *pane_height =
        ClampWorkbenchPaneWidth(*pane_height, min_height, max_height);
  }
  if (active) {
    // The splitter is the bottom pane's top edge. Dragging down shrinks it.
    const float proposed = *pane_height - ImGui::GetIO().MouseDelta.y;
    // Once the pane reaches its minimum, the next downward drag collapses it.
    // A threshold below min_height is otherwise unreachable because every
    // frame clamps pane_height back to min_height.
    const float reachable_collapse_threshold =
        std::max(collapse_threshold, min_height);
    if (proposed < reachable_collapse_threshold) {
      collapse_requested = true;
      *pane_height = min_height;
      ImGui::SetTooltip(tr("Collapse tool drawer"));
    } else {
      *pane_height = ClampWorkbenchPaneWidth(proposed, min_height, max_height);
      ImGui::SetTooltip(tr("Height: %.0f px"), *pane_height);
    }
  }

  ImVec4 splitter_color = gui::GetOutlineVec4();
  splitter_color.w = active ? 0.95f : (hovered ? 0.72f : 0.35f);
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(splitter_pos.x, splitter_pos.y + splitter_height * 0.5f),
      ImVec2(splitter_pos.x + width, splitter_pos.y + splitter_height * 0.5f),
      ImGui::GetColorU32(splitter_color), active ? 2.0f : 1.0f);
  return collapse_requested;
}

}  // namespace yaze::editor
