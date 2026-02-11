#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <cstdint>
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
  decltype(core::FeatureFlags::get().dungeon) prev = core::FeatureFlags::get().dungeon;
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

  DungeonEditorV2 editor(&rom);

  // Inject a room with an out-of-range internal ID. Save must not use
  // room.id() for iteration (otherwise it would try SaveRoomData(999)).
  editor.rooms()[5] = zelda3::Room(999, &rom, nullptr);
  editor.rooms()[5].SetLoaded(false);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto status = editor.Save();
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST(DungeonEditorV2RomSafetyTest, SaveRoomBlocksInvalidRoomBeforeWriting) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());
  EXPECT_CALL(rom, WriteHelper(testing::_)).Times(0);

  DungeonEditorV2 editor(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto& room = editor.rooms()[0];
  room.SetLoaded(true);
  room.ClearTileObjects();
  room.AddTileObject(zelda3::RoomObject(/*id=*/0x01, /*x=*/70, /*y=*/0,
                                        /*size=*/0, /*layer=*/0));

  auto status = editor.SaveRoom(0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveWritesWaterFillZonesWhenCollisionSavingDisabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  DungeonEditorV2 editor(&rom);
  editor.rooms()[0].SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  ASSERT_TRUE(editor.rooms()[0].water_fill_dirty());

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = true;

  auto status = editor.Save();
  EXPECT_TRUE(status.ok()) << status.message();

  EXPECT_FALSE(editor.rooms()[0].water_fill_dirty());
  EXPECT_EQ(editor.rooms()[0].water_fill_sram_bit_mask(), 0x01);

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
     CollectWriteRangesIncludesWaterFillRegionWhenDirtyAndEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  DungeonEditorV2 editor(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveWaterFillZones = true;

  EXPECT_TRUE(editor.CollectWriteRanges().empty());

  editor.rooms()[0].SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  auto ranges = editor.CollectWriteRanges();
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

  DungeonEditorV2 editor(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = true;

  EXPECT_TRUE(editor.CollectWriteRanges().empty());

  editor.rooms()[0].SetCollisionTile(/*x=*/0, /*y=*/0, /*tile=*/0x08);
  auto ranges = editor.CollectWriteRanges();
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

}  // namespace yaze::editor
