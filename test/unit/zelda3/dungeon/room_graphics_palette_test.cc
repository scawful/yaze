#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "rom/rom.h"
#include "zelda3/dungeon/palette_debug.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3::test {

// USDASM grounding:
// - bank_00.asm LoadBackgroundGraphics chooses Expand3bppToVRAM_RightPalette for
//   underworld (UW) runtime slots $0F >= 4. InitializeTilesets loads destination
//   blocks 0..7 while counting $0F down from 7..0. Right-palette expansion sets
//   bit3 for any non-zero pixel, shifting values 1-7 to 9-15.
//
// In yaze we store sheets as 8BPP linear indices (one byte per pixel). To
// mirror the runtime expansion, we apply the +8 shift when copying UW
// destination blocks 0-3 into the room's graphics buffer.
TEST(RoomGraphicsPaletteTest,
     CopyRoomGraphicsToBuffer_ShiftsRightPaletteBlocks) {
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

  // Destination block 3 is runtime slot 4: right palette shift.
  EXPECT_EQ(gfx[3 * 4096 + 0], 0);
  EXPECT_EQ(gfx[3 * 4096 + 1], 9);
  EXPECT_EQ(gfx[3 * 4096 + 2], 15);

  // Destination block 4 is runtime slot 3: left palette (no shift).
  EXPECT_EQ(gfx[4 * 4096 + 0], 0);
  EXPECT_EQ(gfx[4 * 4096 + 1], 1);
  EXPECT_EQ(gfx[4 * 4096 + 2], 7);

  // Destination block 5 is runtime slot 2 and is also unshifted for UW.
  EXPECT_EQ(gfx[5 * 4096 + 0], 0);
  EXPECT_EQ(gfx[5 * 4096 + 1], 1);
  EXPECT_EQ(gfx[5 * 4096 + 2], 7);

  // Sprite blocks should NOT be shifted by this UW background rule.
  EXPECT_EQ(gfx[8 * 4096 + 0], 0);
  EXPECT_EQ(gfx[8 * 4096 + 1], 1);
  EXPECT_EQ(gfx[8 * 4096 + 2], 7);
}

