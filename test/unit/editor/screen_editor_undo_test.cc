#include "app/editor/graphics/screen_undo_actions.h"

#include <memory>

#include <gtest/gtest.h>

#include "app/editor/core/undo_manager.h"
#include "app/editor/graphics/screen_editor.h"

namespace yaze::editor {
class ScreenFloorMutationTestPeer {
 public:
  static void Seed(ScreenEditor& editor, bool boss = false) {
    std::vector<std::array<uint8_t, zelda3::kNumRooms>> rooms(2), graphics(2);
    rooms[0].fill(0x22);
    rooms[1].fill(0x33);
    graphics[0].fill(0x42);
    graphics[1].fill(0x43);
    editor.dungeon_maps_.emplace_back(boss ? 0x22 : 0xFFFF, 1, 1, rooms,
                                      graphics);
    editor.dungeon_map_labels_[0].resize(2);
    editor.dungeon_map_labels_[0][0].fill("22");
    editor.dungeon_map_labels_[0][1].fill("33");
    editor.rom_backed_state_valid_ = true;
    editor.selected_dungeon = 0;
    editor.floor_number = 1;
  }
  static absl::Status Change(ScreenEditor& editor, bool basement, bool add) {
    return editor.ChangeDungeonMapFloorCount(basement, add);
  }
  static ScreenSnapshot Snapshot(const ScreenEditor& editor) {
    return editor.CaptureDungeonMapSnapshot();
  }
  static bool CanUndo(const ScreenEditor& editor) {
    return editor.undo_manager_.CanUndo();
  }
  static int Floor(const ScreenEditor& editor) { return editor.floor_number; }
};
namespace {

TEST(ScreenFloorMutationTest, BasementInsertionAndRemovalPreserveRowsAndUndo) {
  ScreenEditor editor;
  ScreenFloorMutationTestPeer::Seed(editor, true);
  const auto before = ScreenFloorMutationTestPeer::Snapshot(editor);
  ASSERT_TRUE(ScreenFloorMutationTestPeer::Change(editor, true, true).ok());
  const auto added = ScreenFloorMutationTestPeer::Snapshot(editor);
  EXPECT_EQ(added.dungeon_map.map_data.nbr_of_basement, 2);
  EXPECT_EQ(added.dungeon_map.map_data.floor_rooms.size(), 3u);
  EXPECT_EQ(added.dungeon_map.map_data.floor_rooms[0][0], 0x0F);
  EXPECT_EQ(added.dungeon_map.map_data.floor_gfx[0][0], 0xFF);
  EXPECT_EQ(added.dungeon_map.labels[0][0], "$0F");
  EXPECT_EQ(added.dungeon_map.map_data.floor_rooms[1],
            before.dungeon_map.map_data.floor_rooms[0]);
  EXPECT_EQ(added.dungeon_map.map_data.floor_gfx[2],
            before.dungeon_map.map_data.floor_gfx[1]);
  EXPECT_EQ(added.dungeon_map.labels[1], before.dungeon_map.labels[0]);
  EXPECT_EQ(ScreenFloorMutationTestPeer::Floor(editor), 2);
  ASSERT_TRUE(editor.Undo().ok());
  auto undone = ScreenFloorMutationTestPeer::Snapshot(editor);
  EXPECT_EQ(undone.dungeon_map.map_data.floor_rooms,
            before.dungeon_map.map_data.floor_rooms);
  EXPECT_EQ(undone.dungeon_map.labels, before.dungeon_map.labels);
  EXPECT_EQ(ScreenFloorMutationTestPeer::Floor(editor), 1);
  ASSERT_TRUE(editor.Redo().ok());
  EXPECT_EQ(ScreenFloorMutationTestPeer::Floor(editor), 2);
  // Remove the new empty deepest row, not the boss-bearing B1 row.
  ASSERT_TRUE(ScreenFloorMutationTestPeer::Change(editor, true, false).ok());
  auto removed = ScreenFloorMutationTestPeer::Snapshot(editor);
  EXPECT_EQ(removed.dungeon_map.map_data.floor_rooms,
            before.dungeon_map.map_data.floor_rooms);
  EXPECT_EQ(removed.dungeon_map.map_data.floor_gfx,
            before.dungeon_map.map_data.floor_gfx);
  EXPECT_EQ(removed.dungeon_map.labels, before.dungeon_map.labels);
  EXPECT_EQ(removed.dungeon_map.map_data.boss_room, 0x22);
  EXPECT_TRUE(editor.HasPendingDungeonMapChanges());
  EXPECT_FALSE(editor.HasPendingDungeonMapTile16Changes());
}

TEST(ScreenFloorMutationTest, UpperFloorUsesTailAndRestoresSelection) {
  ScreenEditor editor;
  ScreenFloorMutationTestPeer::Seed(editor);
  ASSERT_TRUE(ScreenFloorMutationTestPeer::Change(editor, false, true).ok());
  EXPECT_EQ(editor.dungeon_maps_[0].nbr_of_floor, 2);
  EXPECT_EQ(editor.dungeon_maps_[0].floor_rooms[0][0], 0x22);
  EXPECT_EQ(editor.dungeon_maps_[0].floor_rooms[1][0], 0x33);
  EXPECT_EQ(editor.dungeon_maps_[0].floor_rooms[2][0], 0x0F);
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_EQ(editor.dungeon_maps_[0].floor_rooms.size(), 2u);
  ASSERT_TRUE(editor.Redo().ok());
  ASSERT_TRUE(ScreenFloorMutationTestPeer::Change(editor, false, false).ok());
  EXPECT_EQ(editor.dungeon_maps_[0].floor_rooms.size(), 2u);
  ASSERT_TRUE(ScreenFloorMutationTestPeer::Change(editor, false, false).ok());
  EXPECT_EQ(ScreenFloorMutationTestPeer::Floor(editor), 0);
  EXPECT_EQ(editor.dungeon_maps_[0].floor_rooms[0][0], 0x22);
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_EQ(ScreenFloorMutationTestPeer::Floor(editor), 1);
  EXPECT_EQ(editor.dungeon_maps_[0].floor_rooms[1][0], 0x33);
}

TEST(ScreenFloorMutationTest, BossRemovalRefusesWithoutDirtyOrHistoryChanges) {
  for (bool basement : {false, true}) {
    ScreenEditor editor;
    ScreenFloorMutationTestPeer::Seed(editor, true);
    editor.dungeon_maps_[0].boss_room = basement ? 0x22 : 0x33;
    const auto before = ScreenFloorMutationTestPeer::Snapshot(editor);
    EXPECT_FALSE(
        ScreenFloorMutationTestPeer::Change(editor, basement, false).ok());
    const auto after = ScreenFloorMutationTestPeer::Snapshot(editor);
    EXPECT_EQ(after.dungeon_map.map_data.floor_rooms,
              before.dungeon_map.map_data.floor_rooms);
    EXPECT_EQ(after.dungeon_map.map_data.floor_gfx,
              before.dungeon_map.map_data.floor_gfx);
    EXPECT_EQ(after.dungeon_map.labels, before.dungeon_map.labels);
    EXPECT_EQ(after.dungeon_map.floor_index, before.dungeon_map.floor_index);
    EXPECT_FALSE(ScreenFloorMutationTestPeer::CanUndo(editor));
    EXPECT_FALSE(editor.HasPendingScreenChanges());
  }
}

TEST(ScreenFloorMutationTest, InvalidShapeAndLastFloorRemovalRefuse) {
  ScreenEditor editor;
  ScreenFloorMutationTestPeer::Seed(editor);
  editor.dungeon_maps_[0].floor_gfx.pop_back();
  EXPECT_FALSE(ScreenFloorMutationTestPeer::Change(editor, true, true).ok());
  EXPECT_FALSE(ScreenFloorMutationTestPeer::CanUndo(editor));
  EXPECT_FALSE(editor.HasPendingScreenChanges());
  editor.dungeon_maps_[0].floor_gfx.push_back({});
  ASSERT_TRUE(ScreenFloorMutationTestPeer::Change(editor, false, false).ok());
  EXPECT_FALSE(ScreenFloorMutationTestPeer::Change(editor, true, false).ok());
  EXPECT_EQ(editor.dungeon_maps_[0].floor_rooms.size(), 1u);
}

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
