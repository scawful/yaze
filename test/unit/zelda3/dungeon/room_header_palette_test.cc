#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze::zelda3::test {
namespace {

TEST(RoomHeaderPaletteTest, PaletteIdIsNotTruncatedToSixBits) {
  Rom rom;
  std::vector<uint8_t> dummy_rom(0x200000, 0);

  // Build a minimal room header pointer table:
  // - kRoomHeaderPointer points to a SNES address holding a 16-bit pointer table
  // - kRoomHeaderPointerBank holds the bank byte for header entries
  // - table entry [room_id * 2] contains the low/high bytes of the header addr
  constexpr int room_id = 0;
  constexpr uint32_t kHeaderTableSnes = 0x018000;
  constexpr uint8_t kHeaderBank = 0x01;
  constexpr uint16_t kHeaderWordAddr = 0x9000;
  constexpr uint32_t kHeaderSnes = (static_cast<uint32_t>(kHeaderBank) << 16) |
                                   static_cast<uint32_t>(kHeaderWordAddr);

  const uint32_t header_table_pc = SnesToPc(kHeaderTableSnes);
  const uint32_t header_pc = SnesToPc(kHeaderSnes);

  // LONG pointer at kRoomHeaderPointer (little-endian).
  dummy_rom[kRoomHeaderPointer + 0] =
      static_cast<uint8_t>(kHeaderTableSnes & 0xFF);
  dummy_rom[kRoomHeaderPointer + 1] =
      static_cast<uint8_t>((kHeaderTableSnes >> 8) & 0xFF);
  dummy_rom[kRoomHeaderPointer + 2] =
      static_cast<uint8_t>((kHeaderTableSnes >> 16) & 0xFF);

  // Bank byte for header entries.
  dummy_rom[kRoomHeaderPointerBank] = kHeaderBank;

  // Room 0 header pointer entry.
  dummy_rom[header_table_pc + (room_id * 2) + 0] =
      static_cast<uint8_t>(kHeaderWordAddr & 0xFF);
  dummy_rom[header_table_pc + (room_id * 2) + 1] =
      static_cast<uint8_t>((kHeaderWordAddr >> 8) & 0xFF);

  // Header bytes: byte1 is the palette set ID.
  dummy_rom[header_pc + 0] = 0x00;   // byte0 (bg2/collision/light)
  dummy_rom[header_pc + 1] = 0x40;   // palette set ID (must be preserved)
  dummy_rom[header_pc + 2] = 0x00;   // blockset
  dummy_rom[header_pc + 3] = 0x00;   // spriteset

  ASSERT_TRUE(rom.LoadFromData(dummy_rom).ok());

  Room room = LoadRoomHeaderFromRom(&rom, room_id);

  EXPECT_EQ(room.palette, 0x40);

  room.SetPalette(0x47);
  ASSERT_TRUE(room.SaveRoomHeader().ok());

  EXPECT_EQ(rom.vector()[header_pc + 1], 0x47);
}

}  // namespace
}  // namespace yaze::zelda3::test

