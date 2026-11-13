#include "zelda3/overworld/overworld_version_helper.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

#include "absl/status/status.h"
#include "core/asar_wrapper.h"
#include "fixtures/lyra/native_directory_fixture.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "test_utils/overworld_model_test_peer.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_allocation.h"
#include "zelda3/overworld/overworld_layout.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_sprite_io.h"
#include "zelda3/overworld/overworld_upgrade_system.h"
#include "zelda3/overworld/terrain_graphics.h"

namespace yaze::zelda3 {

namespace {

void PatchEaTailOperands(Rom& rom) {
  const TailMapExpansionPlan plan;
  for (size_t i = 0; i < plan.operand_snes.size(); ++i) {
    const uint32_t pc = SnesToPc(plan.operand_snes[i]);
    ASSERT_LT(pc + 2, rom.size());
    rom[pc - 1] = 0xBF;
    const uint32_t patched = plan.operand_patched[i];
    rom[pc] = static_cast<uint8_t>(patched & 0xFF);
    rom[pc + 1] = static_cast<uint8_t>((patched >> 8) & 0xFF);
    rom[pc + 2] = static_cast<uint8_t>((patched >> 16) & 0xFF);
  }
  constexpr uint32_t stream_pc = 0x058000;
  const std::vector<uint8_t> stream = {0xE4, 0xFF, 0x00, 0xFF};
  std::copy(stream.begin(), stream.end(), rom.begin() + stream_pc);
  for (int map = 0; map < kExpandedMapCount; ++map) {
    ASSERT_TRUE(
        rom.WriteLong(kExpandedPtrTableHigh + map * 3, PcToSnes(stream_pc))
            .ok());
    ASSERT_TRUE(
        rom.WriteLong(kExpandedPtrTableLow + map * 3, PcToSnes(stream_pc))
            .ok());
  }
}

void InitOverworldTestRom(Rom& rom, uint8_t asm_version = 0xFF) {
  std::vector<uint8_t> data(2 * 1024 * 1024, 0);
  data[OverworldCustomASMHasBeenApplied] = asm_version;
  data[kMap16ExpandedFlagPos] = 0x0F;
  data[kMap32ExpandedFlagPos] = 0x04;
  data[kOverworldEntranceExpandedFlagPos] = 0xB8;
  EXPECT_TRUE(rom.LoadFromData(data).ok());
}

// Point every map at the same synthetic item stream in the last ROM bank.
bool PointItemsAtRomEnd(Rom& rom, uint32_t stream_pc) {
  const uint32_t stream_snes = PcToSnes(stream_pc);
  if (!rom.WriteLong(overworldItemsAddress,
                     PcToSnes(kOverworldItemsPointersNew))
           .ok() ||
      !rom.WriteByte(overworldItemsAddressBank, stream_snes >> 16).ok()) {
    return false;
  }
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    if (!rom.WriteShort(kOverworldItemsPointersNew + i * 2,
                        stream_snes & 0xFFFF)
             .ok()) {
      return false;
    }
  }
  return true;
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

TEST(OverworldReadOnlyTest, VanillaDiggableDefaultsDoNotReadExpandedStorage) {
  Rom rom;
  std::vector<uint8_t> bytes(1024 * 1024, 0);
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  Overworld ow(&rom);
  ASSERT_TRUE(ow.LoadDiggableTiles().ok());
  EXPECT_GT(ow.diggable_tiles().GetDiggableCount(), 0);
  EXPECT_EQ(rom.vector(), bytes);
}

TEST(OverworldReadOnlyTest, TransparencyUsesSourceZeroAndPreservesOpaqueEight) {
  std::vector<uint8_t> decoded(3 * 4096, 7);
  decoded[2 * 4096] = 0;
  decoded[2 * 4096 + 1] = 8;
  // Tile 192 is in upper-palette slot 3; source zero would preview as index 8.
  auto mask = Tile8SourceTransparency(decoded, 2, 192, false, false);
  ASSERT_TRUE(mask.ok());
  EXPECT_EQ((*mask)[0], 1);
  EXPECT_EQ((*mask)[1], 0);
  EXPECT_EQ((*mask)[2], 0);
  auto lower = Tile8SourceTransparency(decoded, 2, 64, false, false);
  ASSERT_TRUE(lower.ok());
  EXPECT_EQ(*lower, *mask);
  auto flipped = Tile8SourceTransparency(decoded, 2, 192, true, true);
  ASSERT_TRUE(flipped.ok());
  EXPECT_EQ((*flipped)[63], 1);
  EXPECT_EQ((*flipped)[62], 0);
}

TEST(OverworldReadOnlyTest, TransparencyReadsExplicitSheetZero) {
  std::vector<uint8_t> decoded(2 * 4096, 0);
  decoded[1] = 5;
  auto mask = Tile8SourceTransparency(decoded, 0, 0, false, false);
  ASSERT_TRUE(mask.ok());
  EXPECT_EQ((*mask)[0], 1);
  EXPECT_EQ((*mask)[1], 0);
}

TEST(OverworldReadOnlyTest, TransparencyRejectsUnloadedOrMissingSheets) {
  const std::vector<uint8_t> decoded(2 * 4096, 0);
  EXPECT_FALSE(Tile8SourceTransparency(decoded, -1, 0, false, false).ok());
  EXPECT_FALSE(Tile8SourceTransparency(decoded, 2, 0, false, false).ok());
  EXPECT_FALSE(Tile8SourceTransparency(decoded, 0, -1, false, false).ok());
  EXPECT_FALSE(Tile8SourceTransparency(decoded, 1, 1024, false, false).ok());
  const std::vector<uint8_t> failed_sheet(2 * 4096, 0xFF);
  EXPECT_FALSE(Tile8SourceTransparency(failed_sheet, 1, 0, false, false).ok());
}

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

TEST(OverworldRomProfileTest, LegacyV1AndV2StaySeparateFromLyra) {
  Rom rom;
  for (const uint8_t version : {0x01, 0x02}) {
    InitOverworldTestRom(rom, version);
    const auto profile = DetectOverworldRomProfile(rom);
    EXPECT_EQ(profile.layout_kind, OverworldRomProfile::LayoutKind::kLegacy);
    EXPECT_EQ(profile.lyra_layout.presence,
              yaze::rom::LyraLayoutPresence::kAbsent);
    EXPECT_EQ(profile.editable_map_count, kNumOverworldMaps);
    EXPECT_TRUE(rom.CheckLayoutWritable().ok());
  }
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
  PatchEaTailOperands(rom);
  profile = DetectOverworldRomProfile(rom);
  EXPECT_TRUE(profile.has_tail_map_expansion);
  EXPECT_EQ(profile.editable_map_count, kExpandedMapCount);
  EXPECT_EQ(profile.materialized_map_count, kNumOverworldMaps);
}

TEST(OverworldRomProfileTest, V5CollisionByteCannotMasqueradeAsTailMarker) {
  Rom rom;
  InitOverworldTestRom(rom, 0x05);
  rom[GetExpandedPtrTableMarker()] = GetExpandedPtrTableMagic();
  const auto before = rom.vector();
  const auto profile = DetectOverworldRomProfile(rom);
  EXPECT_EQ(profile.layout_kind,
            OverworldRomProfile::LayoutKind::kZScreamPerSheet);
  EXPECT_EQ(profile.zscream_layout, zscream::LayoutDialect::kZScreamV5);
  EXPECT_FALSE(profile.has_tail_map_expansion);
  EXPECT_EQ(profile.editable_map_count, kNumOverworldMaps);
  EXPECT_FALSE(IsSpecialTailMapExpansionEnabled(rom));
  Overworld overworld(&rom);
  EXPECT_FALSE(overworld.HasExpandedPointerTables());
  EXPECT_EQ(rom.vector(), before);
}

TEST(OverworldRomProfileTest, UnknownCustomMarkersCannotEnableTailExpansion) {
  for (uint8_t marker : {0x04, 0x06, 0xFE}) {
    Rom rom;
    InitOverworldTestRom(rom, marker);
    rom[GetExpandedPtrTableMarker()] = GetExpandedPtrTableMagic();
    const auto profile = DetectOverworldRomProfile(rom);
    EXPECT_EQ(profile.layout_kind,
              OverworldRomProfile::LayoutKind::kUnknownCustom);
    EXPECT_FALSE(profile.has_tail_map_expansion);
    EXPECT_FALSE(Overworld(&rom).HasExpandedPointerTables());
  }
}

TEST(OverworldRomProfileTest, CustomLayoutsRejectBundledAsmBeforeMutation) {
  for (uint8_t marker : {0x04, 0x05, 0x06, 0xFE}) {
    Rom rom;
    InitOverworldTestRom(rom, marker);
    rom.set_dirty(false);
    const auto before = rom.vector();
    OverworldUpgradeSystem upgrade(rom);
    EXPECT_EQ(upgrade.ApplyZSCustomOverworldASM(3).code(),
              absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(rom.vector(), before);
    EXPECT_FALSE(rom.dirty());
  }
}

namespace {

std::filesystem::path UpgradeAsmDirectory() {
  return std::filesystem::path(__FILE__)
             .parent_path()
             .parent_path()
             .parent_path()
             .parent_path() /
         "assets" / "asm";
}

void SeedUpgradeSource(Rom& rom, bool v2) {
  std::vector<uint8_t> bytes(v2 ? 0x200000 : 0x100000, 0xFF);
  bytes[0x7FD5] = 0x20;  // US LoROM; fixture has no copyrighted ROM contents.
  bytes[0x7FD9] = 1;
  std::copy_n(std::array<uint8_t, 4>{0xA8, 0xB9, 0x1D, 0xF5}.begin(), 4,
              bytes.begin() + 0x3B518);
  const auto word = [&bytes](int pc, uint16_t value) {
    bytes[pc] = value & 0xFF;
    bytes[pc + 1] = value >> 8;
  };
  for (int i = 0; i < 64; ++i)
    bytes[kOverworldMapParentId + i] = i;
  std::fill_n(bytes.begin() + kOverworldScreenSize, 128, 1);
  // One authored 2x2 area not in the reset list; DW remains small. A shared
  // legacy parent entry is irrelevant when the DW screen's size is small.
  for (int i : {0x22, 0x23, 0x2A, 0x2B}) {
    bytes[kOverworldMapParentId + i] = 0x22;
    bytes[kOverworldScreenSize + i] = 0;
  }
  // A separate DW area: its LW equivalents remain small.
  for (int i : {0x44, 0x45, 0x4C, 0x4D}) {
    bytes[kOverworldMapParentId + (i & 63)] = 4;
    bytes[kOverworldScreenSize + i] = 0;
  }
  for (int table :
       {kOverworldScreenTileMapChangeByScreen1,
        kOverworldScreenTileMapChangeByScreen2,
        kOverworldScreenTileMapChangeByScreen3,
        kOverworldScreenTileMapChangeByScreen4, kOverworldTransitionPositionY,
        kOverworldTransitionPositionX, 0x13EE2, 0x13F62})
    for (int i = 0; i < 64; ++i)
      word(table + i * 2, static_cast<uint16_t>(table + i));
  for (int i = 0; i < 128; ++i) {
    bytes[kAreaGfxIdPtr + i] = i % 0x30;
    bytes[kOverworldMapPaletteIds + i] = i % 0x20;
  }
  for (int i = 0; i < 16; ++i) {
    bytes[kOverworldSpecialGFXGroup + i] = 0x10 + i;
    bytes[kOverworldSpecialPALGroup + i] = i;
    bytes[kOverworldSpecialSpriteGFXGroup + i] = i;
    bytes[kOverworldSpecialSpritePalette + i] = i;
  }
  for (const auto [table, count] :
       {std::pair{0x4C881, 64}, std::pair{0x4C901, 144},
        std::pair{0x4CA21, 144}})
    for (int i = 0; i < count; ++i)
      word(table + i * 2, i == 2 ? 0xCB50 : 0xCB41);
  bytes[0x4CB41] = 0xFF;
  bytes[0x4CB50] = 3;
  bytes[0x4CB51] = 4;
  bytes[0x4CB52] = 0x0B;
  bytes[0x4CB53] = 0xFF;
  word(kRoomSpritePointerTableOperand, 0xD2B2);
  for (int i = 0; i < 144; ++i)
    word(kOverworldMessageIds + i * 2, 0x80 + i);
  word(0x166E1, 0x0061);
  word(0x166E3, 0x0073);  // Authored special camera origin, moved by real ASM.
  if (v2) {
    core::AsarWrapper asar;
    ASSERT_TRUE(asar.Initialize().ok());
    const auto patched = asar.ApplyPatch(
        (UpgradeAsmDirectory() / "ZSCustomOverworld.asm").string(), bytes);
    ASSERT_TRUE(patched.ok()) << patched.status();
    ASSERT_TRUE(patched->success);
    std::fill(bytes.begin() + 0x140000, bytes.begin() + 0x140998, 0);
    bytes[OverworldCustomASMHasBeenApplied] = 2;
    bytes[OverworldCustomAreaSpecificBGEnabled] = 0xFF;
    bytes[OverworldCustomMainPaletteEnabled] = 1;
    bytes[OverworldCustomAnimatedGFXEnabled] = 1;
    bytes[OverworldCustomSubscreenOverlayEnabled] = 1;
    bytes[OverworldCustomTileGFXGroupEnabled] = 1;
    bytes[OverworldCustomMosaicEnabled] = 1;  // Previously unused feature.
    bytes[OverworldCustomMosaicArray + 9] = 0x0A;
    word(OverworldCustomAreaSpecificBGPalette + 6, 0x1234);
    bytes[OverworldCustomMainPaletteArray + 7] = 4;
    bytes[OverworldCustomTileGFXGroupArray + 11] = 0x3A;
  }
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  rom.set_filename("synthetic-upgrade-source.sfc");
  rom.set_dirty(false);
}

uint16_t UpgradeWord(const std::vector<uint8_t>& bytes, int pc) {
  return bytes[pc] | (bytes[pc + 1] << 8);
}

}  // namespace

TEST(OverworldUpgradeServiceTest, ActualAsarPreservesCustomGeometryAndPools) {
  ASSERT_TRUE(std::filesystem::exists(UpgradeAsmDirectory() / "yaze.asm"));
  for (const bool v2 : {false, true}) {
    SCOPED_TRACE(v2);
    Rom rom;
    ASSERT_NO_FATAL_FAILURE(SeedUpgradeSource(rom, v2));
    const auto original = rom.vector();
    OverworldUpgradeSystem upgrade(rom, UpgradeAsmDirectory().string());
    // This calls the real bundled ASM through AsarWrapper, not a marker stub.
    const auto status = upgrade.ApplyZSCustomOverworldASM(3);
    ASSERT_TRUE(status.ok()) << status;
    EXPECT_TRUE(rom.dirty());
    EXPECT_EQ(rom.filename(), "synthetic-upgrade-source.sfc");
    const auto& output = rom.vector();
    EXPECT_EQ(output[OverworldCustomASMHasBeenApplied], 3);
    EXPECT_EQ(output.size(), 0x200000u);
    EXPECT_EQ(output[0x7FD7], 0x0B);
    uint32_t checksum = 0;
    for (uint8_t value : output)
      checksum += value;
    EXPECT_EQ(UpgradeWord(output, 0x7FDE), checksum & 0xFFFF);
    EXPECT_EQ(UpgradeWord(output, 0x7FDC) ^ UpgradeWord(output, 0x7FDE),
              0xFFFF);
    const std::array<uint8_t, 4> special_camera = {0x61, 0, 0x73, 0};
    const auto camera =
        std::search(output.begin(), output.end(), special_camera.begin(),
                    special_camera.end());
    ASSERT_NE(camera, output.end());
    EXPECT_NE(std::distance(output.begin(), camera), 0x166E1);
    for (int i = 0; i < 128; ++i) {
      EXPECT_EQ(output[kOverworldScreenSize + i],
                original[kOverworldScreenSize + i] == 0 ? 1 : 0)
          << i;
      const int expected_parent =
          original[kOverworldScreenSize + i] == 0
              ? (i & 0x40) + original[kOverworldMapParentId + (i & 63)]
              : i;
      EXPECT_EQ(output[kOverworldMapParentIdExpanded + i], expected_parent)
          << i;
      EXPECT_EQ(output[kOverworldPalettesScreenToSetNew + i],
                original[kOverworldMapPaletteIds + i])
          << i;
    }
    for (int i : {0x81, 0x82, 0x89, 0x8A}) {
      EXPECT_EQ(output[kOverworldMapParentIdExpanded + i], 0x81);
      EXPECT_EQ(output[kOverworldScreenSize + i], 1);
    }
    EXPECT_EQ(output[kOverworldScreenSize + 0x80], 0);
    EXPECT_EQ(output[kOverworldMapParentIdExpanded + 0x83], 0x83);
    const std::array<int, 8> old_tables = {
        kOverworldScreenTileMapChangeByScreen1,
        kOverworldScreenTileMapChangeByScreen2,
        kOverworldScreenTileMapChangeByScreen3,
        kOverworldScreenTileMapChangeByScreen4,
        kOverworldTransitionPositionY,
        kOverworldTransitionPositionX,
        0x13EE2,
        0x13F62};
    const std::array<int, 8> new_tables = {
        kOverworldScreenTileMapChangeByScreen1Expanded,
        kOverworldScreenTileMapChangeByScreen2Expanded,
        kOverworldScreenTileMapChangeByScreen3Expanded,
        kOverworldScreenTileMapChangeByScreen4Expanded,
        kOverworldTransitionPositionYExpanded,
        kOverworldTransitionPositionXExpanded,
        transition_target_northExpanded,
        transition_target_westExpanded};
    for (int i = 0; i < 128; ++i)
      for (size_t table = 0; table < old_tables.size(); ++table)
        EXPECT_EQ(UpgradeWord(output, new_tables[table] + i * 2),
                  UpgradeWord(original, old_tables[table] + (i & 63) * 2));
    EXPECT_EQ(UpgradeWord(output, 0x141438 + 2 * 2), 0xCB50);
    EXPECT_EQ(UpgradeWord(output, 0x141438 + 100 * 2), 0xCB41);
    EXPECT_EQ(UpgradeWord(output, 0x141578 + 143 * 2),
              UpgradeWord(original, 0x4C901 + 143 * 2));
    EXPECT_EQ(UpgradeWord(output, 0x1416B8 + 159 * 2), 0xCB41);
    EXPECT_TRUE(std::equal(original.begin() + 0x4CB41,
                           original.begin() + 0x4CB54,
                           output.begin() + 0x4CB41));
    EXPECT_EQ(UpgradeWord(output, kOverworldMessagesExpanded + 0x83 * 2),
              UpgradeWord(original, kOverworldMessageIds + 0x83 * 2));
    if (v2) {
      auto expected_pool = std::vector<uint8_t>(original.begin() + 0x140000,
                                                original.begin() + 0x140998);
      expected_pool[OverworldCustomASMHasBeenApplied - 0x140000] = 3;
      expected_pool[OverworldCustomMosaicEnabled - 0x140000] = 0;
      EXPECT_TRUE(std::equal(expected_pool.begin(), expected_pool.end(),
                             output.begin() + 0x140000));
    } else {
      for (int flag :
           {OverworldCustomAreaSpecificBGEnabled,
            OverworldCustomMainPaletteEnabled,
            OverworldCustomAnimatedGFXEnabled,
            OverworldCustomSubscreenOverlayEnabled,
            OverworldCustomTileGFXGroupEnabled, OverworldCustomMosaicEnabled})
        EXPECT_EQ(output[flag], 0);
    }
    const auto path =
        std::filesystem::temp_directory_path() /
        ("yaze-synthetic-upgrade-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".sfc");
    struct CleanupRom {
      std::filesystem::path path;
      ~CleanupRom() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
      }
    } cleanup{path};
    Rom::SaveSettings settings;
    settings.filename = path.string();
    ASSERT_TRUE(rom.SaveToFile(settings).ok());
    Rom reopened;
    ASSERT_TRUE(reopened.LoadFromFile(path.string()).ok());
    for (int i = 0; i < 160; ++i) {
      OverworldMap map(i, &reopened, nullptr, core::RomAddressOverrides{});
      EXPECT_EQ(map.parent(), output[kOverworldMapParentIdExpanded + i]);
      EXPECT_EQ(static_cast<int>(map.area_size()),
                output[kOverworldScreenSize + i]);
    }
    const auto once = rom.vector();
    EXPECT_EQ(upgrade.ApplyZSCustomOverworldASM(3).code(),
              absl::StatusCode::kAlreadyExists);
    EXPECT_EQ(rom.vector(), once);
  }
}

TEST(OverworldUpgradeServiceTest, RejectsInvalidSourcesWithoutMutation) {
  for (int which = 0; which < 10; ++which) {
    SCOPED_TRACE(which);
    Rom rom;
    ASSERT_NO_FATAL_FAILURE(SeedUpgradeSource(rom, true));
    if (which == 0)
      rom[kOverworldScreenSize] = 2;
    if (which == 1)
      rom[kOverworldMapParentId + 0x23] = 0x3F;
    if (which == 2)
      rom[OverworldCustomASMHasBeenApplied] = 1;
    if (which == 3)
      rom[0x7FD9] = 0;
    if (which == 4)
      rom.Expand(0x140200);
    if (which == 5) {
      test::native_overworld::NativeDirectoryFixture fixture;
      ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
    }
    if (which == 6)
      rom[kOverworldMessagesExpanded] ^= 1;
    if (which == 7)
      rom[kOverworldMapParentIdExpanded] = 0x22;
    if (which == 8)
      rom[0x3B518] = 0x22;  // Custom message loader, unknown target.
    if (which == 9) {
      ASSERT_NO_FATAL_FAILURE(SeedUpgradeSource(rom, false));
      rom.Expand(0x200000);
      rom[0x141800] = 0x42;  // Vanilla-owned content in the new message span.
    }
    rom.set_dirty(true);
    const auto before = rom.vector();
    const auto size = rom.size();
    const auto filename = rom.filename();
    OverworldUpgradeSystem upgrade(rom, UpgradeAsmDirectory().string());
    EXPECT_FALSE(upgrade.ApplyZSCustomOverworldASM(3).ok());
    EXPECT_EQ(rom.vector(), before);
    EXPECT_EQ(rom.size(), size);
    EXPECT_EQ(rom.filename(), filename);
    EXPECT_TRUE(rom.dirty());
  }
}

TEST(OverworldUpgradeServiceTest,
     ActualAsarFailureLeavesCompleteSourceUntouched) {
  Rom rom;
  ASSERT_NO_FATAL_FAILURE(SeedUpgradeSource(rom, false));
  const auto before = rom.vector();
  const auto temporary =
      std::filesystem::temp_directory_path() /
      ("yaze-upgrade-failure-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  ASSERT_TRUE(std::filesystem::create_directory(temporary));
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code ec;
      std::filesystem::remove_all(path, ec);
    }
  } cleanup{temporary};
  {
    std::ofstream patch(temporary / "yaze.asm");
    ASSERT_TRUE(patch.good());
    patch << "incsrc \""
          << (UpgradeAsmDirectory() / "yaze.asm").generic_string()
          << "\"\nerror \"injected upgrade failure after real bundled ASM\"\n";
  }
  OverworldUpgradeSystem upgrade(rom, temporary.string());
  const auto status = upgrade.ApplyZSCustomOverworldASM(3);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.size(), before.size());
  EXPECT_EQ(rom.filename(), "synthetic-upgrade-source.sfc");
  EXPECT_FALSE(rom.dirty());
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

TEST(OverworldMap32StorageTest, ExpandedSaveKeepsOperandsThatAlreadyMatch) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  Overworld overworld(&rom);
  OverworldModelTestPeer::InitializeMapSlots(overworld, kNumOverworldMaps);
  PopulateUniqueTile32Maps(overworld.mutable_map_tiles(), 64);
  ASSERT_TRUE(overworld.CreateTile32Tilemap().ok());

