#include "app/editor/layout/layout_designer/split_boundary_drag.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

bool IsHorizontalSplit(SplitDirection d) {
  return d == SplitDirection::kLeft || d == SplitDirection::kRight;
}

}  // namespace

SplitBoundaryHit HitTestSplitBoundary(const DockTree& tree,
                                      const DockTreeLayout& layout,
                                      ImVec2 mouse, float tolerance) {
  SplitBoundaryHit result;

  std::function<bool(const DockNode*)> walk = [&](const DockNode* node) {
    if (node == nullptr || node->type != DockNode::Type::kSplit)
      return false;
    const auto split_it = layout.node_rects.find(node);
    if (split_it == layout.node_rects.end())
      return false;

    const DockNode* a = node->child_a.get();
    const DockNode* b = node->child_b.get();
    if (a == nullptr || b == nullptr)
      return false;

    const auto a_it = layout.node_rects.find(a);
    const auto b_it = layout.node_rects.find(b);
    if (a_it == layout.node_rects.end() || b_it == layout.node_rects.end()) {
      // Collapsed split — no interactive gutter.
      return false;
    }

    // Recurse first so deeper gutters win over an ancestor's when they
    // overlap (rare but possible at the tolerance band).
    if (walk(a) || walk(b))
      return true;

    const ImRect& split_rect = split_it->second;
    const ImRect& a_rect = a_it->second;
    const bool horizontal = IsHorizontalSplit(node->split_direction);
    if (horizontal) {
      const float gutter_x = a_rect.Max.x;
      if (std::abs(mouse.x - gutter_x) <= tolerance &&
          mouse.y >= split_rect.Min.y && mouse.y <= split_rect.Max.y) {
        result.split_node = node;
        result.horizontal = true;
        return true;
      }
    } else {
      const float gutter_y = a_rect.Max.y;
      if (std::abs(mouse.y - gutter_y) <= tolerance &&
          mouse.x >= split_rect.Min.x && mouse.x <= split_rect.Max.x) {
        result.split_node = node;
        result.horizontal = false;
        return true;
      }
    }
    return false;
  };

  walk(tree.root.get());
  return result;
}

float ComputeDraggedSplitRatio(float start_ratio, float axis_delta_px,
                               float axis_size_px) {
  if (axis_size_px <= 0.0f) {
    return std::clamp(start_ratio, kMinSplitRatio, kMaxSplitRatio);
  }
  const float new_ratio = start_ratio + axis_delta_px / axis_size_px;
  return std::clamp(new_ratio, kMinSplitRatio, kMaxSplitRatio);
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
