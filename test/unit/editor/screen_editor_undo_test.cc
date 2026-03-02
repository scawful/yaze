#include "app/editor/core/undo_manager.h"
#include "app/editor/graphics/screen_undo_actions.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

ScreenSnapshot MakeSnapshot(uint8_t room_value, uint8_t gfx_value,
                            uint16_t boss_room, uint8_t selected_room,
                            const std::string& label) {
  ScreenSnapshot snapshot;

  std::vector<std::array<uint8_t, zelda3::kNumRooms>> floor_rooms(1);
  floor_rooms[0].fill(0x0F);
  floor_rooms[0][selected_room] = room_value;

  std::vector<std::array<uint8_t, zelda3::kNumRooms>> floor_gfx(1);
  floor_gfx[0].fill(0xFF);
  floor_gfx[0][selected_room] = gfx_value;

  snapshot.dungeon_maps.emplace_back(boss_room, 1, 0, floor_rooms, floor_gfx);
  snapshot.dungeon_map_labels[0].emplace_back();
  snapshot.dungeon_map_labels[0][0][selected_room] = label;
  snapshot.selected_dungeon = 0;
  snapshot.floor_number = 0;
  snapshot.selected_room = selected_room;
  return snapshot;
}

TEST(ScreenUndoActionsTest, UndoRedoRestoresDungeonMapAndSelectionState) {
  constexpr uint8_t kEditedRoom = 3;
  ScreenSnapshot before =
      MakeSnapshot(0x12, 0x34, 0x0001, kEditedRoom, "Before");
  ScreenSnapshot after = MakeSnapshot(0x24, 0x56, 0x0002, kEditedRoom, "After");

  ScreenSnapshot restored;
  ScreenEditAction action(
      before, after,
      [&](const ScreenSnapshot& snapshot) { restored = snapshot; },
      "Edit dungeon map");

  ASSERT_TRUE(action.Undo().ok());
  ASSERT_EQ(restored.dungeon_maps.size(), 1u);
  EXPECT_EQ(restored.dungeon_maps[0].boss_room, 0x0001);
  EXPECT_EQ(restored.dungeon_maps[0].floor_rooms[0][kEditedRoom], 0x12);
  EXPECT_EQ(restored.dungeon_maps[0].floor_gfx[0][kEditedRoom], 0x34);
  EXPECT_EQ(restored.dungeon_map_labels[0][0][kEditedRoom], "Before");
  EXPECT_EQ(restored.selected_room, kEditedRoom);

  ASSERT_TRUE(action.Redo().ok());
  ASSERT_EQ(restored.dungeon_maps.size(), 1u);
  EXPECT_EQ(restored.dungeon_maps[0].boss_room, 0x0002);
  EXPECT_EQ(restored.dungeon_maps[0].floor_rooms[0][kEditedRoom], 0x24);
  EXPECT_EQ(restored.dungeon_maps[0].floor_gfx[0][kEditedRoom], 0x56);
  EXPECT_EQ(restored.dungeon_map_labels[0][0][kEditedRoom], "After");
  EXPECT_EQ(restored.selected_room, kEditedRoom);
}

TEST(ScreenUndoActionsTest, UndoManagerIntegrationUsesDescriptionAndRestores) {
  constexpr uint8_t kEditedRoom = 5;
  UndoManager manager;
  ScreenSnapshot before = MakeSnapshot(0x11, 0x22, 0x0010, kEditedRoom, "Old");
  ScreenSnapshot after = MakeSnapshot(0x33, 0x44, 0x0020, kEditedRoom, "New");
  ScreenSnapshot restored;

  manager.Push(std::make_unique<ScreenEditAction>(
      before, after,
      [&](const ScreenSnapshot& snapshot) { restored = snapshot; },
      "Paint dungeon room gfx"));

  EXPECT_TRUE(manager.CanUndo());
  EXPECT_EQ(manager.GetUndoDescription(), "Paint dungeon room gfx");

  ASSERT_TRUE(manager.Undo().ok());
  EXPECT_EQ(restored.dungeon_maps[0].floor_gfx[0][kEditedRoom], 0x22);
  EXPECT_EQ(restored.dungeon_map_labels[0][0][kEditedRoom], "Old");
  EXPECT_TRUE(manager.CanRedo());

  ASSERT_TRUE(manager.Redo().ok());
  EXPECT_EQ(restored.dungeon_maps[0].floor_gfx[0][kEditedRoom], 0x44);
  EXPECT_EQ(restored.dungeon_map_labels[0][0][kEditedRoom], "New");
}

}  // namespace
}  // namespace yaze::editor
