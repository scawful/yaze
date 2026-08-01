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

TEST(DungeonEntranceEditPolicyTest,
     LoadedSpawnPropertyChangeMarksMatchingRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  for (int slot = 0; slot < zelda3::kNumDungeonSpawnPoints; ++slot) {
    auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, slot);
    ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
    auto spawn = *spawn_or;

    EXPECT_TRUE(CanEditDungeonSpawnPoint(slot, spawn));
    EXPECT_FALSE(MarkDungeonSpawnPointDirtyIfEditable(slot, spawn, false));
    EXPECT_FALSE(spawn.dirty());
    EXPECT_TRUE(MarkDungeonSpawnPointDirtyIfEditable(slot, spawn, true));
    EXPECT_TRUE(spawn.dirty());
  }
}

TEST(DungeonEntranceEditPolicyTest,
     DedicatedSpawnSlotCannotMarkDefaultModelDirtyBeforeLoad) {
  zelda3::DungeonSpawnPoint default_model;

  EXPECT_FALSE(CanEditDungeonSpawnPoint(0, default_model));
  EXPECT_FALSE(MarkDungeonSpawnPointDirtyIfEditable(0, default_model, true));
  EXPECT_FALSE(default_model.dirty());
}

TEST(DungeonEntranceEditPolicyTest, SpawnSlotCannotMarkMismatchedRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, 1);
  ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
  auto spawn = *spawn_or;

  EXPECT_FALSE(CanEditDungeonSpawnPoint(0, spawn));
  EXPECT_FALSE(MarkDungeonSpawnPointDirtyIfEditable(0, spawn, true));
  EXPECT_FALSE(spawn.dirty());
}

TEST(DungeonEntranceEditPolicyTest,
     InvalidSpawnSlotsCannotMarkLoadedRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, 0);
  ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();

  for (const int invalid_slot : {-1, zelda3::kNumDungeonSpawnPoints}) {
    auto spawn = *spawn_or;
    EXPECT_FALSE(CanEditDungeonSpawnPoint(invalid_slot, spawn));
    EXPECT_FALSE(
        MarkDungeonSpawnPointDirtyIfEditable(invalid_slot, spawn, true));
    EXPECT_FALSE(spawn.dirty());
  }
}

}  // namespace
}  // namespace yaze::editor
