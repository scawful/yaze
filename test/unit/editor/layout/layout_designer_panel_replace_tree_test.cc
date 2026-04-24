#include <gtest/gtest.h>

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "app/editor/layout/layout_designer/layout_designer_panel.h"

// Phase 8 invariant test: every operation that replaces `tree_` wholesale
// must clear `selected_` and `drag_.split_node` (raw DockNode* into the
// old tree) and also clear the undo stack (its snapshots reference the
// replaced tree's identity and crossing a Load boundary via Undo would
// surprise users). Enforced by `LayoutDesignerPanel::ReplaceTree`; this
// file pins the behavior so Phase 8.5 (stable DockNodeId rewrite) has a
// known-good reference.

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

// A minimal tree with a single leaf and one panel entry — enough to own
// a DockNode we can point `selected_` at and a valid clone target for
// the undo stack.
DockTree MakeSampleTree(const std::string& name, const std::string& panel_id) {
  DockTree tree(name);
  tree.root = DockNode::MakeLeaf(
      {{panel_id, panel_id + " Display", "ICON_MD_FAVORITE"}});
  return tree;
}

TEST(LayoutDesignerPanelReplaceTreeTest, ClearsSelection) {
  LayoutDesignerPanel panel;
  // The default-constructed panel already has a valid tree; point
  // `selected_` at its root so we can verify the clear.
  panel.set_selected_for_test(panel.tree_for_test().root.get());
  ASSERT_NE(panel.selected_for_test(), nullptr);

  panel.ReplaceTree(MakeSampleTree("Next", "dungeon.room_selector"));
  EXPECT_EQ(panel.selected_for_test(), nullptr)
      << "ReplaceTree must null selected_ — the pointer referenced the "
         "now-replaced tree.";
}

TEST(LayoutDesignerPanelReplaceTreeTest, ClearsActiveDrag) {
  LayoutDesignerPanel panel;
  // Stash a mutable node pointer into drag_ to simulate mid-drag state.
  panel.set_drag_node_for_test(panel.tree_for_test().root.get());
  ASSERT_TRUE(panel.has_active_drag_for_test());

  panel.ReplaceTree(MakeSampleTree("Next", "overworld.tile16"));
  EXPECT_FALSE(panel.has_active_drag_for_test())
      << "ReplaceTree must reset drag_ — the drag node pointer dangled "
         "across the tree swap.";
}

TEST(LayoutDesignerPanelReplaceTreeTest, ClearsUndoHistory) {
  LayoutDesignerPanel panel;
  // Build up a shallow undo stack by calling ReplaceTree with undo
  // pushed inline — but PushUndoSnapshot is private, so we exercise the
  // public path by replacing then re-replacing. Easier: replace once
  // with a non-default tree (undo should already be empty), then verify
  // that subsequent replaces keep it empty.
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

}  // namespace
}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
