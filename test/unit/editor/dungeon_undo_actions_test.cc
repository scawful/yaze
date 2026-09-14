#include "app/editor/dungeon/dungeon_undo_actions.h"

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

TEST(DungeonUndoActionsTest, CustomCollisionUndoRedoRestoresMap) {
  zelda3::CustomCollisionMap before;
  before.has_data = true;
  before.tiles[0] = 0x08;

  zelda3::CustomCollisionMap after = before;
  after.tiles[0] = 0x1B;

  zelda3::CustomCollisionMap restored;
  int restored_room_id = -1;

  DungeonCustomCollisionAction action(
      /*room_id=*/0x25, before, after,
      [&](int room_id, const zelda3::CustomCollisionMap& map) {
        restored_room_id = room_id;
        restored = map;
      });

  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(restored_room_id, 0x25);
  EXPECT_TRUE(restored.has_data);
  EXPECT_EQ(restored.tiles[0], 0x08);

  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(restored_room_id, 0x25);
  EXPECT_TRUE(restored.has_data);
  EXPECT_EQ(restored.tiles[0], 0x1B);
}

TEST(DungeonUndoActionsTest, CustomCollisionBatchUndoRedoRestoresEveryRoom) {
  zelda3::CustomCollisionMap empty;
  zelda3::CustomCollisionMap room_25;
  room_25.has_data = true;
  room_25.tiles[10] = 0xB0;
  zelda3::CustomCollisionMap room_26;
  room_26.has_data = true;
  room_26.tiles[20] = 0xB7;

  const std::vector<DungeonCustomCollisionSnapshot> before = {{0x25, empty},
                                                              {0x26, empty}};
  const std::vector<DungeonCustomCollisionSnapshot> after = {{0x25, room_25},
                                                             {0x26, room_26}};
  std::vector<DungeonCustomCollisionSnapshot> restored;

  DungeonCustomCollisionBatchAction action(
      before, after,
      [&](const std::vector<DungeonCustomCollisionSnapshot>& snapshots) {
        restored = snapshots;
        return absl::OkStatus();
      });

  EXPECT_EQ(action.Description(), "Generate minecart collision for 2 rooms");
  ASSERT_TRUE(action.Undo().ok());
  ASSERT_EQ(restored.size(), 2u);
  EXPECT_EQ(restored[0].room_id, 0x25);
  EXPECT_FALSE(restored[0].map.has_data);
  EXPECT_EQ(restored[1].room_id, 0x26);
  EXPECT_FALSE(restored[1].map.has_data);

  ASSERT_TRUE(action.Redo().ok());
  ASSERT_EQ(restored.size(), 2u);
  EXPECT_EQ(restored[0].map.tiles[10], 0xB0);
  EXPECT_EQ(restored[1].map.tiles[20], 0xB7);
}

TEST(DungeonUndoActionsTest, CustomCollisionBatchRequiresRestoreCallback) {
  DungeonCustomCollisionBatchAction action({}, {}, {});
  EXPECT_TRUE(absl::IsInternal(action.Undo()));
  EXPECT_TRUE(absl::IsInternal(action.Redo()));
}

TEST(DungeonUndoActionsTest, WaterFillUndoRedoRestoresSnapshot) {
  WaterFillSnapshot before;
  before.sram_bit_mask = 0x02;
  before.offsets = {0u, 64u, 65u};

  WaterFillSnapshot after = before;
  after.offsets.push_back(66u);

  WaterFillSnapshot restored;
  int restored_room_id = -1;

  DungeonWaterFillAction action(
      /*room_id=*/0x27, before, after,
      [&](int room_id, const WaterFillSnapshot& snap) {
        restored_room_id = room_id;
        restored = snap;
      });

  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(restored_room_id, 0x27);
  EXPECT_EQ(restored.sram_bit_mask, 0x02);
  EXPECT_EQ(restored.offsets, before.offsets);

  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(restored_room_id, 0x27);
  EXPECT_EQ(restored.sram_bit_mask, 0x02);
  EXPECT_EQ(restored.offsets, after.offsets);
}

}  // namespace
}  // namespace yaze::editor
