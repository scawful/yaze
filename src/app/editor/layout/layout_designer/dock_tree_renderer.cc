#include "app/editor/layout/layout_designer/dock_tree_renderer.h"

#include <algorithm>
#include <functional>
#include <string>

#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

bool IsHorizontalSplit(SplitDirection d) {
  return d == SplitDirection::kLeft || d == SplitDirection::kRight;
}

void LayoutNodeRecursive(const DockNode* node, ImRect rect,
                         DockTreeLayout* out) {
  if (node == nullptr)
    return;
  out->node_rects[node] = rect;
  if (node->type != DockNode::Type::kSplit)
    return;
  if (node->child_a == nullptr || node->child_b == nullptr)
    return;

  const float width = rect.GetWidth();
  const float height = rect.GetHeight();
  const bool horizontal = IsHorizontalSplit(node->split_direction);
  const float axis_size = horizontal ? width : height;
  const float child_a_size = axis_size * node->split_ratio;
  const float child_b_size = axis_size - child_a_size;

  if (child_a_size < kMinCellSize || child_b_size < kMinCellSize) {
    return;
  }

  ImRect r_a;
  ImRect r_b;
  if (horizontal) {
    const float split_x = rect.Min.x + child_a_size;
    r_a = ImRect(rect.Min.x, rect.Min.y, split_x, rect.Max.y);
    r_b = ImRect(split_x, rect.Min.y, rect.Max.x, rect.Max.y);
  } else {
    const float split_y = rect.Min.y + child_a_size;
    r_a = ImRect(rect.Min.x, rect.Min.y, rect.Max.x, split_y);
    r_b = ImRect(rect.Min.x, split_y, rect.Max.x, rect.Max.y);
  }

  LayoutNodeRecursive(node->child_a.get(), r_a, out);
  LayoutNodeRecursive(node->child_b.get(), r_b, out);
}

}  // namespace

DockTreeLayout ComputeLayout(const DockTree& tree, const ImRect& viewport) {
  DockTreeLayout layout;
  LayoutNodeRecursive(tree.root.get(), viewport, &layout);
  return layout;
}

void RenderDockTree(const DockTree& tree, const DockTreeLayout& layout,
                    const DockNode* selected, ImDrawList* dl) {
  if (dl == nullptr || tree.root == nullptr)
    return;

  const ImU32 cell_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
  const ImU32 cell_border = ImGui::GetColorU32(ImGuiCol_Border);
  const ImU32 split_gutter = cell_border;
  const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);
  const ImU32 selection = ImGui::ColorConvertFloat4ToU32(gui::GetAccentColor());

  std::function<void(const DockNode*)> draw = [&](const DockNode* node) {
    if (node == nullptr)
      return;
    const auto it = layout.node_rects.find(node);
    if (it == layout.node_rects.end())
      return;
    const ImRect& rect = it->second;

    const bool is_split = node->type == DockNode::Type::kSplit;
    const bool children_expanded =
        is_split && node->child_a != nullptr && node->child_b != nullptr &&
        layout.node_rects.count(node->child_a.get()) > 0 &&
        layout.node_rects.count(node->child_b.get()) > 0;

    if (!is_split || !children_expanded) {
      dl->AddRectFilled(rect.Min, rect.Max, cell_bg);
      dl->AddRect(rect.Min, rect.Max, cell_border);

      std::string label;
      if (node->type == DockNode::Type::kLeaf && !node->panels.empty()) {
        const int idx = std::clamp(node->active_tab_index, 0,
                                   static_cast<int>(node->panels.size()) - 1);
        label = node->panels[idx].display_name;
      } else if (is_split) {
        label = "(collapsed split)";
      }

      if (!label.empty() && rect.GetWidth() > 40.0f &&
          rect.GetHeight() > 20.0f) {
        dl->AddText(ImVec2(rect.Min.x + 6.0f, rect.Min.y + 6.0f), text_color,
                    label.c_str());
      }
      return;
    }

    const ImRect& a_rect = layout.node_rects.at(node->child_a.get());
    if (IsHorizontalSplit(node->split_direction)) {
      dl->AddLine(ImVec2(a_rect.Max.x, rect.Min.y),
                  ImVec2(a_rect.Max.x, rect.Max.y), split_gutter);
    } else {
      dl->AddLine(ImVec2(rect.Min.x, a_rect.Max.y),
                  ImVec2(rect.Max.x, a_rect.Max.y), split_gutter);
    }
    draw(node->child_a.get());
    draw(node->child_b.get());
  };

  draw(tree.root.get());

  if (selected != nullptr) {
    const auto it = layout.node_rects.find(selected);
    if (it != layout.node_rects.end()) {
      dl->AddRect(it->second.Min, it->second.Max, selection, 0.0f, 0, 2.5f);
    }
  }
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
