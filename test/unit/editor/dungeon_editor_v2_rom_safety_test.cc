#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "core/features.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mocks/mock_rom.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::editor {

namespace {

struct DungeonSaveFlagsGuard {
  decltype(core::FeatureFlags::get().dungeon) prev =
      core::FeatureFlags::get().dungeon;
  ~DungeonSaveFlagsGuard() { core::FeatureFlags::get().dungeon = prev; }
};

void ConfigureMinimalDungeonSave() {
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = true;
  d.kSaveSprites = false;
  d.kSaveRoomHeaders = false;
  d.kSaveTorches = false;
  d.kSavePits = false;
  d.kSaveBlocks = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = false;
  d.kSaveChests = false;
  d.kSavePotItems = false;
  d.kSavePalettes = false;
}

}  // namespace

TEST(DungeonEditorV2RomSafetyTest, SaveUsesRoomIndicesNotInternalIds) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // No rooms are loaded, so SaveRoomData should be a no-op and never write.
  EXPECT_CALL(rom, WriteHelper(testing::_)).Times(0);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Inject a room with an out-of-range internal ID. Save must not use
  // room.id() for iteration (otherwise it would try SaveRoomData(999)).
  editor->rooms()[5] = zelda3::Room(999, &rom, nullptr);
  editor->rooms()[5].SetLoaded(false);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST(DungeonEditorV2RomSafetyTest, SaveRoomBlocksInvalidRoomBeforeWriting) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());
  EXPECT_CALL(rom, WriteHelper(testing::_)).Times(0);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.ClearTileObjects();
  room.AddTileObject(zelda3::RoomObject(/*id=*/0x01, /*x=*/70, /*y=*/0,
                                        /*size=*/0, /*layer=*/0));

  auto status = editor->SaveRoom(0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveWritesWaterFillZonesWhenCollisionSavingDisabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0].SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  ASSERT_TRUE(editor->rooms()[0].water_fill_dirty());

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = true;

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();

  EXPECT_FALSE(editor->rooms()[0].water_fill_dirty());
  EXPECT_EQ(editor->rooms()[0].water_fill_sram_bit_mask(), 0x01);

  auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  const auto& zones = *zones_or;
  ASSERT_EQ(zones.size(), 1u);
  EXPECT_EQ(zones[0].room_id, 0);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x01);
  ASSERT_EQ(zones[0].fill_offsets.size(), 1u);
  EXPECT_EQ(zones[0].fill_offsets[0], 65u);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveNormalizesDuplicateWaterFillMasksInsteadOfFailing) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0].SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  editor->rooms()[0].set_water_fill_sram_bit_mask(0x01);
  editor->rooms()[1].SetWaterFillTile(/*x=*/2, /*y=*/2, /*filled=*/true);
  editor->rooms()[1].set_water_fill_sram_bit_mask(
      0x01);  // Duplicate on purpose.

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = true;

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(editor->rooms()[0].water_fill_sram_bit_mask(), 0x01);
  EXPECT_EQ(editor->rooms()[1].water_fill_sram_bit_mask(), 0x02);

  auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  const auto& zones = *zones_or;
  ASSERT_EQ(zones.size(), 2u);
  EXPECT_EQ(zones[0].room_id, 0);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x01);
  EXPECT_EQ(zones[1].room_id, 1);
  EXPECT_EQ(zones[1].sram_bit_mask, 0x02);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveNormalizesInvalidWaterFillMaskInsteadOfFailing) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0].SetWaterFillTile(/*x=*/4, /*y=*/4, /*filled=*/true);
  editor->rooms()[0].set_water_fill_sram_bit_mask(
      0x03);  // Invalid (not single-bit).

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = true;

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(editor->rooms()[0].water_fill_sram_bit_mask(), 0x01);

  auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  const auto& zones = *zones_or;
  ASSERT_EQ(zones.size(), 1u);
  EXPECT_EQ(zones[0].room_id, 0);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x01);
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesIncludesWaterFillRegionWhenDirtyAndEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveWaterFillZones = true;

  EXPECT_TRUE(editor->CollectWriteRanges().empty());

  editor->rooms()[0].SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  auto ranges = editor->CollectWriteRanges();
  ASSERT_EQ(ranges.size(), 1u);
  EXPECT_EQ(ranges[0].first,
            static_cast<uint32_t>(zelda3::kWaterFillTableStart));
  EXPECT_EQ(ranges[0].second,
            static_cast<uint32_t>(zelda3::kWaterFillTableEnd));
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesIncludesCustomCollisionRegionsWhenDirtyAndEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = true;

  EXPECT_TRUE(editor->CollectWriteRanges().empty());

  editor->rooms()[0].SetCollisionTile(/*x=*/0, /*y=*/0, /*tile=*/0x08);
  auto ranges = editor->CollectWriteRanges();
  ASSERT_EQ(ranges.size(), 2u);
  EXPECT_EQ(ranges[0].first,
            static_cast<uint32_t>(zelda3::kCustomCollisionRoomPointers));
  EXPECT_EQ(ranges[0].second,
            static_cast<uint32_t>(zelda3::kCustomCollisionRoomPointers +
                                  (zelda3::kNumberOfRooms * 3)));
  EXPECT_EQ(ranges[1].first,
            static_cast<uint32_t>(zelda3::kCustomCollisionDataPosition));
  EXPECT_EQ(ranges[1].second,
            static_cast<uint32_t>(zelda3::kCustomCollisionDataSoftEnd));
}

