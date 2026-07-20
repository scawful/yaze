#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze::zelda3::test {
namespace {

constexpr int kTestRoomId = 0;

struct HeaderRomData {
  std::vector<uint8_t> dummy_rom = std::vector<uint8_t>(0x200000, 0);
  uint32_t header_pc = 0;
};

HeaderRomData BuildHeaderRomData(uint8_t byte0, uint8_t palette_id) {
  HeaderRomData data;
  // Build a minimal room header pointer table:
  // - kRoomHeaderPointer points to a SNES address holding a 16-bit pointer table
  // - kRoomHeaderPointerBank holds the bank byte for header entries
  // - table entry [room_id * 2] contains the low/high bytes of the header addr
  constexpr uint32_t kHeaderTableSnes = 0x018000;
  constexpr uint8_t kHeaderBank = 0x01;
  constexpr uint16_t kHeaderWordAddr = 0x9000;
  constexpr uint32_t kHeaderSnes = (static_cast<uint32_t>(kHeaderBank) << 16) |
                                   static_cast<uint32_t>(kHeaderWordAddr);

  const uint32_t header_table_pc = SnesToPc(kHeaderTableSnes);
  data.header_pc = SnesToPc(kHeaderSnes);

  // LONG pointer at kRoomHeaderPointer (little-endian).
  data.dummy_rom[kRoomHeaderPointer + 0] =
      static_cast<uint8_t>(kHeaderTableSnes & 0xFF);
  data.dummy_rom[kRoomHeaderPointer + 1] =
      static_cast<uint8_t>((kHeaderTableSnes >> 8) & 0xFF);
  data.dummy_rom[kRoomHeaderPointer + 2] =
      static_cast<uint8_t>((kHeaderTableSnes >> 16) & 0xFF);

  // Bank byte for header entries.
  data.dummy_rom[kRoomHeaderPointerBank] = kHeaderBank;

  // Room 0 header pointer entry.
  data.dummy_rom[header_table_pc + (kTestRoomId * 2) + 0] =
      static_cast<uint8_t>(kHeaderWordAddr & 0xFF);
  data.dummy_rom[header_table_pc + (kTestRoomId * 2) + 1] =
      static_cast<uint8_t>((kHeaderWordAddr >> 8) & 0xFF);

  data.dummy_rom[data.header_pc + 0] = byte0;
  data.dummy_rom[data.header_pc + 1] = palette_id;
  data.dummy_rom[data.header_pc + 2] = 0x00;
  data.dummy_rom[data.header_pc + 3] = 0x00;
  return data;
}

TEST(RoomHeaderPaletteTest, PaletteIdIsNotTruncatedToSixBits) {
  Rom rom;
  auto data = BuildHeaderRomData(/*byte0=*/0x00, /*palette_id=*/0x40);

  ASSERT_TRUE(rom.LoadFromData(data.dummy_rom).ok());

  Room room = LoadRoomHeaderFromRom(&rom, kTestRoomId);

  EXPECT_EQ(room.palette(), 0x40);

  room.SetPalette(0x47);
  ASSERT_TRUE(room.SaveRoomHeader().ok());

  EXPECT_EQ(rom.vector()[data.header_pc + 1], 0x47);
}

TEST(RoomHeaderPaletteTest, HeaderByteUsesBg2HighBitsForLayerMerging) {
  constexpr uint8_t byte0 = static_cast<uint8_t>((5 << 5) | (2 << 2));
  Rom rom;
  auto data = BuildHeaderRomData(byte0, /*palette_id=*/0x00);
  ASSERT_TRUE(rom.LoadFromData(data.dummy_rom).ok());

  Room room = LoadRoomHeaderFromRom(&rom, kTestRoomId);

  EXPECT_EQ(room.bg2(), static_cast<background2>(5));
  EXPECT_EQ(room.layer_merging().ID, 5);
  EXPECT_EQ(room.collision(), static_cast<CollisionKey>(2));

  ASSERT_TRUE(room.SaveRoomHeader().ok());
  EXPECT_EQ(rom.vector()[data.header_pc + 0], byte0);
}

TEST(RoomHeaderPaletteTest, DarkRoomPreservesLayerHighBitsOnSave) {
  constexpr uint8_t byte0 = static_cast<uint8_t>((5 << 5) | (3 << 2) | 1);
  Rom rom;
  auto data = BuildHeaderRomData(byte0, /*palette_id=*/0x00);
  ASSERT_TRUE(rom.LoadFromData(data.dummy_rom).ok());

  Room room = LoadRoomHeaderFromRom(&rom, kTestRoomId);

  EXPECT_EQ(room.bg2(), background2::DarkRoom);
  EXPECT_EQ(room.layer_merging().ID, 8);
  EXPECT_EQ(room.collision(), static_cast<CollisionKey>(3));

  ASSERT_TRUE(room.SaveRoomHeader().ok());
  EXPECT_EQ(rom.vector()[data.header_pc + 0], byte0);
}

TEST(RoomHeaderPaletteTest,
     HeaderByteSevenRoundTripsPitAndStaircaseLayersExactly) {
  Rom rom;
  auto data = BuildHeaderRomData(/*byte0=*/0x00, /*palette_id=*/0x00);
  data.dummy_rom[data.header_pc + 7] = 0xE7;
  data.dummy_rom[data.header_pc + 8] = 0x02;
  ASSERT_TRUE(rom.LoadFromData(data.dummy_rom).ok());

  Room room = LoadRoomHeaderFromRom(&rom, kTestRoomId);
  const std::vector<uint8_t> before = rom.vector();

  ASSERT_TRUE(room.SaveRoomHeader().ok());
  EXPECT_EQ(rom.vector(), before);
}

}  // namespace
}  // namespace yaze::zelda3::test
