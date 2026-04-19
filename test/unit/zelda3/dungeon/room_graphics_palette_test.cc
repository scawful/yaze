#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/dungeon/room.h"

namespace yaze::zelda3::test {

// USDASM grounding:
// - bank_00.asm LoadBackgroundGraphics chooses Expand3bppToVRAM_RightPalette for
//   underworld (UW) background slots $0F >= 4 (i.e. the second half of the 8
//   background sheets in the set). Right-palette expansion sets bit3 for any
//   non-zero pixel, shifting values 1-7 to 9-15.
//
// In yaze we store sheets as 8BPP linear indices (one byte per pixel). To
// mirror the runtime expansion, we apply the +8 shift when copying UW
// background blocks 4-7 into the room's graphics buffer.
TEST(RoomGraphicsPaletteTest, CopyRoomGraphicsToBuffer_ShiftsRightPaletteBlocks) {
  auto rom = std::make_unique<Rom>();
  std::vector<uint8_t> dummy_rom(0x200000, 0);
  rom->LoadFromData(dummy_rom);

  GameData game_data;
  // Minimal graphics buffer: 16 sheets × 4096 bytes per 8BPP sheet.
  game_data.graphics_buffer.assign(16 * 4096, 0);

  auto write_sheet_pattern = [&](int sheet_id) {
    const int base = sheet_id * 4096;
    // A few sentinel pixels:
    // - 0 should remain 0 under right-palette expansion (transparent).
    // - 1 and 7 should become 9 and 15 respectively when expanded as "right".
    game_data.graphics_buffer[base + 0] = 0;
    game_data.graphics_buffer[base + 1] = 1;
    game_data.graphics_buffer[base + 2] = 7;
  };

  for (int sheet = 0; sheet < 16; ++sheet) {
    write_sheet_pattern(sheet);
  }

  Room room(/*room_id=*/0, rom.get(), &game_data);
  // Use sheet IDs matching block indices for deterministic offsets.
  for (int block = 0; block < 16; ++block) {
    room.mutable_blocks()[block] = static_cast<uint8_t>(block);
  }

  room.CopyRoomGraphicsToBuffer();

  const auto& gfx = room.get_gfx_buffer();

  // Block 3 (UW background slot < 4): left palette (no shift).
  EXPECT_EQ(gfx[3 * 4096 + 0], 0);
  EXPECT_EQ(gfx[3 * 4096 + 1], 1);
  EXPECT_EQ(gfx[3 * 4096 + 2], 7);

  // Block 4 (UW background slot >= 4): right palette shift (+8 for non-zero).
  EXPECT_EQ(gfx[4 * 4096 + 0], 0);
  EXPECT_EQ(gfx[4 * 4096 + 1], 9);
  EXPECT_EQ(gfx[4 * 4096 + 2], 15);

  // Block 5 also shifted.
  EXPECT_EQ(gfx[5 * 4096 + 0], 0);
  EXPECT_EQ(gfx[5 * 4096 + 1], 9);
  EXPECT_EQ(gfx[5 * 4096 + 2], 15);

  // Sprite blocks should NOT be shifted by this UW background rule.
  EXPECT_EQ(gfx[8 * 4096 + 0], 0);
  EXPECT_EQ(gfx[8 * 4096 + 1], 1);
  EXPECT_EQ(gfx[8 * 4096 + 2], 7);
}

TEST(RoomGraphicsPaletteTest, BuildDungeonRenderPaletteIncludesHudRows) {
  gfx::SnesPalette hud_palette;
  for (int i = 0; i < 32; ++i) {
    hud_palette.AddColor(gfx::SnesColor(i, i + 1, i + 2));
  }

  gfx::SnesPalette dungeon_palette;
  for (int i = 0; i < 90; ++i) {
    dungeon_palette.AddColor(gfx::SnesColor(i + 32, i + 33, i + 34));
  }

  const auto colors =
      BuildDungeonRenderPalette(dungeon_palette, &hud_palette);

  ASSERT_EQ(colors.size(), 256u);

  // HUD rows 0-1 occupy indices 0..31 directly.
  EXPECT_EQ(colors[0].r, 0);
  EXPECT_EQ(colors[0].g, 1);
  EXPECT_EQ(colors[0].b, 2);
  EXPECT_EQ(colors[16].r, 16);
  EXPECT_EQ(colors[16].g, 17);
  EXPECT_EQ(colors[16].b, 18);
  EXPECT_EQ(colors[31].r, 31);
  EXPECT_EQ(colors[31].g, 32);
  EXPECT_EQ(colors[31].b, 33);

  // Dungeon banks start at CGRAM row 2 col 1, so row-start slots remain empty.
  EXPECT_EQ(colors[32].a, 0);
  EXPECT_EQ(colors[33].r, 32);
  EXPECT_EQ(colors[33].g, 33);
  EXPECT_EQ(colors[33].b, 34);
  EXPECT_EQ(colors[47].r, 46);
  EXPECT_EQ(colors[47].g, 47);
  EXPECT_EQ(colors[47].b, 48);
  EXPECT_EQ(colors[48].a, 0);
  EXPECT_EQ(colors[49].r, 47);
  EXPECT_EQ(colors[49].g, 48);
  EXPECT_EQ(colors[49].b, 49);

  // Undrawn fill color remains transparent.
  EXPECT_EQ(colors[255].a, 0);
}

}  // namespace yaze::zelda3::test
