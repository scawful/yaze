#include "app/editor/layout/layout_designer/tree_undo_stack.h"

#include <string>

#include <gtest/gtest.h>

#include "app/editor/layout/layout_designer/dock_tree.h"

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

PanelEntry MakePanel(const std::string& id) {
  return {id, id + " Display", "ICON_MD_FAVORITE"};
}

DockTree MakeTreeWith(const std::string& panel_id) {
  DockTree tree(panel_id + ".tree");
  tree.root = DockNode::MakeLeaf({MakePanel(panel_id)});
  return tree;
}

TEST(TreeUndoStackTest, CanUndoFalseUntilFirstPush) {
  TreeUndoStack stack;
  EXPECT_FALSE(stack.CanUndo());
  EXPECT_FALSE(stack.CanRedo());

  DockTree current = MakeTreeWith("a");
  stack.Push(current);
  EXPECT_TRUE(stack.CanUndo());
  EXPECT_FALSE(stack.CanRedo());
}

TEST(TreeUndoStackTest, UndoRestoresPreviousSnapshot) {
  TreeUndoStack stack;
  DockTree current = MakeTreeWith("a");
  stack.Push(current);
  current = MakeTreeWith("b");

  ASSERT_TRUE(stack.Undo(&current));
  ASSERT_EQ(current.root->panels.size(), 1u);
  EXPECT_EQ(current.root->panels[0].panel_id, "a");
  EXPECT_TRUE(stack.CanRedo());
  EXPECT_FALSE(stack.CanUndo());
}

TEST(TreeUndoStackTest, RedoReappliesReplacedSnapshot) {
  TreeUndoStack stack;
  DockTree current = MakeTreeWith("a");
  stack.Push(current);
  current = MakeTreeWith("b");
  ASSERT_TRUE(stack.Undo(&current));
  ASSERT_EQ(current.root->panels[0].panel_id, "a");

  ASSERT_TRUE(stack.Redo(&current));
  EXPECT_EQ(current.root->panels[0].panel_id, "b");
  EXPECT_TRUE(stack.CanUndo());
  EXPECT_FALSE(stack.CanRedo());
}

TEST(TreeUndoStackTest, PushClearsRedoHistory) {
  TreeUndoStack stack;
  DockTree current = MakeTreeWith("a");
  stack.Push(current);
  current = MakeTreeWith("b");
  ASSERT_TRUE(stack.Undo(&current));
  EXPECT_TRUE(stack.CanRedo());

  // Any new push after an undo invalidates the redo branch.
  stack.Push(current);
  EXPECT_FALSE(stack.CanRedo());
}

TEST(TreeUndoStackTest, UndoReturnsFalseOnEmptyStackAndNull) {
  TreeUndoStack stack;
  DockTree current = MakeTreeWith("only");
  EXPECT_FALSE(stack.Undo(&current));
  EXPECT_FALSE(stack.Undo(nullptr));
}

TEST(TreeUndoStackTest, CapEvictsOldestOnOverflow) {
  TreeUndoStack stack(/*max_steps=*/3);
  DockTree current = MakeTreeWith("0");
  stack.Push(current);  // snapshot "0"
  current = MakeTreeWith("1");
  stack.Push(current);  // snapshot "1"
  current = MakeTreeWith("2");
  stack.Push(current);  // snapshot "2"
  current = MakeTreeWith("3");
  stack.Push(current);  // snapshot "3" — "0" should evict.

  EXPECT_EQ(stack.UndoDepth(), 3u);
  // Undo three times — the oldest snapshot we can reach is "1".
  current = MakeTreeWith("current");
  ASSERT_TRUE(stack.Undo(&current));
  EXPECT_EQ(current.root->panels[0].panel_id, "3");
  ASSERT_TRUE(stack.Undo(&current));
  EXPECT_EQ(current.root->panels[0].panel_id, "2");
  ASSERT_TRUE(stack.Undo(&current));
  EXPECT_EQ(current.root->panels[0].panel_id, "1");
  EXPECT_FALSE(stack.CanUndo());
}

TEST(TreeUndoStackTest, PopLastPushRemovesOnlyLastUndoEntry) {
  TreeUndoStack stack;
  DockTree current = MakeTreeWith("a");
  stack.Push(current);  // snapshot "a"
  current = MakeTreeWith("b");
  stack.Push(current);  // snapshot "b"
  EXPECT_EQ(stack.UndoDepth(), 2u);

  stack.PopLastPush();
  EXPECT_EQ(stack.UndoDepth(), 1u);
  EXPECT_FALSE(stack.CanRedo())
      << "PopLastPush must not touch redo — that's what Undo() is for.";

  // The surviving snapshot should be the oldest Push().
  current = MakeTreeWith("current");
  ASSERT_TRUE(stack.Undo(&current));
  EXPECT_EQ(current.root->panels[0].panel_id, "a");
}

TEST(TreeUndoStackTest, PopLastPushDoesNotMutateCurrent) {
  TreeUndoStack stack;
  DockTree current = MakeTreeWith("snapshot");
  stack.Push(current);
  current = MakeTreeWith("live");

  stack.PopLastPush();
  EXPECT_EQ(current.root->panels[0].panel_id, "live")
      << "PopLastPush must leave current untouched (no write-back).";
  EXPECT_FALSE(stack.CanUndo());
}

TEST(TreeUndoStackTest, PopLastPushOnEmptyIsNoOp) {
  TreeUndoStack stack;
  stack.PopLastPush();  // must not crash or underflow.
  EXPECT_FALSE(stack.CanUndo());
}

TEST(TreeUndoStackTest, ClearEmptiesBothStacks) {
  TreeUndoStack stack;
  DockTree current = MakeTreeWith("a");
  stack.Push(current);
  current = MakeTreeWith("b");
  ASSERT_TRUE(stack.Undo(&current));
  EXPECT_TRUE(stack.CanRedo());

  stack.Clear();
  EXPECT_FALSE(stack.CanUndo());
  EXPECT_FALSE(stack.CanRedo());
}

}  // namespace
}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
