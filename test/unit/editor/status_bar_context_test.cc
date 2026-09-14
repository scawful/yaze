#include "app/editor/menu/status_bar.h"

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

TEST(StatusBarContextTest,
     ActiveEditorAndDirtyScopeAreIndependentOfContributions) {
  StatusBar bar;

  bar.SetActiveEditor("Dungeon");
  bar.SetDirtyScope("Rooms+ROM");
  bar.SetCustomSegment("Room", "0x001");

  EXPECT_TRUE(bar.has_active_editor_for_test());
  EXPECT_EQ(bar.active_editor_for_test(), "Dungeon");
  EXPECT_TRUE(bar.has_dirty_scope_for_test());
  EXPECT_EQ(bar.dirty_scope_for_test(), "Rooms+ROM");

  // Editor contributions clear mode/custom only — context strip stays.
  bar.ClearEditorContributions();
  EXPECT_TRUE(bar.has_active_editor_for_test());
  EXPECT_TRUE(bar.has_dirty_scope_for_test());

  bar.ClearActiveEditor();
  bar.ClearDirtyScope();
  EXPECT_FALSE(bar.has_active_editor_for_test());
  EXPECT_FALSE(bar.has_dirty_scope_for_test());
}

TEST(StatusBarContextTest, SessionDisplayNameStoredForMultiSession) {
  StatusBar bar;
  bar.SetSessionInfo(1, 3, "Session 1 (oos168)");
  EXPECT_EQ(bar.session_display_name_for_test(), "Session 1 (oos168)");

  bar.SetSessionInfo(0, 1);
  EXPECT_TRUE(bar.session_display_name_for_test().empty());
}

TEST(StatusBarContextTest, EmptyLabelsClearSegments) {
  StatusBar bar;
  bar.SetActiveEditor("Overworld");
  bar.SetDirtyScope("Project");
  bar.SetActiveEditor("");
  bar.SetDirtyScope("");
  EXPECT_FALSE(bar.has_active_editor_for_test());
  EXPECT_FALSE(bar.has_dirty_scope_for_test());
}

}  // namespace
}  // namespace yaze::editor
