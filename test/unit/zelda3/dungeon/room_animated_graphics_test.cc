#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3::test {

TEST(RoomAnimatedGraphicsTest, ResolvesRelocatedTableAndUsesMainBlockset) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  GameData data;
  data.version = zelda3_version::US;
  data.graphics_buffer.assign(kNumGfxSheets * 4096, 0);
  std::fill_n(data.graphics_buffer.begin() + 93 * 4096, 1024, 6);
  std::fill_n(data.graphics_buffer.begin() + 94 * 4096, 1024, 3);
  std::fill_n(data.graphics_buffer.begin() + 92 * 4096, 1024, 1);
  Room room(0x25, &rom, &data);
  room.SetBlockset(1);
  room.SetBackgroundTileset(7);
  // A relocated table makes treating the operand's own PC offset as a SNES
  // address fail. Entry 7 also differs, so the header alias cannot mask an
  // incorrect active-main-blockset selection.
  constexpr int kTablePc = 0x12345;
  const auto address = PcToSnes(kTablePc);
  ASSERT_TRUE(rom.WriteVector(room.version_constants().kGfxAnimatedPointer,
                              {static_cast<uint8_t>(address),
                               static_cast<uint8_t>(address >> 8),
                               static_cast<uint8_t>(address >> 16)})
                  .ok());
  ASSERT_TRUE(rom.WriteByte(kTablePc + 1, 93).ok());
  ASSERT_TRUE(rom.WriteByte(kTablePc + 7, 94).ok());
  const auto original_rom = rom.vector();
  room.LoadAnimatedGraphics();
  const auto& graphics = room.get_gfx_buffer();
  EXPECT_TRUE(std::all_of(graphics.begin() + 7 * 4096 - 1024,
                          graphics.begin() + 7 * 4096,
                          [](uint8_t pixel) { return pixel == 6; }));
  EXPECT_TRUE(std::all_of(graphics.begin() + 7 * 4096,
                          graphics.begin() + 7 * 4096 + 1024,
                          [](uint8_t pixel) { return pixel == 1; }));
  EXPECT_EQ(rom.vector(), original_rom);
}

TEST(RoomAnimatedGraphicsTest, RejectsTableOutsideRomWithoutWritingGraphics) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  GameData data;
  data.version = zelda3_version::US;
  data.graphics_buffer.assign(kNumGfxSheets * 4096, 0);
  Room room(0x25, &rom, &data);
  ASSERT_TRUE(rom.WriteVector(room.version_constants().kGfxAnimatedPointer,
                              {0xFF, 0xFF, 0xFF})
                  .ok());
  room.mutable_blocks().fill(0);
  room.CopyRoomGraphicsToBuffer();
  const auto before = room.get_gfx_buffer();
  room.LoadAnimatedGraphics();
  EXPECT_EQ(room.get_gfx_buffer(), before);
}

}  // namespace yaze::zelda3::test
