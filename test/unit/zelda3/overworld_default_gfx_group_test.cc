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
// OverworldMap::LoadAreaGraphics currently keeps the legacy vanilla groups
// (kOverworldGfxGroups2 main blockset + kOverworldGfxGroups1 area group) for
// $00/$FF slots instead, so the tests below that expect the default row are
// DISABLED_ until that is fixed. Run them with
//   --gtest_also_run_disabled_tests --gtest_filter='OverworldDefaultGfxGroup*'

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "rom/rom.h"
#include "zelda3/game_data.h"
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
TEST(OverworldDefaultGfxGroupTest, DISABLED_FfSlotsLoadTheWorldDefaultSheet) {
  auto bytes = MakeV3Rom();
  SetGroup(bytes, 0x83, {0x3A, 0x3B, 0x3C, 0xFF, 0x38, 0x45, 0xFF, 0x5B});
  auto rom = LoadRom(bytes);
  const std::array<uint8_t, 8> expected = {
      0x3A, 0x3B, 0x3C, DefaultSheet(2, 3), 0x38, 0x45, DefaultSheet(2, 6),
      0x5B};
  EXPECT_EQ(ResolvedSheets(rom.get(), 0x83), expected);
}

TEST(OverworldDefaultGfxGroupTest, DISABLED_DefaultRowFollowsTheAreaWorld) {
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
TEST(OverworldDefaultGfxGroupTest, DISABLED_ZeroIsALiteralSheet) {
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

}  // namespace
}  // namespace yaze::zelda3
