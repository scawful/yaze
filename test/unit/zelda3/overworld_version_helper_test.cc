#include "zelda3/overworld/overworld_version_helper.h"

#include <vector>

#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {

namespace {

void InitOverworldTestRom(Rom& rom, uint8_t asm_version = 0xFF) {
  std::vector<uint8_t> data(2 * 1024 * 1024, 0);
  data[OverworldCustomASMHasBeenApplied] = asm_version;
  data[kMap16ExpandedFlagPos] = 0x0F;
  data[kMap32ExpandedFlagPos] = 0x04;
  data[kOverworldEntranceExpandedFlagPos] = 0xB8;
  EXPECT_TRUE(rom.LoadFromData(data).ok());
}

void PopulateUniqueTile32Maps(OverworldMapTiles* maps,
                              int unique_definition_count = NumberOfMap32) {
  ASSERT_NE(maps, nullptr);
  const auto make_world = [] {
    return OverworldBlockset(512, std::vector<uint16_t>(512, 0));
  };
  maps->light_world = make_world();
  maps->dark_world = make_world();
  maps->special_world = make_world();

  uint16_t tile16_id = 1;
  int definition_index = 0;
  const auto fill = [&tile16_id, &definition_index, unique_definition_count](
                        OverworldBlockset* world, int width, int height) {
    for (int y = 0; y < height; y += 2) {
      for (int x = 0; x < width; x += 2) {
        // A distinct top-left Tile16 makes each packed Tile32 unique.
        if (definition_index < unique_definition_count) {
          (*world)[x][y] = tile16_id++;
        }
        ++definition_index;
      }
    }
  };
  fill(&maps->light_world, 256, 256);    // 64 screens
  fill(&maps->dark_world, 256, 256);     // 64 screens
  fill(&maps->special_world, 256, 128);  // 32 screens
}

}  // namespace

TEST(OverworldVersionHelperTest, GetVersion_VanillaSmallRom) {
  Rom rom;
  std::vector<uint8_t> data(1024 * 1024, 0);  // 1MB ROM
  auto status = rom.LoadFromData(data);
  ASSERT_TRUE(status.ok());

  // Should return Vanilla and not crash
  EXPECT_EQ(OverworldVersionHelper::GetVersion(rom),
            OverworldVersion::kVanilla);
}

TEST(OverworldVersionHelperTest, GetVersion_VanillaExpandedRom_ZeroFilled) {
  Rom rom;
  std::vector<uint8_t> data(2 * 1024 * 1024, 0);  // 2MB ROM, 0 filled
  auto status = rom.LoadFromData(data);
  ASSERT_TRUE(status.ok());

  EXPECT_EQ(OverworldVersionHelper::GetVersion(rom),
            OverworldVersion::kVanilla);
}

TEST(OverworldVersionHelperTest, GetVersion_VanillaExpandedRom_FF_Filled) {
  Rom rom;
  std::vector<uint8_t> data(2 * 1024 * 1024, 0xFF);  // 2MB ROM, 0xFF filled
  auto status = rom.LoadFromData(data);
  ASSERT_TRUE(status.ok());

  EXPECT_EQ(OverworldVersionHelper::GetVersion(rom),
            OverworldVersion::kVanilla);
}

TEST(OverworldVersionHelperTest, GetVersion_V1) {
  Rom rom;
  std::vector<uint8_t> data(2 * 1024 * 1024, 0);
  data[OverworldCustomASMHasBeenApplied] = 1;
  auto status = rom.LoadFromData(data);
  ASSERT_TRUE(status.ok());

  EXPECT_EQ(OverworldVersionHelper::GetVersion(rom),
            OverworldVersion::kZSCustomV1);
}

TEST(OverworldVersionHelperTest, GetVersion_V3) {
  Rom rom;
  std::vector<uint8_t> data(2 * 1024 * 1024, 0);
  data[OverworldCustomASMHasBeenApplied] = 3;
  auto status = rom.LoadFromData(data);
  ASSERT_TRUE(status.ok());

  EXPECT_EQ(OverworldVersionHelper::GetVersion(rom),
            OverworldVersion::kZSCustomV3);
}

