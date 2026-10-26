#include "zelda3/overworld/zscream_custom_layout.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {
namespace {

namespace zs = zscream;

uint8_t CollisionValue(size_t sheet, size_t entry) {
  return static_cast<uint8_t>((sheet * 19 + entry * 7) & 0xFF);
}

std::vector<uint8_t> SyntheticLayout(uint8_t marker = 0x05) {
  std::vector<uint8_t> bytes(
      zs::kSheetCollisionTablePc + zs::kSheetCollisionTableSize, 0);
  bytes[zs::kVersionMarkerPc] = marker;
  for (size_t sheet = 0; sheet < zs::kCollisionSheetCount; ++sheet) {
    for (size_t entry = 0; entry < zs::kCollisionsPerSheet; ++entry) {
      bytes[zs::kSheetCollisionTablePc + sheet * zs::kCollisionsPerSheet +
            entry] = CollisionValue(sheet, entry);
    }
  }
  return bytes;
}

Rom LoadSynthetic(const std::vector<uint8_t>& bytes) {
  Rom rom;
  Rom::LoadOptions options;
  options.strip_header = false;
  options.load_resource_labels = false;
  EXPECT_TRUE(rom.LoadFromData(bytes, options).ok());
  return rom;
}

TEST(ZscreamCustomLayoutTest, DetectsNeutralV5DialectFromRawMarkerOnly) {
  const Rom rom = LoadSynthetic(SyntheticLayout());
  EXPECT_EQ(zs::DetectLayout(rom), zs::LayoutDialect::kZScreamV5);
  EXPECT_STREQ(zs::LayoutName(zs::DetectLayout(rom)),
               "ZScream V5 per-sheet collisions");
  EXPECT_FALSE(zs::CanUseTailExpansion(rom));
}

TEST(ZscreamCustomLayoutTest, ReadsFirstAndLastSheetWithoutChangingRom) {
  Rom rom = LoadSynthetic(SyntheticLayout());
  const auto before = rom.vector();
  const bool was_dirty = rom.dirty();
  for (int sheet : {0, 1, 63, 127}) {
    const auto result = zs::LoadSheetCollisions(rom, sheet);
    ASSERT_TRUE(result.ok()) << result.status();
    for (size_t entry = 0; entry < result->size(); ++entry) {
      EXPECT_EQ((*result)[entry], CollisionValue(sheet, entry))
          << "sheet " << sheet << ", entry " << entry;
    }
  }
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.dirty(), was_dirty);
}

TEST(ZscreamCustomLayoutTest, ComposesEightResolvedSheetsInNativeOrder) {
  const Rom rom = LoadSynthetic(SyntheticLayout());
  const zs::ResolvedSheetIds sheets = {127, 0, 31, 9, 64, 3, 1, 126};
  const auto result = zs::ComposeTile8Collisions(rom, sheets);
  ASSERT_TRUE(result.ok()) << result.status();
  ASSERT_EQ(result->size(), 0x200);
  // Reproduce the ASM's 32 little-endian word transfers per selected sheet.
  size_t destination = 0;
  for (uint8_t id : sheets) {
    const size_t source =
        zs::kSheetCollisionTablePc + ((static_cast<uint16_t>(id) << 8) >> 2);
    for (size_t word = 0; word < 0x20; ++word) {
      EXPECT_EQ((*result)[destination++], rom.vector()[source + word * 2]);
      EXPECT_EQ((*result)[destination++], rom.vector()[source + word * 2 + 1]);
    }
  }
  EXPECT_EQ(destination, 512);
  EXPECT_EQ((*result)[0], CollisionValue(127, 0));
  EXPECT_EQ((*result)[63], CollisionValue(127, 63));
  EXPECT_EQ((*result)[64], CollisionValue(0, 0));
  EXPECT_EQ((*result)[511], CollisionValue(126, 63));
}

TEST(ZscreamCustomLayoutTest, RepeatedResolvedSheetsRemainRepeated) {
  const Rom rom = LoadSynthetic(SyntheticLayout());
  const zs::ResolvedSheetIds sheets = {2, 2, 2, 2, 2, 2, 2, 2};
  const auto result = zs::ComposeTile8Collisions(rom, sheets);
  ASSERT_TRUE(result.ok()) << result.status();
  for (size_t index = 0; index < result->size(); ++index) {
    EXPECT_EQ((*result)[index], CollisionValue(2, index % 64));
  }
}

TEST(ZscreamCustomLayoutTest, RejectsWrongOrUnknownMarker) {
  for (uint8_t marker : {0x00, 0x01, 0x02, 0x03, 0x04, 0x06, 0xFE, 0xFF}) {
    const Rom rom = LoadSynthetic(SyntheticLayout(marker));
    const auto result = zs::LoadSheetCollisions(rom, 0);
    EXPECT_EQ(result.status().code(), absl::StatusCode::kFailedPrecondition)
        << "marker " << static_cast<int>(marker);
    const auto composed =
        zs::ComposeTile8Collisions(rom, {0, 1, 2, 3, 4, 5, 6, 7});
    EXPECT_EQ(composed.status().code(), absl::StatusCode::kFailedPrecondition);
  }
}