  // ZScream/Oracle write the FastROM mirror ($84:8000) of the target; the
  // save must not rewrite it as the equivalent $04:8000.
  const uint32_t top_right = GetMap32TileTRExpanded();
  const uint32_t fast_rom = PcToSnes(top_right) | 0x800000;
  ASSERT_TRUE(rom.WriteLong(0x0176EC, fast_rom).ok());

  ASSERT_TRUE(overworld.SaveMap32Expanded().ok());
  EXPECT_EQ(*rom.ReadLong(0x0176EC), fast_rom);
  // An operand that points elsewhere is still relocated.
  EXPECT_EQ(*rom.ReadLong(0x0176F3), PcToSnes(top_right + 1));
  EXPECT_EQ(*rom.ReadLong(0x01779A), PcToSnes(GetMap32TileBRExpanded() + 5));
}

TEST(OverworldMap32StorageTest, ExpandedOverflowFailsBeforeAnyRomWrite) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  Overworld overworld(&rom);
  OverworldModelTestPeer::InitializeMapSlots(overworld, kNumOverworldMaps);
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
  OverworldModelTestPeer::InitializeMapSlots(overworld, kNumOverworldMaps);

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

TEST(OverworldEntranceCompatibilityTest,
     ExpandedHoleAndUnusedKindsRoundTripWithoutLegacyWrites) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  rom[kOverworldEntranceExpandedFlagPos] = 0;
  ASSERT_TRUE(rom.WriteShort(kOverworldEntranceMap, 0xBEEF).ok());
  std::vector<OverworldEntrance> entries(256);
  for (auto& entry : entries)
    entry.deleted = true;
  entries[200] =
      OverworldEntrance(336, 3216, 0x0B, 0xB0, ((9 * 64) + 21) * 2, false);
  entries[201] =
      OverworldEntrance(352, 3232, 0x23, 0xB0, ((10 * 64) + 22) * 2, true);
  ASSERT_TRUE(SaveEntrances(&rom, entries, true).ok());
  EXPECT_EQ(*rom.ReadWord(kOverworldEntranceMap), 0xBEEF);
  EXPECT_EQ(*rom.ReadWord(GetOverworldEntranceMapExpanded() + 201 * 2), 0x80B0);
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromData(rom.vector()).ok());
  const auto loaded = LoadEntrances(&reopened);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(loaded->at(200).map_id_, 0xB0);
  EXPECT_EQ(loaded->at(200).entrance_id_, 0x0B);
  EXPECT_FALSE(loaded->at(200).deleted);
  EXPECT_FALSE(loaded->at(200).is_hole_);
  EXPECT_EQ(loaded->at(201).map_id_, 0xB0);
  EXPECT_TRUE(loaded->at(201).is_hole_);
  EXPECT_FALSE(loaded->at(201).deleted);
  EXPECT_TRUE(loaded->at(202).deleted);
  EXPECT_FALSE(loaded->at(202).is_hole_);
  const auto before = reopened.vector();
  ASSERT_TRUE(SaveEntrances(&reopened, *loaded, true).ok());
  EXPECT_EQ(reopened.vector(), before);
}