TEST(OverworldVersionHelperTest, GetAsmVersion_SmallRomReturnsVanillaMarker) {
  Rom rom;
  std::vector<uint8_t> data(1024, 0);
  ASSERT_TRUE(rom.LoadFromData(data).ok());

  EXPECT_EQ(OverworldVersionHelper::GetAsmVersion(rom), 0xFF);
}

TEST(OverworldRomProfileTest, VanillaIgnoresExpansionFlagBytes) {
  Rom rom;
  InitOverworldTestRom(rom, 0xFF);
  rom[kMap16ExpandedFlagPos] = 0x3D;
  rom[kMap32ExpandedFlagPos] = 0x3E;
  rom[kOverworldEntranceExpandedFlagPos] = 0x00;
  rom[GetExpandedPtrTableMarker()] = GetExpandedPtrTableMagic();

  const auto profile = DetectOverworldRomProfile(rom);

  EXPECT_TRUE(profile.is_vanilla);
  EXPECT_FALSE(profile.has_expanded_tile16);
  EXPECT_FALSE(profile.has_expanded_tile32);
  EXPECT_FALSE(profile.has_expanded_entrances);
  EXPECT_FALSE(profile.has_tail_map_expansion);
  EXPECT_EQ(profile.editable_map_count, kNumOverworldMaps);
}

TEST(OverworldRomProfileTest, V2UsesExplicitExpansionFlags) {
  Rom rom;
  InitOverworldTestRom(rom, 0x02);

  auto profile = DetectOverworldRomProfile(rom);
  EXPECT_FALSE(profile.is_vanilla);
  EXPECT_FALSE(profile.supports_area_enum);
  EXPECT_FALSE(profile.has_expanded_tile16);
  EXPECT_FALSE(profile.has_expanded_tile32);
  EXPECT_FALSE(profile.has_expanded_entrances);

  rom[kMap16ExpandedFlagPos] = 0x3D;
  rom[kMap32ExpandedFlagPos] = 0x3E;
  rom[kOverworldEntranceExpandedFlagPos] = 0x00;

  profile = DetectOverworldRomProfile(rom);
  EXPECT_TRUE(profile.has_expanded_tile16);
  EXPECT_TRUE(profile.has_expanded_tile32);
  EXPECT_TRUE(profile.has_expanded_entrances);
}

TEST(OverworldRomProfileTest, V3ForcesExpandedTilesAndRequiresTailMarker) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);

  auto profile = DetectOverworldRomProfile(rom);
  EXPECT_TRUE(profile.supports_area_enum);
  EXPECT_TRUE(profile.has_expanded_tile16);
  EXPECT_TRUE(profile.has_expanded_tile32);
  EXPECT_FALSE(profile.has_tail_map_expansion);
  EXPECT_EQ(profile.editable_map_count, kNumOverworldMaps);

  rom[GetExpandedPtrTableMarker()] = GetExpandedPtrTableMagic();
  profile = DetectOverworldRomProfile(rom);
  EXPECT_TRUE(profile.has_tail_map_expansion);
  EXPECT_EQ(profile.editable_map_count, kExpandedMapCount);
}

TEST(OverworldMap32StorageTest, CapacityTracksDetectedRomProfile) {
  Rom rom;
  InitOverworldTestRom(rom, 0xFF);
  auto profile = DetectOverworldRomProfile(rom);
  EXPECT_EQ(Map32StorageBytesForProfile(profile),
            kMap32TileStorageBytesVanilla);
  EXPECT_EQ(Map32DefinitionCapacityForProfile(profile),
            kMap32DefinitionCapacityVanilla);
  EXPECT_EQ(kMap32DefinitionCapacityVanilla, 8864);

  InitOverworldTestRom(rom, 0x03);
  profile = DetectOverworldRomProfile(rom);
  EXPECT_EQ(Map32StorageBytesForProfile(profile),
            kMap32TileStorageBytesExpanded);
  EXPECT_EQ(Map32DefinitionCapacityForProfile(profile),
            kMap32DefinitionCapacityExpanded);
  EXPECT_EQ(kMap32DefinitionCapacityExpanded, 17728);
}

