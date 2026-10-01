// ZSCustomOverworld v3 tile GFX groups: how an area's $FF ("use default")
// slot resolves. Reference is the game's full sheet load,
// InitTilesetsLongCalls in Oracle of Secrets Overworld/ZSCustomOverworld.asm:
//
//   sheet = OWGFXGroupTable[$8A * 8 + slot]            ; 0x140480
//   if sheet == $FF:
//     sheet = DefaultGFXGroups[(($8A & $C0) >> 3) + slot]  ; 0x140980
//
// $8A is the area parent, so the default row is LW 0x140980, DW 0x140988,
// SW 0x140990. Only $FF means "default"; $00 is loaded as sheet 0x00.
// (Scroll transitions skip $FF slots and keep the previous sheet, which the
// editor cannot model; this pins the full-load result.)
//
// OverworldMap::LoadAreaGraphics follows that full-load contract: $FF reads
// the area world's DefaultGFXGroups row, and $00 is sheet 0.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics_sheet_store.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {
namespace {

// Written over every legacy (vanilla) GFX group byte, so a slot that falls
// back to the legacy tables shows up as this value.
constexpr uint8_t kLegacySheet = 0x7F;

uint8_t DefaultSheet(int world, int slot) {
  return static_cast<uint8_t>(0x10 * (world + 1) + slot);
}

std::vector<uint8_t> MakeV3Rom() {
  std::vector<uint8_t> rom(0x200000, 0x00);
  rom[OverworldCustomASMHasBeenApplied] = 0x03;
  rom[OverworldCustomTileGFXGroupEnabled] = 0xFF;
  for (int i = 0; i < kNumOverworldMaps; ++i) {
    rom[kOverworldMapParentIdExpanded + i] = static_cast<uint8_t>(i);
    rom[kOverworldScreenSize + i] = 0x00;  // small
  }
  const auto& us = kVersionConstantsMap.at(zelda3_version::US);
  std::fill(rom.begin() + us.kOverworldGfxGroups1,
            rom.begin() + us.kOverworldGfxGroups2 + 0x25 * 8, kLegacySheet);
  // The checked GameData loader follows this pointer, while the overworld
  // loader reads the corresponding US main-group table directly.
  rom[kGfxGroupsPointer] = 0x73;
  rom[kGfxGroupsPointer + 1] = 0xE0;
  for (int world = 0; world < 3; ++world) {
    for (int slot = 0; slot < 8; ++slot) {
      rom[OverworldCustomDefaultGFXGroups + world * 8 + slot] =
          DefaultSheet(world, slot);
    }
  }
  return rom;
}

void SetGroup(std::vector<uint8_t>& rom, int area,
              const std::array<uint8_t, 8>& sheets) {
  for (int slot = 0; slot < 8; ++slot) {
    rom[OverworldCustomTileGFXGroupArray + area * 8 + slot] = sheets[slot];
  }
}

std::array<uint8_t, 8> ResolvedSheets(Rom* rom, int area) {
  OverworldMap map(area, rom);
  map.LoadAreaGraphics();
  std::array<uint8_t, 8> sheets{};
  for (int slot = 0; slot < 8; ++slot) {
    sheets[slot] = map.static_graphics(slot);
  }
  return sheets;
}

std::unique_ptr<Rom> LoadRom(const std::vector<uint8_t>& bytes) {
  auto rom = std::make_unique<Rom>();
  EXPECT_TRUE(rom->LoadFromData(bytes).ok());
  return rom;
}

// Oracle of Secrets sky area 0x83 as shipped: slots 3 and 6 are $FF. The
// game loads SW defaults $3D/$3E there; the editor showed $0A/$1D.
TEST(OverworldDefaultGfxGroupTest, FfSlotsLoadTheWorldDefaultSheet) {
  auto bytes = MakeV3Rom();
  SetGroup(bytes, 0x83, {0x3A, 0x3B, 0x3C, 0xFF, 0x38, 0x45, 0xFF, 0x5B});
  auto rom = LoadRom(bytes);
  const std::array<uint8_t, 8> expected = {
      0x3A, 0x3B, 0x3C, DefaultSheet(2, 3), 0x38, 0x45, DefaultSheet(2, 6),
      0x5B};
  EXPECT_EQ(ResolvedSheets(rom.get(), 0x83), expected);
}

TEST(OverworldDefaultGfxGroupTest, DefaultRowFollowsTheAreaWorld) {
  auto bytes = MakeV3Rom();
  const std::array<uint8_t, 8> all_default = {0xFF, 0xFF, 0xFF, 0xFF,
                                              0xFF, 0xFF, 0xFF, 0xFF};
  const int areas[3] = {0x05, 0x45, 0x95};
  for (int area : areas) {
    SetGroup(bytes, area, all_default);
  }
  auto rom = LoadRom(bytes);
  for (int world = 0; world < 3; ++world) {
    SCOPED_TRACE(areas[world]);
    const auto sheets = ResolvedSheets(rom.get(), areas[world]);
    for (int slot = 0; slot < 8; ++slot) {
      EXPECT_EQ(sheets[slot], DefaultSheet(world, slot)) << "slot " << slot;
    }
  }
}

// Secondary: InitTilesetsLongCalls compares only with $FF, so $00 is a
// literal sheet. (Oracle's one $00 entry, area 0x88 slot 0, happens to match
// the legacy group, so no shipped area differs today.)
TEST(OverworldDefaultGfxGroupTest, ZeroIsALiteralSheet) {
  auto bytes = MakeV3Rom();
  SetGroup(bytes, 0x10, {0x00, 0x3B, 0x3C, 0x3D, 0x53, 0x4D, 0x3E, 0x5B});
  auto rom = LoadRom(bytes);
  EXPECT_EQ(ResolvedSheets(rom.get(), 0x10)[0], 0x00);
}

// Guard for the fix: explicit sheets win over every fallback.
TEST(OverworldDefaultGfxGroupTest, ExplicitSheetsAreLoadedAsIs) {
  auto bytes = MakeV3Rom();
  const std::array<uint8_t, 8> explicit_sheets = {0x3A, 0x3B, 0x3C, 0x0A,
                                                  0x51, 0x4E, 0x3E, 0x59};
  SetGroup(bytes, 0x86, explicit_sheets);
  auto rom = LoadRom(bytes);
  EXPECT_EQ(ResolvedSheets(rom.get(), 0x86), explicit_sheets);
}

TEST(OverworldDefaultGfxGroupTest,
     SharedBackgroundEditsCannotOverrideExplicitOrWorldDefaultSlots) {
  auto bytes = MakeV3Rom();
  SetGroup(bytes, 0x05, {0x00, 0x31, 0xFF, 0x42, 0xFF, 0x45, 0x46, 0x47});
  auto rom = LoadRom(bytes);
  GameData data;
  data.version = zelda3_version::US;
  ASSERT_TRUE(LoadGfxGroups(*rom, data).ok());
  ASSERT_TRUE(data.gfx_groups_loaded);
  data.graphics_buffer.assign(
      GraphicsSheetStore::kSheetCount * GraphicsSheetStore::kSheetBytes, 1);
  data.sheet_store.MarkAllSheetsChanged();
  OverworldMap map(0x05, rom.get(), &data);
  map.set_area_graphics(7);
  map.set_sprite_graphics(0, 5);
  map.LoadAreaGraphics();
  const std::array<uint8_t, 8> expected = {
      0x00, 0x31, DefaultSheet(0, 2), 0x42, DefaultSheet(0, 4), 0x45,
      0x46, 0x47};
  for (int slot = 0; slot < 8; ++slot) {
    EXPECT_EQ(map.static_graphics(slot), expected[slot]) << "slot " << slot;
  }
  ASSERT_TRUE(map.BuildTileset().ok());
  EXPECT_FALSE(map.SourceSheetsChanged());

  data.main_blockset_ids[0x20][0] = 0x60;
  data.main_blockset_ids[0x20][2] = 0x61;
  data.room_blockset_ids[7][0] = 0x62;
  data.room_blockset_ids[7][1] = 0x63;
  EXPECT_FALSE(map.SourceSheetsChanged());
  map.LoadAreaGraphics();
  for (int slot = 0; slot < 8; ++slot) {
    EXPECT_EQ(map.static_graphics(slot), expected[slot]) << "slot " << slot;
  }

  data.spriteset_ids[5][0] = 0x10;
  EXPECT_TRUE(map.SourceSheetsChanged());
  map.LoadAreaGraphics();
  EXPECT_EQ(map.static_graphics(12), 0x83);
}

TEST(OverworldDefaultGfxGroupTest,
     AnimatedTopHalfUsesItsOwnFallbackWhileStaticSlotSevenStaysCustom) {
  auto bytes = MakeV3Rom();
  bytes[OverworldCustomAnimatedGFXEnabled] = 0xFF;
  bytes[OverworldCustomAnimatedGFXArray + 0x45] = 0xFF;
  SetGroup(bytes, 0x45, {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x58});
  auto rom = LoadRom(bytes);
  GameData data;
  data.version = zelda3_version::US;
  ASSERT_TRUE(LoadGfxGroups(*rom, data).ok());
  OverworldMap map(0x45, rom.get(), &data);
  map.LoadAreaGraphics();
  EXPECT_EQ(map.static_graphics(7), 0x58);
  EXPECT_EQ(map.animated_sheet(), DefaultSheet(1, 7));

  data.main_blockset_ids[0x21][7] = 0x64;
  map.LoadAreaGraphics();
  EXPECT_EQ(map.static_graphics(7), 0x58);
  EXPECT_EQ(map.animated_sheet(), DefaultSheet(1, 7));

  map.set_animated_gfx(0x5A);
  map.LoadAreaGraphics();
  EXPECT_EQ(map.static_graphics(7), 0x58);
  EXPECT_EQ(map.animated_sheet(), 0x5A);
}

}  // namespace
}  // namespace yaze::zelda3