TEST(OverworldEntranceCompatibilityTest,
     NormalSavePreservesExpandedIdsInsteadOfWritingLegacyHoles) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  rom[kOverworldEntranceExpandedFlagPos] = 0;
  Overworld overworld(&rom);
  auto& entrances = *overworld.mutable_entrances();
  entrances.resize(256);
  for (auto& entry : entrances)
    entry.deleted = true;
  entrances[200] =
      OverworldEntrance(336, 3216, 0x0B, 0xB0, ((9 * 64) + 21) * 2, false);
  entrances[201] =
      OverworldEntrance(352, 3232, 0x23, 0xB0, ((10 * 64) + 22) * 2, true);
  // Legacy holes are not separate storage in an expanded layout. Their
  // loaded values must never overwrite combined-table ID rows 161-255.
  auto holes = LoadHoles(&rom);
  ASSERT_TRUE(holes.ok()) << holes.status();
  *overworld.mutable_holes() = *holes;

  ASSERT_TRUE(overworld.SaveEntrances().ok());
  const auto first_save = rom.vector();
  ASSERT_TRUE(overworld.SaveEntrances().ok());
  EXPECT_EQ(rom.vector(), first_save);
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromData(rom.vector()).ok());
  auto loaded = LoadEntrances(&reopened);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->size(), 256);
  EXPECT_EQ(loaded->at(200).entrance_id_, 0x0B);
  EXPECT_EQ(loaded->at(201).entrance_id_, 0x23);
  EXPECT_EQ(loaded->at(201).map_id_, 0xB0);
  EXPECT_TRUE(loaded->at(201).is_hole_);
  const auto before = rom.vector();
  EXPECT_EQ(SaveHoles(&rom, overworld.holes()).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom.vector(), before);
}

