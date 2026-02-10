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

}  // namespace yaze::zelda3::test