TEST(DungeonEditorV2RomSafetyTest, UndoSnapshotLeakDetection) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.ClearTileObjects();

  // Add an initial object to make changes detectable.
  room.AddTileObject(zelda3::RoomObject(/*id=*/0x01, /*x=*/10, /*y=*/10,
                                        /*size=*/0, /*layer=*/0));

  // First BeginUndoSnapshot should work normally.
  editor->BeginUndoSnapshot(0);
  EXPECT_TRUE(editor->has_pending_undo_)
      << "has_pending_undo_ should be true after BeginUndoSnapshot";

  // Second BeginUndoSnapshot without FinalizeUndoAction should auto-finalize
  // the first snapshot (preventing leak) and start a new one.
  // This tests that double-Begin is handled gracefully.
  editor->BeginUndoSnapshot(0);
  EXPECT_TRUE(editor->has_pending_undo_)
      << "has_pending_undo_ should still be true after second "
         "BeginUndoSnapshot";

  // FinalizeUndoAction should clear the flag.
  editor->FinalizeUndoAction(0);
  EXPECT_FALSE(editor->has_pending_undo_)
      << "has_pending_undo_ should be false after FinalizeUndoAction";

  // BeginUndoSnapshot after FinalizeUndoAction should work normally again.
  editor->BeginUndoSnapshot(0);
  EXPECT_TRUE(editor->has_pending_undo_)
      << "has_pending_undo_ should be true after new BeginUndoSnapshot";

  // Clean up pending undo.
  editor->FinalizeUndoAction(0);
  EXPECT_FALSE(editor->has_pending_undo_)
      << "has_pending_undo_ should be false after final FinalizeUndoAction";
}

TEST(DungeonEditorV2RomSafetyTest, ViewerCacheLRUEviction) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // Disable workbench mode to test individual room viewers
  auto prev_workbench = core::FeatureFlags::get().dungeon.kUseWorkbench;
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Create 25 viewers (exceeds kMaxCachedViewers = 20)
  for (int i = 0; i < 25; ++i) {
    auto* viewer = editor->GetViewerForRoom(i);
    ASSERT_NE(viewer, nullptr) << "Failed to get viewer for room " << i;
  }

  // Verify that viewer count is at most kMaxCachedViewers
  EXPECT_LE(editor->room_viewers_.Size(),
            static_cast<size_t>(DungeonEditorV2::kMaxCachedViewers))
      << "Viewer cache should not exceed kMaxCachedViewers";

  // Verify LRU ordering: most recent rooms (20-24) should still be cached
  for (int i = 20; i < 25; ++i) {
    EXPECT_TRUE(editor->room_viewers_.Contains(i))
        << "Recent room " << i << " should still be cached";
  }

  // Verify oldest rooms (0-4) were evicted
  int evicted_count = 0;
  for (int i = 0; i < 5; ++i) {
    if (!editor->room_viewers_.Contains(i)) {
      evicted_count++;
    }
  }
  EXPECT_GT(evicted_count, 0) << "Some old rooms should have been evicted";

  // Restore workbench flag
  core::FeatureFlags::get().dungeon.kUseWorkbench = prev_workbench;
}

TEST(DungeonEditorV2RomSafetyTest, ViewerCacheNeverEvictsActiveRooms) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // Disable workbench mode to test individual room viewers
  auto prev_workbench = core::FeatureFlags::get().dungeon.kUseWorkbench;
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Mark room 0 as active
  editor->active_rooms_.push_back(0);

  // Get viewer for room 0
  auto* viewer0 = editor->GetViewerForRoom(0);
  ASSERT_NE(viewer0, nullptr);

  // Create 25 more viewers (should trigger eviction)
  for (int i = 1; i < 26; ++i) {
    auto* viewer = editor->GetViewerForRoom(i);
    ASSERT_NE(viewer, nullptr);
  }

  // Verify room 0 is still cached (it's active, so should not be evicted)
  EXPECT_TRUE(editor->room_viewers_.Contains(0))
      << "Active room 0 should never be evicted";

  // Verify the same viewer instance is returned
  EXPECT_EQ(editor->GetViewerForRoom(0), viewer0)
      << "Active room viewer should remain the same instance";

  // Restore workbench flag
  core::FeatureFlags::get().dungeon.kUseWorkbench = prev_workbench;
}

TEST(DungeonEditorV2RomSafetyTest, ViewerCacheLRUAccessOrderUpdate) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // Disable workbench mode to test individual room viewers
  auto prev_workbench = core::FeatureFlags::get().dungeon.kUseWorkbench;
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Create 20 viewers (exactly at limit)
  for (int i = 0; i < 20; ++i) {
    auto* viewer = editor->GetViewerForRoom(i);
    ASSERT_NE(viewer, nullptr);
  }

  EXPECT_EQ(editor->room_viewers_.Size(),
            static_cast<size_t>(DungeonEditorV2::kMaxCachedViewers));

  // Access room 0 again (should move it to the back of LRU)
  auto* viewer0 = editor->GetViewerForRoom(0);
  ASSERT_NE(viewer0, nullptr);

  // Create one more viewer (should evict room 1, not room 0)
  auto* viewer20 = editor->GetViewerForRoom(20);
  ASSERT_NE(viewer20, nullptr);

  // Verify room 0 is still cached (was recently accessed)
  EXPECT_TRUE(editor->room_viewers_.Contains(0))
      << "Recently accessed room 0 should not be evicted";

  // Verify room 1 was evicted (oldest in LRU after room 0 was accessed)
  EXPECT_FALSE(editor->room_viewers_.Contains(1))
      << "Room 1 should have been evicted as the oldest";

  // Restore workbench flag
  core::FeatureFlags::get().dungeon.kUseWorkbench = prev_workbench;
}

}  // namespace yaze::editor