TEST(OverworldEntranceCompatibilityTest,
     ExpandedNormalSaveRejectsEditedLegacyHolesBeforeAnyWrite) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  rom[kOverworldEntranceExpandedFlagPos] = 0;
  Overworld overworld(&rom);
  overworld.mutable_entrances()->resize(256);
  auto holes = LoadHoles(&rom);
  ASSERT_TRUE(holes.ok()) << holes.status();
  *overworld.mutable_holes() = *holes;
  const auto before = rom.vector();
  // Cover a raw position edit and a stale packed position after a UI move.
  overworld.mutable_holes()->at(0).entrance_id_ ^= 1;
  EXPECT_EQ(overworld.SaveEntrances().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom.vector(), before);
  *overworld.mutable_holes() = *holes;
  overworld.mutable_holes()->at(0).x_ += 16;
  EXPECT_EQ(overworld.SaveEntrances().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom.vector(), before);
  *overworld.mutable_holes() = *holes;
  std::swap(overworld.mutable_holes()->at(0), overworld.mutable_holes()->at(1));
  EXPECT_EQ(overworld.SaveEntrances().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom.vector(), before);
  overworld.mutable_holes()->clear();
  EXPECT_EQ(overworld.SaveEntrances().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom.vector(), before);
  *overworld.mutable_holes() = *holes;
  ASSERT_TRUE(overworld.SaveEntrances().ok());
}

