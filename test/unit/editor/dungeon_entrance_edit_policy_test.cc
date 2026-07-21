#include "app/editor/dungeon/dungeon_entrance_edit_policy.h"

#include <vector>

#include "gtest/gtest.h"
#include "rom/rom.h"

namespace yaze::editor {
namespace {

TEST(DungeonEntranceEditPolicyTest, SpawnPropertyChangeCannotMarkRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::RoomEntrance spawn(&rom, 0, true);

  EXPECT_FALSE(CanEditDungeonEntrance(0, spawn));
  EXPECT_FALSE(MarkDungeonEntranceDirtyIfEditable(0, spawn, true));
  EXPECT_FALSE(spawn.dirty());
}

TEST(DungeonEntranceEditPolicyTest,
     SpawnSlotCannotMarkDefaultModelDirtyBeforeLoad) {
  zelda3::RoomEntrance default_model;

  EXPECT_FALSE(CanEditDungeonEntrance(0, default_model));
  EXPECT_FALSE(MarkDungeonEntranceDirtyIfEditable(0, default_model, true));
  EXPECT_FALSE(default_model.dirty());
}

TEST(DungeonEntranceEditPolicyTest, RegularPropertyChangeMarksRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::RoomEntrance entrance(&rom, 0, false);

  constexpr int kRegularSlot = zelda3::kNumDungeonSpawnPoints;
  EXPECT_TRUE(CanEditDungeonEntrance(kRegularSlot, entrance));
  EXPECT_FALSE(
      MarkDungeonEntranceDirtyIfEditable(kRegularSlot, entrance, false));
  EXPECT_FALSE(entrance.dirty());
  EXPECT_TRUE(MarkDungeonEntranceDirtyIfEditable(kRegularSlot, entrance, true));
  EXPECT_TRUE(entrance.dirty());
}

}  // namespace
}  // namespace yaze::editor
