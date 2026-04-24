#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DROP_ZONE_SUGGESTER_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DROP_ZONE_SUGGESTER_H_

#include <cstdint>

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace editor {
namespace layout_designer {

// Which way the mouse-over-a-leaf maps to a tree mutation.
// Edge bands take the outer ~30% of each axis; the inner region is tab.
struct DropSuggestion {
  enum class Kind : std::uint8_t {
    kNone,
    kTab,          // append to leaf's panels, make active.
    kSplitLeft,    // new panel takes left 30%, existing leaf occupies right.
    kSplitRight,   // new panel takes right 30%, existing occupies left.
    kSplitTop,     // new panel takes top 30%, existing occupies bottom.
    kSplitBottom,  // new panel takes bottom 30%, existing occupies top.
  };
  Kind kind = Kind::kNone;
};

// Edge band as a fraction of rect width/height. Mouse positions whose
// relative distance to the nearest edge falls below this threshold become
// split suggestions; all other interior positions become tabs.
constexpr float kDropEdgeFraction = 0.3f;

// Split ratio applied when the suggestion creates a new leaf. Using a
// smaller value keeps the existing leaf (which usually holds more user
// state) dominant after the split.
constexpr float kDropSplitRatio = 0.3f;

// Pure geometry: classify `mouse` within `leaf_rect`. Returns kNone when
// `mouse` is outside the rect or the rect is degenerate.
DropSuggestion SuggestDrop(const ImRect& leaf_rect, ImVec2 mouse);

// Returns the rect the new panel would occupy if the suggestion applied.
// For kTab the full leaf_rect is returned (tabs paint atop the leaf). For
// kNone the returned rect is empty (Min == Max == leaf_rect.Min).
ImRect ComputeDropPreviewRect(const ImRect& leaf_rect,
                              const DropSuggestion& suggestion);

// Apply `suggestion` to `tree`, using `leaf` as the drop target. Returns
// true on success; false (no mutation) when:
//   - tree or leaf is null,
//   - suggestion is kNone,
//   - leaf is not a kLeaf node (collapsed splits render like leaves but
//     still reject drops — the user must grow the split before dropping
//     into it),
//   - panel.panel_id is empty, or
//   - panel.panel_id is already present elsewhere in the tree (DockTree
//     invariant requires unique panel_ids).
bool ApplyDropSuggestion(DockTree* tree, DockNode* leaf,
                         const DropSuggestion& suggestion, PanelEntry panel);

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DROP_ZONE_SUGGESTER_H_
