#include "app/editor/graphics/screen_undo_actions.h"

#include <memory>

#include <gtest/gtest.h>

#include "app/editor/core/undo_manager.h"

namespace yaze::editor {
namespace {

// ---------------------------------------------------------------------------
// ScreenEditAction unit tests (no ImGui / ROM dependency)
// ---------------------------------------------------------------------------

TEST(ScreenEditActionTest, DungeonMapUndoRedoRoundTrip) {
  // Build a "before" dungeon map snapshot
  ScreenSnapshot before;
  before.edit_type = ScreenEditType::kDungeonMap;
  before.dungeon_map.dungeon_index = 0;
  before.dungeon_map.map_data = zelda3::DungeonMap(
      /*boss_room=*/0x0001, /*nbr_of_floor=*/1, /*nbr_of_basement=*/0,
      /*floor_rooms=*/{{}}, /*floor_gfx=*/{{}});
  before.dungeon_map.map_data.floor_gfx[0][0] = 0x0A;

  // Build an "after" dungeon map snapshot (tile changed)
  ScreenSnapshot after = before;
  after.dungeon_map.map_data.floor_gfx[0][0] = 0x1B;

  // Track which snapshot was last restored
  ScreenSnapshot restored;
  auto restore = [&](const ScreenSnapshot& snap) {
    restored = snap;
  };

  ScreenEditAction action(before, after, restore, "Place tile on dungeon map");

  // Undo should restore the before-state
  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(restored.edit_type, ScreenEditType::kDungeonMap);
  EXPECT_EQ(restored.dungeon_map.map_data.floor_gfx[0][0], 0x0A);

  // Redo should restore the after-state
  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(restored.dungeon_map.map_data.floor_gfx[0][0], 0x1B);
}

TEST(ScreenEditActionTest, Tile16CompUndoRedoRoundTrip) {
  ScreenSnapshot before;
  before.edit_type = ScreenEditType::kTile16Edit;
  before.tile16_comp.tile16_id = 5;
  before.tile16_comp.tile_info[0] = gfx::TileInfo(10, 0, false, false, false);

  ScreenSnapshot after = before;
  after.tile16_comp.tile_info[0] = gfx::TileInfo(20, 1, true, false, false);

  ScreenSnapshot restored;
  auto restore = [&](const ScreenSnapshot& snap) {
    restored = snap;
  };

  ScreenEditAction action(before, after, restore, "Modify tile16 #5");

  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(restored.tile16_comp.tile_info[0].id_, 10);

  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(restored.tile16_comp.tile_info[0].id_, 20);
}

TEST(ScreenEditActionTest, DescriptionAndMemoryUsage) {
  ScreenSnapshot before;
  before.edit_type = ScreenEditType::kDungeonMap;
  before.dungeon_map.dungeon_index = 3;
  before.dungeon_map.map_data = zelda3::DungeonMap(0, 1, 0, {{}}, {{}});

  ScreenSnapshot after = before;

  ScreenEditAction action(
      before, after, [](const ScreenSnapshot&) {}, "Add floor");

  EXPECT_EQ(action.Description(), "Add floor");
  EXPECT_GT(action.MemoryUsage(), 0u);
  EXPECT_FALSE(action.CanMergeWith(action));
}

TEST(ScreenEditActionTest, NullRestoreCallbackReturnsError) {
  ScreenSnapshot snap;
  ScreenEditAction action(snap, snap, nullptr, "bad action");

  auto undo_status = action.Undo();
  EXPECT_FALSE(undo_status.ok());

  auto redo_status = action.Redo();
  EXPECT_FALSE(redo_status.ok());
}

TEST(ScreenEditActionTest, UndoManagerIntegration) {
  // Verify ScreenEditAction works with UndoManager push/undo/redo cycle.
  UndoManager mgr;

  ScreenSnapshot before;
  before.edit_type = ScreenEditType::kDungeonMap;
  before.dungeon_map.dungeon_index = 0;
  before.dungeon_map.map_data =
      zelda3::DungeonMap(0xFFFF, 2, 1, {{}, {}, {}}, {{}, {}, {}});

  ScreenSnapshot after = before;
  after.dungeon_map.map_data.nbr_of_floor = 3;

  ScreenSnapshot restored;
  auto restore = [&](const ScreenSnapshot& snap) {
    restored = snap;
  };

  mgr.Push(
      std::make_unique<ScreenEditAction>(before, after, restore, "Add floor"));

  EXPECT_TRUE(mgr.CanUndo());
  EXPECT_FALSE(mgr.CanRedo());
  EXPECT_EQ(mgr.GetUndoDescription(), "Add floor");

  ASSERT_TRUE(mgr.Undo().ok());
  EXPECT_EQ(restored.dungeon_map.map_data.nbr_of_floor, 2);

  ASSERT_TRUE(mgr.Redo().ok());
  EXPECT_EQ(restored.dungeon_map.map_data.nbr_of_floor, 3);
}

}  // namespace
}  // namespace yaze::editor
