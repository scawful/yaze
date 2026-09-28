#ifndef YAZE_TEST_UNIT_ZELDA3_DUNGEON_ROOM_CENSUS_TEST_ROM_H_
#define YAZE_TEST_UNIT_ZELDA3_DUNGEON_ROOM_CENSUS_TEST_ROM_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"
#include "zelda3/dungeon/room_census.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3::test {

inline constexpr int kTestObjectPointerTablePc = 0x070000;
inline constexpr int kTestObjectDataPc = 0x060000;
inline constexpr int kTestSpritePointerTablePc = 0x048000;
inline constexpr uint32_t kTestHeaderTableSnes = 0x018000;
inline constexpr uint16_t kTestDefaultHeader = 0x9000;

// Room header byte offsets (14-byte header in bank $01).
inline constexpr int kTestHeaderTag1 = 5;
inline constexpr int kTestHeaderHolewarp = 9;
inline constexpr int kTestHeaderStair1 = 10;

// Encodes an object for a room object stream.
inline std::vector<uint8_t> TestObjectBytes(int id, int x, int y) {
  const RoomObject object(id, x, y, 0, 0);
  const auto encoded = object.EncodeObjectToBytes();
  return {encoded.b1, encoded.b2, encoded.b3};
}

// A door entry: b1 = position << 4 | direction, b2 = door type.
inline std::vector<uint8_t> TestDoorBytes(int position, int direction,
                                          uint8_t type) {
  return {static_cast<uint8_t>((position << 4) | (direction & 3)), type};
}

// Object stream: header, primary objects, two empty lists, doors.
inline std::vector<uint8_t> TestRoomStream(
    const std::vector<std::vector<uint8_t>>& objects,
    const std::vector<std::vector<uint8_t>>& doors = {}) {
  std::vector<uint8_t> stream = {0x00, 0x00};
  for (const auto& object : objects) {
    stream.insert(stream.end(), object.begin(), object.end());
  }
  stream.insert(stream.end(), {0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF});
  for (const auto& door : doors) {
    stream.insert(stream.end(), door.begin(), door.end());
  }
  stream.insert(stream.end(), {0xFF, 0xFF, 0xFF, 0xFF});
  return stream;
}

// Gives `room` its own object stream (at PC `stream_pc`) and its own header
// (at $01:`header_addr`) in a ROM from BuildRoomCensusTestRom().
inline void SetTestRoom(std::vector<uint8_t>& data, int room, int stream_pc,
                        const std::vector<uint8_t>& stream,
                        uint16_t header_addr,
                        const std::array<uint8_t, 14>& header) {
  std::copy(stream.begin(), stream.end(), data.begin() + stream_pc);
  const uint32_t stream_snes = PcToSnes(stream_pc);
  const int pointer_pc = kTestObjectPointerTablePc + room * 3;
  data[pointer_pc] = stream_snes & 0xFF;
  data[pointer_pc + 1] = (stream_snes >> 8) & 0xFF;
  data[pointer_pc + 2] = (stream_snes >> 16) & 0xFF;
  const uint32_t header_table_pc = SnesToPc(kTestHeaderTableSnes);
  data[header_table_pc + room * 2] = header_addr & 0xFF;
  data[header_table_pc + room * 2 + 1] = header_addr >> 8;
  std::copy(header.begin(), header.end(),
            data.begin() + SnesToPc(0x010000 | header_addr));
}

// A synthetic 2 MB ROM for the room census. Every room has an empty object
// stream and sprite list; every overworld entrance slot places entrance 0,
// whose room word is 0 (room 0x000).
//   0x000: warp tile, holewarp byte 0x04 -> reaches 0x004.
//   spawn 0 -> 0x104: warp tile, holewarp byte 0x05 -> reaches 0x105 (the
//   game keeps the high byte), not 0x005.
// Everything else is empty and unreached: 292 free rooms.
inline std::vector<uint8_t> BuildRoomCensusTestRom() {
  constexpr int kWarpStreamPc = kTestObjectDataPc + 0x100;
  std::vector<uint8_t> data(0x200000, 0x00);
  auto write = [&](int pc, const std::vector<uint8_t>& bytes) {
    std::copy(bytes.begin(), bytes.end(), data.begin() + pc);
  };
  auto write_word = [&](int pc, uint16_t value) {
    data[pc] = value & 0xFF;
    data[pc + 1] = value >> 8;
  };
  auto write_long = [&](int pc, uint32_t value) {
    data[pc] = value & 0xFF;
    data[pc + 1] = (value >> 8) & 0xFF;
    data[pc + 2] = (value >> 16) & 0xFF;
  };
  // Object streams: header, layer terminators, door marker.
  write_long(kRoomObjectPointer, PcToSnes(kTestObjectPointerTablePc));
  write(kTestObjectDataPc,
        {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF});
  write(kWarpStreamPc, TestRoomStream({TestObjectBytes(0xFCA, 10, 10)}));
  // Sprites: one empty list for every room.
  write_word(kRoomsSpritePointer,
             static_cast<uint16_t>(PcToSnes(kTestSpritePointerTablePc)));
  write(kSpritesData, {0x00, 0xFF});
  // Room layouts: an empty object list at each layout pointer (a zero ROM
  // would decode the rest of the ROM as layout objects).
  for (int layout : kRoomLayoutPointers) {
    write(SnesToPc(layout), {0xFF, 0xFF});
  }
  // Room headers.
  write_long(kRoomHeaderPointer, kTestHeaderTableSnes);
  data[kRoomHeaderPointerBank] = 0x01;
  const uint32_t header_table_pc = SnesToPc(kTestHeaderTableSnes);
  for (int room = 0; room < kRoomCensusRoomCount; ++room) {
    write_long(kTestObjectPointerTablePc + room * 3,
               PcToSnes(kTestObjectDataPc));
    write_word(kTestSpritePointerTablePc + room * 2,
               static_cast<uint16_t>(PcToSnes(kSpritesData)));
    write_word(header_table_pc + room * 2, kTestDefaultHeader);
  }
  auto set_warp_room = [&](int room, uint16_t header, uint8_t holewarp) {
    write_long(kTestObjectPointerTablePc + room * 3, PcToSnes(kWarpStreamPc));
    write_word(header_table_pc + room * 2, header);
    data[SnesToPc(0x010000 | header) + kTestHeaderHolewarp] = holewarp;
  };
  set_warp_room(0x000, 0x9010, 0x04);
  set_warp_room(0x104, 0x9020, 0x05);
  write_word(kDungeonSpawnRoom, 0x0104);  // spawn point 0
  return data;
}

}  // namespace yaze::zelda3::test

#endif  // YAZE_TEST_UNIT_ZELDA3_DUNGEON_ROOM_CENSUS_TEST_ROM_H_