TEST(OverworldEntranceCompatibilityTest, NormalSaveStillWritesLegacyHoles) {
  Rom rom;
  InitOverworldTestRom(rom, 0xFF);
  Overworld overworld(&rom);
  overworld.mutable_entrances()->resize(kNumOverworldEntrances);
  overworld.mutable_holes()->assign(
      kNumOverworldHoles, OverworldEntrance(32, 64, 0x23, 0, 0x604, true));
  ASSERT_TRUE(overworld.SaveEntrances().ok());
  auto loaded = LoadHoles(&rom);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(loaded->at(0).entrance_id_, 0x23);
  EXPECT_EQ(loaded->at(0).map_pos_, 0x604);
  EXPECT_TRUE(loaded->at(0).is_hole_);
}

TEST(OverworldEntranceCompatibilityTest,
     IncompleteTablesAndHoleVectorsRefuseBeforeMutation) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(100, 0x5A)).ok());
  const auto before = rom.vector();
  EXPECT_FALSE(
      SaveEntrances(&rom, std::vector<OverworldEntrance>(256), true).ok());
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(SaveHoles(&rom, {}).ok());
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(LoadEntrances(nullptr).ok());
  EXPECT_FALSE(LoadHoles(nullptr).ok());
  EXPECT_FALSE(SaveEntrances(nullptr, {}, false).ok());
}

