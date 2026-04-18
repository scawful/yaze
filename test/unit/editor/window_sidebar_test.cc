#include <gtest/gtest.h>

#include "app/editor/menu/window_sidebar.h"

namespace yaze::editor {
namespace {

TEST(WindowSidebarTest, MatchesSearchByNameIdAndShortcut) {
  EXPECT_TRUE(WindowSidebar::MatchesWindowSearch(
      "item", "Item List", "overworld.item_list", "Ctrl+I"));
  EXPECT_TRUE(WindowSidebar::MatchesWindowSearch(
      "overworld.item", "Item List", "overworld.item_list", "Ctrl+I"));
  EXPECT_TRUE(WindowSidebar::MatchesWindowSearch(
      "ctrl+i", "Item List", "overworld.item_list", "Ctrl+I"));
  EXPECT_FALSE(WindowSidebar::MatchesWindowSearch(
      "graphics", "Item List", "overworld.item_list", "Ctrl+I"));
}

TEST(WindowSidebarTest, DetectsDungeonWindowModeTargets) {
  EXPECT_TRUE(
      WindowSidebar::IsDungeonWindowModeTarget("dungeon.room_selector"));
  EXPECT_TRUE(WindowSidebar::IsDungeonWindowModeTarget("dungeon.room_matrix"));
  EXPECT_TRUE(WindowSidebar::IsDungeonWindowModeTarget("dungeon.room_12a"));
  EXPECT_FALSE(
      WindowSidebar::IsDungeonWindowModeTarget("dungeon.workbench"));
  EXPECT_FALSE(
      WindowSidebar::IsDungeonWindowModeTarget("overworld.map_properties"));
}

}  // namespace
}  // namespace yaze::editor
