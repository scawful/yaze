#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_SPLIT_BOUNDARY_DRAG_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_SPLIT_BOUNDARY_DRAG_H_

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "app/editor/layout/layout_designer/dock_tree_renderer.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {
namespace layout_designer {

// Minimum/maximum split_ratio enforced by DockTree::Validate. Exposed here
// so the drag path clamps before writing back into the tree.
constexpr float kMinSplitRatio = 0.05f;
constexpr float kMaxSplitRatio = 0.95f;

// Pixel half-width of the gutter hit zone. Matches ImGui's default dock
// splitter feel.
constexpr float kSplitBoundaryTolerance = 4.0f;

struct SplitBoundaryHit {
  // Split node under the mouse, or nullptr on miss.
  const DockNode* split_node = nullptr;
  // true when split axis is X (kLeft/kRight); false for Y.
  bool horizontal = true;
};

// Walks `tree` looking for an expanded split whose gutter line is within
// `tolerance` pixels of `mouse`, with `mouse` also inside the split's
// bounding rect along the non-axis dimension. Returns the first match
// (deeper splits win over shallower — children are visited after the
// current node). Pure geometry; no ImGui context required.
SplitBoundaryHit HitTestSplitBoundary(
    const DockTree& tree, const DockTreeLayout& layout, ImVec2 mouse,
    float tolerance = kSplitBoundaryTolerance);

// Converts a pixel delta along the split's axis into a new split_ratio,
// clamped to [kMinSplitRatio, kMaxSplitRatio]. `start_ratio` is the
// ratio captured at drag start and `axis_size_px` is the span of the
// split's rect along its axis — dividing these out keeps the drag
// numerically stable across frames.
float ComputeDraggedSplitRatio(float start_ratio, float axis_delta_px,
                               float axis_size_px);

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_SPLIT_BOUNDARY_DRAG_H_
