#include <gtest/gtest.h>

#include "app/editor/menu/window_sidebar.h"
#include "app/editor/system/workspace/workspace_window_manager.h"

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
  EXPECT_FALSE(WindowSidebar::IsDungeonWindowModeTarget("dungeon.workbench"));
  EXPECT_FALSE(
      WindowSidebar::IsDungeonWindowModeTarget("overworld.map_properties"));
}

TEST(WindowSidebarTest, SidebarSectionForMapsGroupsAndFallbacks) {
  EXPECT_EQ(WindowSidebar::SidebarSectionFor(""), "Editors");
  EXPECT_EQ(WindowSidebar::SidebarSectionFor("Windows"), "Editors");
  EXPECT_EQ(WindowSidebar::SidebarSectionFor("Core"), "Core");
  EXPECT_EQ(WindowSidebar::SidebarSectionFor("Advanced"), "Advanced");
  EXPECT_EQ(WindowSidebar::SidebarSectionFor("Planning"), "Planning");
}

TEST(WindowSidebarTest, SidebarSectionForInfersRoomsFromCardId) {
  WindowDescriptor room;
  room.card_id = "dungeon.room_42";
  room.workflow_group = "";
  EXPECT_EQ(WindowSidebar::SidebarSectionFor(room), "Rooms");

  WindowDescriptor selector;
  selector.card_id = "dungeon.room_selector";
  selector.workflow_group = "";
  EXPECT_EQ(WindowSidebar::SidebarSectionFor(selector), "Editors");

  WindowDescriptor core;
  core.card_id = "dungeon.workbench";
  core.workflow_group = "Core";
  EXPECT_EQ(WindowSidebar::SidebarSectionFor(core), "Core");
}

TEST(WindowSidebarTest, OmitsWindowModeTargetsInWorkbench) {
  EXPECT_TRUE(
      WindowSidebar::ShouldOmitWindowInSidebar("dungeon.room_selector", true));
  EXPECT_TRUE(
      WindowSidebar::ShouldOmitWindowInSidebar("dungeon.room_matrix", true));
  EXPECT_TRUE(
      WindowSidebar::ShouldOmitWindowInSidebar("dungeon.room_12a", true));
  EXPECT_FALSE(
      WindowSidebar::ShouldOmitWindowInSidebar("dungeon.workbench", true));
  EXPECT_FALSE(WindowSidebar::ShouldOmitWindowInSidebar(
      "dungeon.object_selector", true));
  EXPECT_FALSE(
      WindowSidebar::ShouldOmitWindowInSidebar("dungeon.room_selector", false));
}

}  // namespace
}  // namespace yaze::editor
