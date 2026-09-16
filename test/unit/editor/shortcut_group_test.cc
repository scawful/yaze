#include "app/editor/system/commands/shortcut_manager.h"

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

TEST(ShortcutGroupTest, MapsMenuIaBuckets) {
  EXPECT_EQ(InferShortcutGroup("File: Open ROM"), "File");
  EXPECT_EQ(InferShortcutGroup("Edit: Undo"), "Edit");
  EXPECT_EQ(InferShortcutGroup("View: Show Sidebar"), "Windows");
  EXPECT_EQ(InferShortcutGroup("View: Toggle Project Panel"), "Drawers");
  EXPECT_EQ(InferShortcutGroup("View: Next Right Drawer"), "Drawers");
  EXPECT_EQ(InferShortcutGroup("drawer: Settings"), "Drawers");
  EXPECT_EQ(InferShortcutGroup("window: Room List"), "Windows");
  EXPECT_EQ(InferShortcutGroup("Window Browser"), "Windows");
  EXPECT_EQ(InferShortcutGroup("Command Palette"), "Tools");
  EXPECT_EQ(InferShortcutGroup("layout: Minimal"), "Layout");
  EXPECT_EQ(InferShortcutGroup("Apply Layout: Developer"), "Layout");
  EXPECT_EQ(InferShortcutGroup("dungeon.object.place_tool"), "Editor");
  EXPECT_EQ(InferShortcutGroup("Help: About"), "Help");
  EXPECT_EQ(InferShortcutGroup("Something Else"), "Other");
}

}  // namespace
}  // namespace yaze::editor