TEST(RoomGraphicsPaletteTest,
     CopyRoomGraphicsToBuffer_UsesEntranceMainGroupForRightPaletteSlots) {
  auto rom = std::make_unique<Rom>();
  std::vector<uint8_t> dummy_rom(0x200000, 0);
  rom->LoadFromData(dummy_rom);

  GameData game_data;
  game_data.graphics_buffer.assign(16 * 4096, 0);
  for (int sheet = 0; sheet < 16; ++sheet) {
    const int base = sheet * 4096;
    game_data.graphics_buffer[base + 0] = 0;
    game_data.graphics_buffer[base + 1] = 1;
    game_data.graphics_buffer[base + 2] = 7;
  }

  Room room(/*room_id=*/0, rom.get(), &game_data);
  for (int block = 0; block < 16; ++block) {
    room.mutable_blocks()[block] = static_cast<uint8_t>(block);
  }
  room.SetRenderEntranceBlockset(0x20);

  room.CopyRoomGraphicsToBuffer();

  const auto& gfx = room.get_gfx_buffer();

  // OW runtime slots 2, 3, 4, and 7 map to destination blocks 5, 4, 3, and 0.
  EXPECT_EQ(gfx[0 * 4096 + 1], 9);
  EXPECT_EQ(gfx[3 * 4096 + 1], 9);
  EXPECT_EQ(gfx[4 * 4096 + 1], 9);
  EXPECT_EQ(gfx[5 * 4096 + 1], 9);

  EXPECT_EQ(gfx[1 * 4096 + 1], 1);
  EXPECT_EQ(gfx[2 * 4096 + 1], 1);
  EXPECT_EQ(gfx[6 * 4096 + 1], 1);
  EXPECT_EQ(gfx[7 * 4096 + 1], 1);
  EXPECT_EQ(gfx[8 * 4096 + 1], 1);
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

  const auto colors = BuildDungeonRenderPalette(dungeon_palette, &hud_palette);

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

TEST(RoomGraphicsPaletteTest,
     PaletteDebuggerSamplesMappedDungeonRenderPalette) {
  gfx::SnesPalette hud_palette;
  for (int i = 0; i < 32; ++i) {
    hud_palette.AddColor(gfx::SnesColor(i, i + 1, i + 2));
  }

  gfx::SnesPalette dungeon_palette;
  for (int i = 0; i < 90; ++i) {
    dungeon_palette.AddColor(gfx::SnesColor(i + 32, i + 33, i + 34));
  }

  const auto render_palette =
      BuildDungeonRenderPalette(dungeon_palette, &hud_palette);

  gfx::Bitmap bitmap;
  bitmap.Create(/*width=*/1, /*height=*/1, /*depth=*/8,
                std::vector<uint8_t>{33});
  bitmap.SetPalette(render_palette);

  auto& debugger = PaletteDebugger::Get();
  debugger.Clear();
  debugger.SetCurrentPalette(dungeon_palette);
  debugger.SetCurrentRenderPalette(render_palette);
  debugger.SetCurrentBitmap(&bitmap);

  const auto comp = debugger.SamplePixelAt(0, 0);
  EXPECT_EQ(comp.palette_index, 33);
  EXPECT_EQ(comp.expected_r, 32);
  EXPECT_EQ(comp.expected_g, 33);
  EXPECT_EQ(comp.expected_b, 34);
  EXPECT_EQ(comp.actual_r, 32);
  EXPECT_EQ(comp.actual_g, 33);
  EXPECT_EQ(comp.actual_b, 34);
  EXPECT_TRUE(comp.matches);

  debugger.Clear();
}

TEST(RoomGraphicsPaletteTest,
     LoadRoomGraphicsWithoutEntranceOverrideUsesRoomHeaderBlockset) {
  GameData game_data;
  game_data.main_blockset_ids[3] = {10, 11, 12, 13, 14, 15, 16, 17};
  game_data.room_blockset_ids[3] = {80, 0, 82, 83};
  game_data.spriteset_ids[64] = {1, 2, 3, 4};

  Room room;
  room.SetGameData(&game_data);
  room.SetBlockset(3);
  room.SetSpriteset(0);
  room.SetRenderEntranceBlockset(0xFF);

  room.LoadRoomGraphics();

  const auto blocks = room.blocks();
  EXPECT_EQ(blocks[0], 10);
  EXPECT_EQ(blocks[1], 11);
  EXPECT_EQ(blocks[2], 12);
  EXPECT_EQ(blocks[3], 80);
  EXPECT_EQ(blocks[4], 14);
  EXPECT_EQ(blocks[5], 82);
  EXPECT_EQ(blocks[6], 83);
  EXPECT_EQ(blocks[7], 17);
}

TEST(RoomGraphicsPaletteTest,
     LoadRoomGraphicsUsesEntranceMainAndRoomHeaderOverrides) {
  GameData game_data;
  game_data.main_blockset_ids[3] = {10, 11, 12, 13, 14, 15, 16, 17};
  game_data.main_blockset_ids[5] = {50, 51, 52, 53, 54, 55, 56, 57};
  game_data.room_blockset_ids[3] = {0, 91, 92, 0};
  game_data.spriteset_ids[64] = {1, 2, 3, 4};

  Room room;
  room.SetGameData(&game_data);
  room.SetBlockset(3);
  room.SetSpriteset(0);
  room.SetRenderEntranceBlockset(5);

  room.LoadRoomGraphics();

  const auto blocks = room.blocks();
  EXPECT_EQ(blocks[0], 50);
  EXPECT_EQ(blocks[1], 51);
  EXPECT_EQ(blocks[2], 52);
  EXPECT_EQ(blocks[3], 53);
  EXPECT_EQ(blocks[4], 91);
  EXPECT_EQ(blocks[5], 92);
  EXPECT_EQ(blocks[6], 56);
  EXPECT_EQ(blocks[7], 57);
}

TEST(RoomGraphicsPaletteTest,
     LoadRoomGraphicsExplicitOverrideBeatsStoredRenderContext) {
  GameData game_data;
  game_data.main_blockset_ids[5] = {50, 51, 52, 53, 54, 55, 56, 57};
  game_data.main_blockset_ids[7] = {70, 71, 72, 73, 74, 75, 76, 77};
  game_data.room_blockset_ids[3] = {80, 81, 82, 83};
  game_data.spriteset_ids[64] = {1, 2, 3, 4};

  Room room;
  room.SetGameData(&game_data);
  room.SetBlockset(3);
  room.SetSpriteset(0);
  room.SetRenderEntranceBlockset(5);

  room.LoadRoomGraphics(static_cast<uint8_t>(7));

  const auto blocks = room.blocks();
  EXPECT_EQ(blocks[0], 70);
  EXPECT_EQ(blocks[1], 71);
  EXPECT_EQ(blocks[2], 72);
  EXPECT_EQ(blocks[3], 80);
  EXPECT_EQ(blocks[4], 81);
  EXPECT_EQ(blocks[5], 82);
  EXPECT_EQ(blocks[6], 83);
  EXPECT_EQ(blocks[7], 77);
}

}  // namespace yaze::zelda3::test
