#include "app/editor/layout/layout_designer/dock_tree_hit_test.h"

namespace yaze {
namespace editor {
namespace layout_designer {

const DockNode* HitTestNode(const DockTreeLayout& layout, ImVec2 mouse) {
  const DockNode* best = nullptr;
  float best_area = 0.0f;
  for (const auto& entry : layout.node_rects) {
    const ImRect& rect = entry.second;
    if (!rect.Contains(mouse))
      continue;
    const float area = rect.GetWidth() * rect.GetHeight();
    if (best == nullptr || area < best_area) {
      best = entry.first;
      best_area = area;
    }
  }
  return best;
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