TEST(OverworldMap32StorageTest, ExpandedOverflowFailsBeforeAnyRomWrite) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  Overworld overworld(&rom);
  PopulateUniqueTile32Maps(overworld.mutable_map_tiles());
  const auto before = rom.vector();

  const auto create_status = overworld.CreateTile32Tilemap();
  EXPECT_EQ(create_status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_GT(overworld.tiles32_unique().size(),
            static_cast<size_t>(kMap32DefinitionCapacityExpanded));
  EXPECT_EQ(rom.vector(), before);

  // The public writer repeats the capacity preflight before relocation-pointer
  // or quadrant writes, even if a caller ignores the CreateTile32Tilemap error.
  const auto save_status = overworld.SaveMap32Expanded();
  EXPECT_EQ(save_status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom.vector(), before);
}

TEST(OverworldMap32StorageTest, JapaneseLayoutUsesAdjacentTableCapacity) {
  Rom rom;
  InitOverworldTestRom(rom, 0xFF);
  GameData game_data(&rom);
  game_data.version = zelda3_version::JP;
  Overworld overworld(&rom, &game_data);

  // 8,832 nonzero definitions plus the shared zero definition pad to 8,836.
  // This fits the historical US limit (8,864) but exceeds the JP quadrant
  // span: 0x33C0 bytes == 8,832 packed definitions.
  PopulateUniqueTile32Maps(overworld.mutable_map_tiles(), 8832);
  const auto before = rom.vector();

  const auto status = overworld.CreateTile32Tilemap();
  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted) << status;
  EXPECT_EQ(overworld.tiles32_unique().size(), 8836u);
  EXPECT_EQ(rom.vector(), before);
}

TEST(OverworldCompatibilityHelpersTest, LegacyParentTablesUseLocalMapIds) {
  EXPECT_TRUE(CanPersistLegacyMultiAreaMap(0x00));
  EXPECT_TRUE(CanPersistLegacyMultiAreaMap(0x7F));
  EXPECT_FALSE(CanPersistLegacyMultiAreaMap(0x80));

  EXPECT_EQ(LegacyParentTableIndexForMap(0x03), 0x03);
  EXPECT_EQ(LegacyParentTableIndexForMap(0x43), 0x03);
  EXPECT_EQ(LegacyParentTableValueForMap(0x43), 0x03);
}

TEST(OverworldCompatibilityHelpersTest,
     LegacyScreenSizeTablesKeepLightAndDarkSeparate) {
  EXPECT_TRUE(CanPersistLegacyScreenSize(0x00));
  EXPECT_TRUE(CanPersistLegacyScreenSize(0x7F));
  EXPECT_FALSE(CanPersistLegacyScreenSize(0x80));

  EXPECT_EQ(LegacyScreenSizeTableIndexForMap(0x03), 0x03);
  EXPECT_EQ(LegacyScreenSizeTableIndexForMap(0x43), 0x43);
}

TEST(OverworldMapCompatibilityTest, ConstructorLoadsParentBeforeAreaInfo) {
  Rom rom;
  InitOverworldTestRom(rom, 0xFF);
  rom[kOverworldMapParentId + 0x04] = 0x03;
  rom[kAreaGfxIdPtr + 0x43] = 0x55;
  rom[kAreaGfxIdPtr + 0x44] = 0x66;

  OverworldMap map(0x44, &rom);

  EXPECT_EQ(map.parent(), 0x43);
  EXPECT_EQ(map.area_graphics(), 0x55);
}

TEST(OverworldMapCompatibilityTest, LegacyDarkWorldReadsDarkScreenSizeEntry) {
  Rom rom;
  InitOverworldTestRom(rom, 0xFF);
  rom[kOverworldMapParentId + 0x04] = 0x04;
  rom[kOverworldScreenSize + 0x04] = 0x01;
  rom[kOverworldScreenSize + 0x44] = 0x00;

  OverworldMap map(0x44, &rom);

  EXPECT_EQ(map.parent(), 0x44);
  EXPECT_EQ(map.area_size(), AreaSizeEnum::LargeArea);
  EXPECT_TRUE(map.is_large_map());
}

