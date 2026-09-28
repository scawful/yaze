#include "zelda3/sprite/sprite_sheet_slots.h"

#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "rom/rom.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::test {
namespace {

using zelda3::SpriteTileIssue;

TEST(SpriteSheetSlotsTest, StaticSheetsThenSpritesetValues) {
  // Oracle spriteset 0x0C: Kydrog cutscene, Farore (0x55), birds.
  const std::array<uint8_t, 4> values = {0x51, 0x43, 0x55, 0x42};
  using Slots = std::array<uint8_t, 8>;
  EXPECT_EQ(zelda3::SpriteSheetSlots(values, /*underworld=*/false),
            (Slots{0x73, 0x74, 0x79, 0x7A, 0xC4, 0xB6, 0xC8, 0xB5}));
  EXPECT_EQ(zelda3::SpriteSheetSlots(values, /*underworld=*/true),
            (Slots{0x73, 0x7D, 0x79, 0x7A, 0xC4, 0xB6, 0xC8, 0xB5}));
  EXPECT_FALSE(zelda3::IsUnderworldSpriteset(0x3F));
  EXPECT_TRUE(zelda3::IsUnderworldSpriteset(0x40));
}

TEST(SpriteSheetSlotsTest, NameTableBitSelectsTheSpritesetSlots) {
  EXPECT_EQ(zelda3::SpriteSlotForTile(0x000), 0);
  EXPECT_EQ(zelda3::SpriteSlotForTile(0x0FF), 3);
  EXPECT_EQ(zelda3::SpriteSlotForTile(0x100), 4);
  EXPECT_EQ(zelda3::SpriteSlotForTile(0x186), 6);  // Farore chr $86, page 1
  EXPECT_EQ(zelda3::SpriteSlotForTile(0x1FF), 7);
  EXPECT_EQ(zelda3::SpriteSlotForTile(0x3C0), 7);  // bits above 0x1FF ignored
}

TEST(SpriteSheetSlotsTest, FlagsBlankReservedAndUnreadableTiles) {
  auto fixture = BuildGfxSheetTestRom();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  // Sheet 0x90: art except 16x16 block 5 (tiles 10, 11, 26, 27).
  std::vector<uint8_t> sheet(zelda3::kGfxSheet3bppBytes, 0x11);
  for (int tile : zelda3::GfxSheetBlockTiles(5)) {
    std::fill_n(sheet.begin() + tile * 24, 24, 0x00);
  }
  zelda3::GfxSheetWritePolicy policy;
  policy.allocation_regions.push_back(
      {GfxSheetTestRom::kFreeStart, GfxSheetTestRom::kFreeEnd});
  ASSERT_TRUE(zelda3::WriteGfxSheet(rom, 0x90, sheet, policy).ok());

  const std::array<uint8_t, 8> slots = {0x73, 0x74, 0x79, 0x7A,
                                        0x90, 0x91, 0xE1, 0x7B};
  const std::vector<int> tiles = {0x100, 0x10A, 0x11B,  // slot 4 (0x90)
                                  0x142,                // slot 5 (0x91)
                                  0x180,                // slot 6 (sheet 225)
                                  0x1C0};               // slot 7 (reserved)
  const auto issues = zelda3::CheckSpriteTiles(rom, tiles, slots, {0x7B});
  ASSERT_EQ(issues.size(), 4u);
  EXPECT_EQ(issues[0].tile, 0x10A);
  EXPECT_EQ(issues[0].kind, SpriteTileIssue::Kind::kBlank);
  EXPECT_EQ(issues[0].sheet, 0x90);
  EXPECT_EQ(issues[1].tile, 0x11B);
  EXPECT_EQ(issues[1].kind, SpriteTileIssue::Kind::kBlank);
  EXPECT_EQ(issues[2].tile, 0x180);
  EXPECT_EQ(issues[2].kind, SpriteTileIssue::Kind::kUnreadableSheet);
  EXPECT_EQ(issues[2].slot, 6);
  EXPECT_EQ(issues[3].tile, 0x1C0);
  EXPECT_EQ(issues[3].kind, SpriteTileIssue::Kind::kReservedSheet);
}

}  // namespace
}  // namespace yaze::test