TEST(ZscreamCustomLayoutTest, RejectsShortTableEvenForFirstSheet) {
  auto bytes = SyntheticLayout();
  bytes.pop_back();
  const Rom rom = LoadSynthetic(bytes);
  EXPECT_EQ(zs::DetectLayout(rom), zs::LayoutDialect::kZScreamV5);
  const auto result = zs::LoadSheetCollisions(rom, 0);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kOutOfRange);
  EXPECT_EQ(
      zs::ComposeTile8Collisions(rom, {0, 1, 2, 3, 4, 5, 6, 7}).status().code(),
      absl::StatusCode::kOutOfRange);
}

TEST(ZscreamCustomLayoutTest, RejectsMissingMarkerAndUnloadedRom) {
  const Rom unloaded;
  EXPECT_EQ(zs::DetectLayout(unloaded), zs::LayoutDialect::kUnmarked);
  EXPECT_FALSE(zs::CanUseTailExpansion(unloaded));
  EXPECT_FALSE(zs::LoadSheetCollisions(unloaded, 0).ok());
  const Rom short_rom =
      LoadSynthetic(std::vector<uint8_t>(zs::kVersionMarkerPc, 0xFF));
  EXPECT_EQ(zs::DetectLayout(short_rom), zs::LayoutDialect::kUnmarked);
  EXPECT_FALSE(zs::CanUseTailExpansion(short_rom));
  EXPECT_FALSE(zs::LoadSheetCollisions(short_rom, 0).ok());
}

