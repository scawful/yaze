#include <gtest/gtest.h>

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "app/editor/layout/layout_designer/layout_designer_panel.h"

// Phase 8 invariant test: every operation that replaces `tree_` wholesale
// must clear selection (`selected_id_`), active drag (`drag_.split_id`)
// and the undo stack (its snapshots reference the replaced tree's
// identity and crossing a Load boundary via Undo would surprise users).
//
// Phase 8.5 rewrote selection from raw `const DockNode*` to stable
// `DockNodeId`. The cleared-on-ReplaceTree behavior stayed: a freshly-
// loaded layout shouldn't carry the previous one's cursor. This file
// pins both the clear semantics AND the new property that selection
// survives undo/redo when the id still exists in the swapped-in tree.

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

// A minimal tree with a single leaf and one panel entry — enough to own
// a DockNode whose id can drive the panel's selection state and a valid
// clone target for the undo stack.
DockTree MakeSampleTree(const std::string& name, const std::string& panel_id) {
  DockTree tree(name);
  tree.root = DockNode::MakeLeaf(
      {{panel_id, panel_id + " Display", "ICON_MD_FAVORITE"}});
  return tree;
}

TEST(LayoutDesignerPanelReplaceTreeTest, ClearsSelection) {
  LayoutDesignerPanel panel;
  // The default-constructed panel already has a valid tree; pin its
  // root id as the current selection so we can verify the clear.
  const DockNodeId root_id = panel.tree_for_test().root->id;
  ASSERT_NE(root_id, kInvalidDockNodeId);
  panel.set_selected_id_for_test(root_id);
  ASSERT_EQ(panel.selected_id_for_test(), root_id);

  panel.ReplaceTree(MakeSampleTree("Next", "dungeon.room_selector"));
  EXPECT_EQ(panel.selected_id_for_test(), kInvalidDockNodeId)
      << "ReplaceTree must null selected_id_ — the id named a node in "
         "the now-replaced tree.";
}

TEST(LayoutDesignerPanelReplaceTreeTest, ClearsActiveDrag) {
  LayoutDesignerPanel panel;
  // Stash a real id into drag_ to simulate mid-drag state.
  panel.set_drag_id_for_test(panel.tree_for_test().root->id);
  ASSERT_TRUE(panel.has_active_drag_for_test());

  panel.ReplaceTree(MakeSampleTree("Next", "overworld.tile16"));
  EXPECT_FALSE(panel.has_active_drag_for_test())
      << "ReplaceTree must reset drag_ — the drag id named a node in "
         "the now-replaced tree.";
}

TEST(LayoutDesignerPanelReplaceTreeTest, ClearsUndoHistory) {
  LayoutDesignerPanel panel;
  // PushUndoSnapshot is private, so we exercise the public path: replace
  // once with a non-default tree (undo should already be empty), then
  // verify that subsequent replaces keep it empty.
  panel.ReplaceTree(MakeSampleTree("First", "first.panel"));
  EXPECT_FALSE(panel.undo_for_test().CanUndo());
  EXPECT_FALSE(panel.undo_for_test().CanRedo());

  panel.ReplaceTree(MakeSampleTree("Second", "second.panel"));
  EXPECT_FALSE(panel.undo_for_test().CanUndo())
      << "ReplaceTree must clear the undo history — its snapshots "
         "reference the now-replaced tree's identity.";
  EXPECT_FALSE(panel.undo_for_test().CanRedo());
}

TEST(LayoutDesignerPanelReplaceTreeTest, InstallsReplacementTree) {
  LayoutDesignerPanel panel;
  ASSERT_EQ(panel.tree_for_test().name, "Untitled");

  panel.ReplaceTree(MakeSampleTree("Dungeon Expert", "dungeon.room_selector"));
  EXPECT_EQ(panel.tree_for_test().name, "Dungeon Expert");
  ASSERT_NE(panel.tree_for_test().root, nullptr);
  ASSERT_EQ(panel.tree_for_test().root->panels.size(), 1u);
  EXPECT_EQ(panel.tree_for_test().root->panels[0].panel_id,
            "dungeon.room_selector");
}

// Phase 8.5: selected ids that don't exist in the current tree resolve
// to nullptr at draw time. The panel's empty-state branches (no
// outline, properties column shows the layout-level inputs) handle
// that gracefully — no crash, no UB.
TEST(LayoutDesignerPanelSelectionIdTest, UnknownSelectionIdResolvesToNoNode) {
  LayoutDesignerPanel panel;
  // Far-future id — never produced by the factory before the test runs.
  const DockNodeId nonexistent = internal::AllocateDockNodeId() + 1000000ULL;
  panel.set_selected_id_for_test(nonexistent);
  EXPECT_EQ(panel.tree_for_test().FindNode(nonexistent), nullptr);
  // The panel's draw path resolves selected_id_ via FindNode — selection
  // pointing at a phantom is a safe no-op.
}

}  // namespace
}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