TEST(OverworldItemCompatibilityTest, LoadTerminatorAtExactRomEnd) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  constexpr uint32_t kRomSize = 2 * 1024 * 1024;
  ASSERT_EQ(rom.vector().size(), kRomSize);
  ASSERT_TRUE(PointItemsAtRomEnd(rom, kRomSize - 2));
  ASSERT_TRUE(rom.WriteShort(kRomSize - 2, 0xFFFF).ok());
  std::vector<OverworldMap> maps;
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    maps.emplace_back(i, &rom);
    maps.back().SetAsSmallMap(i);
  }
  auto loaded = LoadItems(&rom, maps);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_TRUE(loaded->empty());
}

TEST(OverworldItemCompatibilityTest, LoadRecordBeforeTerminatorAtRomEnd) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  constexpr uint32_t kRomSize = 2 * 1024 * 1024;
  ASSERT_EQ(rom.vector().size(), kRomSize);
  ASSERT_TRUE(PointItemsAtRomEnd(rom, kRomSize - 2));
  ASSERT_TRUE(rom.WriteShort(kRomSize - 2, 0xFFFF).ok());
  ASSERT_TRUE(rom.WriteShort(kOverworldItemsPointersNew,
                             PcToSnes(kRomSize - 5) & 0xFFFF)
                  .ok());
  ASSERT_TRUE(rom.WriteByte(kRomSize - 5, 2).ok());
  ASSERT_TRUE(rom.WriteByte(kRomSize - 4, 0).ok());
  ASSERT_TRUE(rom.WriteByte(kRomSize - 3, 0x2A).ok());
  std::vector<OverworldMap> maps;
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    maps.emplace_back(i, &rom);
    maps.back().SetAsSmallMap(i);
  }
  auto loaded = LoadItems(&rom, maps);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->size(), 1u);
  EXPECT_EQ((*loaded)[0].room_map_id_, 0);
  EXPECT_EQ((*loaded)[0].game_x_, 1);
  EXPECT_EQ((*loaded)[0].game_y_, 0);
  EXPECT_EQ((*loaded)[0].id_, 0x2A);
}

