#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_HIT_TEST_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_HIT_TEST_H_

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "app/editor/layout/layout_designer/dock_tree_renderer.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {
namespace layout_designer {

// Returns the deepest node drawn at `mouse`, or nullptr when `mouse` is
// outside the laid-out viewport. "Deepest drawn" matches the renderer's
// cell-or-collapsed-split semantics: leaves and collapsed splits are
// terminal, expanded splits are transparent to the hit test.
//
// Implemented as min-area over the layout map — `ComputeLayout` records
// ancestor rects that strictly contain their descendants, so the smallest
// rect containing `mouse` is the deepest drawn cell. Pure geometry; no
// ImGui context required.
const DockNode* HitTestNode(const DockTreeLayout& layout, ImVec2 mouse);

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_HIT_TEST_H_
