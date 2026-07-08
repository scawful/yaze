#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace yaze {
namespace editor {
namespace layout_designer {

// Stable per-node identity. Allocated at factory time from a process-wide
// monotonic counter so ids are unique across all live DockTrees in a
// session. Survives Clone, JSON round-trip, and structural mutations,
// which is what lets the Layout Designer hold selection by id rather than
// raw `DockNode*`.
//
// Reserve 0 as the "invalid / unset" sentinel — every real node has id
// >= 1.
using DockNodeId = std::uint64_t;
inline constexpr DockNodeId kInvalidDockNodeId = 0;

namespace internal {

// Returns a fresh, never-before-used DockNodeId. Thread-safe.
DockNodeId AllocateDockNodeId();

// Bump the internal counter so subsequent AllocateDockNodeId() calls
// return values strictly greater than `id`. Called by the JSON parser
// for every parsed id so newly-allocated nodes don't collide with
// loaded ones. No-op when `id` is kInvalidDockNodeId or already below
// the current counter.
void ObserveDockNodeId(DockNodeId id);

}  // namespace internal

// Dock split direction. Independent of ImGuiDir so the data model and its
// JSON representation don't require imgui.h; LayoutManager converts at the
// DockBuilder boundary.
enum class SplitDirection : std::uint8_t {
  kLeft,
  kRight,
  kUp,
  kDown,
};

// A single panel referenced by the tree. `panel_id` is the stable id
// registered via REGISTER_PANEL (e.g. "dungeon.room_selector");
// `display_name` + `icon` are cached at capture/authoring time so the
// designer can render the palette/canvas without consulting the live
// panel registry.
struct PanelEntry {
  std::string panel_id;
  std::string display_name;
  std::string icon;
};

// A node in the dock tree. Either a leaf (a tab group of panels) or a
// split (direction + ratio + two children).
struct DockNode {
  enum class Type : std::uint8_t { kLeaf, kSplit };

  // Stable id, allocated at factory time. Default-constructed DockNodes
  // (e.g. via `DockNode n;`) have id == kInvalidDockNodeId; the factories
  // (`MakeLeaf` / `MakeSplit`) and `Clone()` set it to a real value.
  DockNodeId id = kInvalidDockNodeId;

  Type type = Type::kLeaf;

  // kLeaf fields.
  std::vector<PanelEntry> panels;
  int active_tab_index = 0;

  // kSplit fields.
  SplitDirection split_direction = SplitDirection::kLeft;
  float split_ratio = 0.5f;
  std::unique_ptr<DockNode> child_a;  // left / top
  std::unique_ptr<DockNode> child_b;  // right / bottom

  DockNode() = default;

  // Factories (clearer than member-by-member init in callers/tests).
  static std::unique_ptr<DockNode> MakeLeaf(std::vector<PanelEntry> panels);
  static std::unique_ptr<DockNode> MakeSplit(SplitDirection dir, float ratio,
                                             std::unique_ptr<DockNode> a,
                                             std::unique_ptr<DockNode> b);

  // Deep copy.
  std::unique_ptr<DockNode> Clone() const;

  // Turn this leaf into a split. Current panels + active_tab_index migrate
  // to the opposite side; `new_child` occupies the other side.
  // `new_child_first == true` puts it at child_a (left/top). Must be called
  // on a leaf.
  void SplitInPlace(SplitDirection dir, float ratio,
                    std::unique_ptr<DockNode> new_child, bool new_child_first);

  // If this is a split with exactly one non-null child, replace self with
  // that child's contents and return true. No-op otherwise.
  bool PromoteSingleChild();

  // Depth-first search for a panel by id. Returns nullptr when absent.
  const PanelEntry* FindPanel(const std::string& panel_id) const;
};

// A named dock tree ready to be serialized and re-applied.
struct DockTree {
  std::string name;
  std::string description;
  // schema_version 2 added stable per-node `DockNodeId`. v1 JSON (no
  // ids) still parses — the loader allocates fresh ids for nodes that
  // lack them.
  std::uint64_t schema_version = 2;
  std::unique_ptr<DockNode> root;  // Always non-null after construction.

  DockTree();
  explicit DockTree(std::string name);

  DockTree Clone() const;

  // Validate structural invariants. On first failure writes a human-
  // readable message into `*error` (if non-null) and returns false.
  //
  // Checks:
  //   - root is non-null
  //   - every kSplit node has non-null child_a and child_b
  //   - every kSplit's split_ratio is in [0.05, 0.95]
  //   - every kLeaf's active_tab_index is 0 (for empty leaves) or in
  //     [0, panels.size())
  //   - every PanelEntry.panel_id is non-empty and unique across the tree
  bool Validate(std::string* error) const;

  // Linear DFS lookup by stable id. Returns nullptr when `id` is
  // kInvalidDockNodeId or not present in this tree. The non-const
  // overload returns a mutable pointer for callers that need to write
  // back (e.g. the split-boundary drag path on the active node).
  const DockNode* FindNode(DockNodeId id) const;
  DockNode* FindNode(DockNodeId id);
};

// Returns an empty single-leaf tree. Equivalent to `DockTree(name)` but
// reads cleanly at call sites that want "a blank canvas".
DockTree MakeEmptyTree(std::string name = "Untitled");

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_DOCK_TREE_H_
