#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_RENDERER_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_RENDERER_H_

#include "absl/container/flat_hash_map.h"
#include "app/editor/layout/layout_designer/dock_tree.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace editor {
namespace layout_designer {

// Minimum legible cell size (pixels). When a split would produce a child
// below this threshold along the split axis, the split collapses: the
// split node's rect is still recorded but its children are not, and the
// renderer paints the split as a single solid cell.
constexpr float kMinCellSize = 20.0f;

struct DockTreeLayout {
  absl::flat_hash_map<const DockNode*, ImRect> node_rects;
};

// Pure geometry: walk `tree`, assign each DockNode an ImRect carved out of
// `viewport` via split_ratio along the split axis (kLeft/kRight are X,
// kUp/kDown are Y). child_a always occupies the left/top subrect,
// child_b the right/bottom, matching DockNode's documented layout.
DockTreeLayout ComputeLayout(const DockTree& tree, const ImRect& viewport);

// Draw the tree into `dl`. Leaves paint a themed background + border and
// (space permitting) the active panel's display name. Splits draw a
// single-pixel gutter between children. A non-null `selected` that is
// present in `layout` gets a thicker accent-colored overlay outline.
void RenderDockTree(const DockTree& tree, const DockTreeLayout& layout,
                    const DockNode* selected, ImDrawList* dl);

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_RENDERER_H_