TEST(OverworldMapCompatibilityTest,
     LegacySpecialWorldReadsSpecialSpriteTables) {
  Rom rom;
  InitOverworldTestRom(rom, 0xFF);
  rom[kOverworldSpecialSpriteGFXGroup + 0x03] = 0x12;
  rom[kOverworldSpecialSpritePalette + 0x03] = 0x05;

  OverworldMap map(0x83, &rom);

  EXPECT_EQ(map.sprite_graphics(0), 0x12);
  EXPECT_EQ(map.sprite_palette(0), 0x05);
}

TEST(OverworldMapCompatibilityTest, V2ReadsExpandedMessageButLegacySize) {
  Rom rom;
  InitOverworldTestRom(rom, 0x02);
  rom[kOverworldScreenSize + 0] = 0x02;
  rom[kOverworldMessageIds] = 0x11;
  rom[kOverworldMessageIds + 1] = 0x11;
  rom[GetOverworldMessagesExpanded()] = 0x22;
  rom[GetOverworldMessagesExpanded() + 1] = 0x22;

  OverworldMap map(0, &rom);

  EXPECT_EQ(map.message_id(), 0x2222);
  EXPECT_EQ(map.area_size(), AreaSizeEnum::SmallArea);
}

TEST(OverworldEntranceCompatibilityTest, VanillaIgnoresExpandedFlagByte) {
  Rom rom;
  InitOverworldTestRom(rom, 0xFF);
  rom[kOverworldEntranceExpandedFlagPos] = 0x00;

  auto entrances = LoadEntrances(&rom);

  ASSERT_TRUE(entrances.ok()) << entrances.status().message();
  EXPECT_EQ(entrances->size(), kNumOverworldEntrances);
}

TEST(OverworldEntranceCompatibilityTest, ExpandedSaveWritesAll256Entries) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  rom[kOverworldEntranceExpandedFlagPos] = 0x00;

  std::vector<OverworldEntrance> entrances;
  entrances.reserve(256);
  for (int i = 0; i < 256; ++i) {
    entrances.emplace_back(0, 0, static_cast<uint8_t>(i), i,
                           static_cast<uint16_t>(0x1000 + i), false);
  }

  ASSERT_TRUE(SaveEntrances(&rom, entrances, true).ok());

  const int index = 200;
  auto map_id = rom.ReadWord(GetOverworldEntranceMapExpanded() + (index * 2));
  auto map_pos = rom.ReadWord(GetOverworldEntrancePosExpanded() + (index * 2));
  auto entrance_id = rom.ReadByte(GetOverworldEntranceIdExpanded() + index);
  ASSERT_TRUE(map_id.ok());
  ASSERT_TRUE(map_pos.ok());
  ASSERT_TRUE(entrance_id.ok());
  EXPECT_EQ(*map_id, index);
  EXPECT_EQ(*map_pos, 0x1000 + index);
  EXPECT_EQ(*entrance_id, static_cast<uint8_t>(index));
}

TEST(OverworldItemCompatibilityTest,
     VanillaSaveLeavesSpecialWorldPointerSlotsUntouched) {
  Rom rom;
  InitOverworldTestRom(rom, 0xFF);
  constexpr uint16_t kSentinel = 0xBEEF;
  ASSERT_TRUE(rom.WriteShort(kOverworldItemsPointersNew +
                                 (kNumOverworldMapItemPointers * 2),
                             kSentinel)
                  .ok());

  ASSERT_TRUE(SaveItems(&rom, {}).ok());

  auto pointer_after = rom.ReadWord(kOverworldItemsPointersNew +
                                    (kNumOverworldMapItemPointers * 2));
  ASSERT_TRUE(pointer_after.ok());
  EXPECT_EQ(*pointer_after, kSentinel);
}

TEST(OverworldItemCompatibilityTest, V3SaveUsesSpecialWorldPointerSlots) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  constexpr uint16_t kSentinel = 0xBEEF;
  ASSERT_TRUE(rom.WriteShort(kOverworldItemsPointersNew +
                                 (kNumOverworldMapItemPointers * 2),
                             kSentinel)
                  .ok());

  ASSERT_TRUE(SaveItems(&rom, {}).ok());

  auto pointer_after = rom.ReadWord(kOverworldItemsPointersNew +
                                    (kNumOverworldMapItemPointers * 2));
  ASSERT_TRUE(pointer_after.ok());
  EXPECT_NE(*pointer_after, kSentinel);
}

}  // namespace yaze::zelda3