TEST(OverworldItemCompatibilityTest, RejectTruncatedItemStreamAtRomEnd) {
  for (const bool truncated_terminator : {false, true}) {
    SCOPED_TRACE(truncated_terminator);
    Rom rom;
    InitOverworldTestRom(rom, 0x03);
    constexpr uint32_t kRomSize = 2 * 1024 * 1024;
    ASSERT_EQ(rom.vector().size(), kRomSize);
    const uint32_t stream = kRomSize - (truncated_terminator ? 1 : 2);
    ASSERT_TRUE(PointItemsAtRomEnd(rom, stream));
    ASSERT_TRUE(rom.WriteByte(stream, truncated_terminator ? 0xFF : 0).ok());
    if (!truncated_terminator) {
      ASSERT_TRUE(rom.WriteByte(stream + 1, 0).ok());
    }
    std::vector<OverworldMap> maps;
    for (int i = 0; i < kNumOverworldMaps; ++i) {
      maps.emplace_back(i, &rom);
      maps.back().SetAsSmallMap(i);
    }
    const auto loaded = LoadItems(&rom, maps);
    EXPECT_FALSE(loaded.ok());
    EXPECT_EQ(loaded.status().code(), absl::StatusCode::kOutOfRange);
  }
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

TEST(OverworldItemCompatibilityTest,
     ModelSavePreservesAuthoredOrderForItemsAtTheSamePosition) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  const OverworldItem first(0x01, 0, 16, 32, false);
  const OverworldItem second(0x02, 0, 16, 32, false);
  ASSERT_TRUE(SaveItems(&rom, {first, second}).ok());

  // Construct only the metadata required by item loading; no graphics context.
  Overworld ow(&rom);
  auto& maps = const_cast<std::vector<OverworldMap>&>(ow.overworld_maps());
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    maps.emplace_back(i, &rom);
    maps.back().SetAsSmallMap(i);
  }
  *ow.mutable_all_items() = {second, first};
  const auto before = rom.vector();
  ASSERT_TRUE(ow.SaveItems().ok());
  EXPECT_NE(rom.vector(), before);

  auto reversed = LoadItems(&rom, maps);
  ASSERT_TRUE(reversed.ok());
  ASSERT_EQ(reversed->size(), 2u);
  EXPECT_EQ((*reversed)[0].id_, second.id_);
  EXPECT_EQ((*reversed)[1].id_, first.id_);
  EXPECT_EQ((*reversed)[0].game_x_, (*reversed)[1].game_x_);
  EXPECT_EQ((*reversed)[0].game_y_, (*reversed)[1].game_y_);

  // A subsequent reverse after saving must persist too.
  std::reverse(ow.mutable_all_items()->begin(), ow.mutable_all_items()->end());
  ASSERT_TRUE(ow.SaveItems().ok());
  auto restored = LoadItems(&rom, maps);
  ASSERT_TRUE(restored.ok());
  ASSERT_EQ(restored->size(), 2u);
  EXPECT_EQ((*restored)[0].id_, first.id_);
  EXPECT_EQ((*restored)[1].id_, second.id_);
}