TEST(ZscreamCustomLayoutTest, RejectsInvalidSheetIdsAndComposedInput) {
  const Rom rom = LoadSynthetic(SyntheticLayout());
  for (int id : {-1, 128, std::numeric_limits<int>::max()}) {
    EXPECT_EQ(zs::LoadSheetCollisions(rom, id).status().code(),
              absl::StatusCode::kInvalidArgument);
  }
  const auto composed =
      zs::ComposeTile8Collisions(rom, {0, 1, 2, 3, 4, 5, 6, 128});
  EXPECT_EQ(composed.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(ZscreamCustomLayoutTest, TailGuardDoesNotTreatUnknownNewerMarkersAsV3) {
  Rom rom = LoadSynthetic(SyntheticLayout());
  for (int marker = 0; marker <= 0xFF; ++marker) {
    rom.mutable_data()[zs::kVersionMarkerPc] = static_cast<uint8_t>(marker);
    EXPECT_EQ(zs::CanUseTailExpansion(rom), marker <= 3 || marker == 0xFF)
        << "marker " << marker;
  }
}

TEST(ZscreamCustomLayoutTest, MapBackgroundSlotsFollowFullLoadWorldDefaults) {
  auto bytes = SyntheticLayout();
  bytes[OverworldCustomTileGFXGroupEnabled] = 0xFF;
  for (int map = 0; map < kNumOverworldMaps; ++map) {
    bytes[kOverworldMapParentIdExpanded + map] = static_cast<uint8_t>(map);
    bytes[kOverworldScreenSize + map] = 0;
    for (int slot = 0; slot < 8; ++slot) {
      bytes[OverworldCustomTileGFXGroupArray + map * 8 + slot] = 0xFF;
    }
  }
  for (int world = 0; world < 3; ++world) {
    for (int slot = 0; slot < 8; ++slot) {
      bytes[OverworldCustomDefaultGFXGroups + world * 8 + slot] =
          static_cast<uint8_t>(world * 16 + slot);
    }
  }
  // Explicit zero must stay sheet zero; only FF inherits the default row.
  bytes[OverworldCustomTileGFXGroupArray + 0x45 * 8 + 2] = 0;
  bytes[OverworldCustomTileGFXGroupArray + 0x05 * 8 + 1] = 91;
  Rom rom = LoadSynthetic(bytes);
  const auto before = rom.vector();
  Overworld overworld(&rom);
  auto& maps =
      const_cast<std::vector<OverworldMap>&>(overworld.overworld_maps());
  for (int map_id = 0; map_id < kNumOverworldMaps; ++map_id) {
    maps.emplace_back(map_id, &rom);
  }
  const int areas[] = {0x05, 0x45, 0x95};
  for (int world = 0; world < 3; ++world) {
    auto& map = maps[areas[world]];
    map.LoadAreaGraphics();
    zs::ResolvedSheetIds ids{};
    for (int slot = 0; slot < 8; ++slot) {
      const uint8_t expected = world == 1 && slot == 2   ? 0
                               : world == 0 && slot == 1 ? 91
                                                         : world * 16 + slot;
      ids[slot] = map.MainSlotProvenance(slot).effective_sheet;
      EXPECT_EQ(ids[slot], expected) << "world " << world << ", slot " << slot;
      EXPECT_EQ(ids[slot], map.static_graphics(slot));
    }
    EXPECT_EQ(map.static_graphics(8), 0x73);  // sprite slot, not collision BG
    const auto collisions = overworld.GetTile8CollisionTypes(areas[world]);
    ASSERT_TRUE(collisions.ok()) << collisions.status();
    EXPECT_EQ(*collisions, *zs::ComposeTile8Collisions(rom, ids));
    for (size_t tile = 0; tile < collisions->size(); ++tile) {
      EXPECT_EQ((*collisions)[tile], CollisionValue(ids[tile / 64], tile % 64));
    }
  }
  EXPECT_EQ(rom.vector(), before);
}

TEST(ZscreamCustomLayoutTest,
     ExtendedGraphicsSheetDoesNotReadBeyondCollisionTable) {
  auto bytes = SyntheticLayout();
  bytes[OverworldCustomTileGFXGroupEnabled] = 0xFF;
  bytes[OverworldCustomTileGFXGroupArray + 0x83 * 8 + 1] = 0xA3;
  bytes[kOverworldMapParentIdExpanded + 0x83] = 0x83;
  Rom rom = LoadSynthetic(bytes);
  const auto before = rom.vector();
  Overworld overworld(&rom);
  auto& maps =
      const_cast<std::vector<OverworldMap>&>(overworld.overworld_maps());
  for (int map_id = 0; map_id < kNumOverworldMaps; ++map_id)
    maps.emplace_back(map_id, &rom);
  maps[0x83].LoadAreaGraphics();
  EXPECT_EQ(overworld.GetTile8CollisionTypes(0x83).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(rom.vector(), before);
}

TEST(ZscreamCustomLayoutTest, LegacyMapReaderKeepsTheLegacyCollisionTable) {
  auto bytes = SyntheticLayout(0xFF);
  const size_t offset =
      kVersionConstantsMap.at(zelda3_version::US).kOverworldTilesType;
  for (size_t i = 0; i < 512; ++i) {
    bytes[offset + i] = static_cast<uint8_t>(i * 3);
  }
  Rom rom = LoadSynthetic(bytes);
  Overworld overworld(&rom);
  overworld.LoadTileTypes();
  const auto collisions = overworld.GetTile8CollisionTypes(0);
  ASSERT_TRUE(collisions.ok()) << collisions.status();
  EXPECT_EQ(*collisions, overworld.all_tiles_types());
  for (size_t i = 0; i < collisions->size(); ++i) {
    EXPECT_EQ((*collisions)[i], static_cast<uint8_t>(i * 3));
  }
  EXPECT_FALSE(overworld.GetTile8CollisionTypes(-1).ok());
  EXPECT_FALSE(overworld.GetTile8CollisionTypes(kNumOverworldMaps).ok());
}

TEST(ZscreamCustomLayoutTest, PerSheetReaderNeverFallsBackToLegacyBytes) {
  auto bytes = SyntheticLayout();
  std::fill_n(bytes.begin() + overworldTilesType, 512, 0xAA);
  Rom rom = LoadSynthetic(bytes);
  Overworld overworld(&rom);
  overworld.LoadTileTypes();
  EXPECT_EQ(overworld.all_tiles_types(), (std::array<uint8_t, 512>{}));
  // No loaded map exists: return an error rather than legacy AA or sheet zero.
  EXPECT_EQ(overworld.GetTile8CollisionTypes(0).status().code(),
            absl::StatusCode::kFailedPrecondition);
  rom.mutable_data()[zs::kVersionMarkerPc] = 0x04;
  EXPECT_EQ(overworld.GetTile8CollisionTypes(0).status().code(),
            absl::StatusCode::kFailedPrecondition);
}

TEST(ZscreamCustomLayoutTest, LegacyCollisionSaveRejectsCustomBeforeAnyWrite) {
  for (uint8_t marker : {0x04, 0x05, 0x06, 0xFE}) {
    Rom rom = LoadSynthetic(SyntheticLayout(marker));
    Overworld overworld(&rom);
    (*overworld.mutable_all_tiles_types())[0] = 0xAA;
    const auto before = rom.vector();
    const bool was_dirty = rom.dirty();
    EXPECT_EQ(overworld.SaveOverworldTilesType().code(),
              absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(rom.vector(), before);
    EXPECT_EQ(rom.dirty(), was_dirty);
  }
}

TEST(ZscreamCustomLayoutTest, CollisionModelRejectsMissingOrShortRom) {
  Overworld absent(nullptr);
  EXPECT_FALSE(absent.GetTile8CollisionTypes(0).ok());
  EXPECT_FALSE(absent.SaveOverworldTilesType().ok());
  absent.LoadTileTypes();
  Rom short_rom = LoadSynthetic(std::vector<uint8_t>(32, 0));
  Overworld short_world(&short_rom);
  EXPECT_EQ(short_world.GetTile8CollisionTypes(0).status().code(),
            absl::StatusCode::kOutOfRange);
  EXPECT_EQ(short_world.SaveOverworldTilesType().code(),
            absl::StatusCode::kOutOfRange);
}

}  // namespace
}  // namespace yaze::zelda3
