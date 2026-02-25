#include "app/editor/dungeon/dungeon_canvas_viewer.h"

#include "gtest/gtest.h"

namespace yaze::editor {

TEST(DungeonCanvasViewerNavigationTest, CanNavigateRoomsReflectsCallbacks) {
  DungeonCanvasViewer viewer;

  EXPECT_FALSE(viewer.CanNavigateRooms());

  viewer.SetRoomNavigationCallback([](int) {});
  EXPECT_TRUE(viewer.CanNavigateRooms());

  viewer.SetRoomNavigationCallback(nullptr);
  EXPECT_FALSE(viewer.CanNavigateRooms());

  viewer.SetRoomSwapCallback([](int, int) {});
  EXPECT_TRUE(viewer.CanNavigateRooms());
}

TEST(DungeonCanvasViewerNavigationTest, NavigateToRoomIgnoresInvalidTargets) {
  DungeonCanvasViewer viewer;
  int nav_calls = 0;
  int swap_calls = 0;
  viewer.SetRoomNavigationCallback([&](int) { ++nav_calls; });
  viewer.SetRoomSwapCallback([&](int, int) { ++swap_calls; });

  viewer.NavigateToRoom(-1);
  viewer.NavigateToRoom(zelda3::kNumberOfRooms);

  EXPECT_EQ(nav_calls, 0);
  EXPECT_EQ(swap_calls, 0);
}

TEST(DungeonCanvasViewerNavigationTest, NavigateToRoomPrefersSwapCallback) {
  DungeonCanvasViewer viewer;

  int nav_target = -1;
  int swap_old = -2;
  int swap_new = -2;
  viewer.SetRoomNavigationCallback([&](int target) { nav_target = target; });
  viewer.SetRoomSwapCallback([&](int old_room, int new_room) {
    swap_old = old_room;
    swap_new = new_room;
  });

  viewer.NavigateToRoom(0xA8);

  EXPECT_EQ(nav_target, -1);
  EXPECT_EQ(swap_old, -1);
  EXPECT_EQ(swap_new, 0xA8);
}

TEST(DungeonCanvasViewerNavigationTest, NavigateToRoomUsesNavigationFallback) {
  DungeonCanvasViewer viewer;

  int nav_target = -1;
  viewer.SetRoomNavigationCallback([&](int target) { nav_target = target; });

  viewer.NavigateToRoom(0xDA);

  EXPECT_EQ(nav_target, 0xDA);
}

}  // namespace yaze::editor