TEST(OverworldItemCompatibilityTest,
     ModelSavePreservesRawAllocationWhenOnlyMapsAreInterleaved) {
  Rom rom;
  InitOverworldTestRom(rom, 0x03);
  const OverworldItem map0_first(0x01, 0, 16, 16, false);
  const OverworldItem map0_second(0x02, 0, 32, 16, false);
  const OverworldItem map1_first(0x03, 1, 512 + 16, 16, false);
  const OverworldItem map1_second(0x04, 1, 512 + 32, 16, false);
  ASSERT_TRUE(
      SaveItems(&rom, {map0_first, map0_second, map1_first, map1_second}).ok());

  // Keep a valid noncanonical allocation, so byte equality proves the no-op
  // path instead of merely reproducing the serializer's preferred layout.
  constexpr int kRelocatedMap0Items = 0x0DC780;
  auto pointer = rom.ReadWord(kOverworldItemsPointersNew);
  auto bank = rom.ReadByte(overworldItemsAddressBank);
  ASSERT_TRUE(pointer.ok());
  ASSERT_TRUE(bank.ok());
  const int original =
      SnesToPc((static_cast<uint32_t>(*bank) << 16) | *pointer);
  for (int i = 0; i < 8; ++i) {
    ASSERT_TRUE(rom.WriteByte(kRelocatedMap0Items + i, rom[original + i]).ok());
  }
  ASSERT_TRUE(rom.WriteShort(kOverworldItemsPointersNew,
                             PcToSnes(kRelocatedMap0Items) & 0xFFFF)
                  .ok());

  Overworld ow(&rom);
  auto& maps = const_cast<std::vector<OverworldMap>&>(ow.overworld_maps());
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    maps.emplace_back(i, &rom);
    maps.back().SetAsSmallMap(i);
  }
  *ow.mutable_all_items() = {map1_first, map0_first, map1_second, map0_second};
  const auto before = rom.vector();
  ASSERT_TRUE(ow.SaveItems().ok());
  EXPECT_EQ(rom.vector(), before);
}

}  // namespace yaze::zelda3
