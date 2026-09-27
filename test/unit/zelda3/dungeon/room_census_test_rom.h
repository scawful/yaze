#ifndef YAZE_TEST_UNIT_ZELDA3_DUNGEON_ROOM_CENSUS_TEST_ROM_H_
#define YAZE_TEST_UNIT_ZELDA3_DUNGEON_ROOM_CENSUS_TEST_ROM_H_

#include <algorithm>
#include <cstdint>
#include <vector>

#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"
#include "zelda3/dungeon/room_census.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3::test {

// A synthetic 2 MB ROM for the room census. Every room has an empty object
// stream and sprite list; every overworld entrance slot places entrance 0,
// whose room word is 0 (room 0x000).
//   0x000: warp tile, holewarp byte 0x04 -> reaches 0x004.
//   spawn 0 -> 0x104: warp tile, holewarp byte 0x05 -> reaches 0x105 (the
//   game keeps the high byte), not 0x005.
// Everything else is empty and unreached: 292 free rooms.
inline std::vector<uint8_t> BuildRoomCensusTestRom() {
  constexpr int kObjectPointerTablePc = 0x070000;
  constexpr int kObjectDataPc = 0x060000;
  constexpr int kWarpStreamPc = kObjectDataPc + 0x100;
  constexpr int kSpritePointerTablePc = 0x048000;
  constexpr uint32_t kHeaderTableSnes = 0x018000;
  constexpr uint16_t kDefaultHeader = 0x9000;
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
  write_long(kRoomObjectPointer, PcToSnes(kObjectPointerTablePc));
  write(kObjectDataPc,
        {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF});
  const RoomObject warp(0xFCA, 10, 10, 0, 0);
  const auto encoded = warp.EncodeObjectToBytes();
  write(kWarpStreamPc, {0x00, 0x00, encoded.b1, encoded.b2, encoded.b3, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF});
  // Sprites: one empty list for every room.
  write_word(kRoomsSpritePointer,
             static_cast<uint16_t>(PcToSnes(kSpritePointerTablePc)));
  write(kSpritesData, {0x00, 0xFF});
  // Room headers.
  write_long(kRoomHeaderPointer, kHeaderTableSnes);
  data[kRoomHeaderPointerBank] = 0x01;
  const uint32_t header_table_pc = SnesToPc(kHeaderTableSnes);
  for (int room = 0; room < kRoomCensusRoomCount; ++room) {
    write_long(kObjectPointerTablePc + room * 3, PcToSnes(kObjectDataPc));
    write_word(kSpritePointerTablePc + room * 2,
               static_cast<uint16_t>(PcToSnes(kSpritesData)));
    write_word(header_table_pc + room * 2, kDefaultHeader);
  }
  auto set_warp_room = [&](int room, uint16_t header, uint8_t holewarp) {
    write_long(kObjectPointerTablePc + room * 3, PcToSnes(kWarpStreamPc));
    write_word(header_table_pc + room * 2, header);
    data[SnesToPc(0x010000 | header) + 9] = holewarp;
  };
  set_warp_room(0x000, 0x9010, 0x04);
  set_warp_room(0x104, 0x9020, 0x05);
  write_word(kDungeonSpawnRoom, 0x0104);  // spawn point 0
  return data;
}

}  // namespace yaze::zelda3::test

#endif  // YAZE_TEST_UNIT_ZELDA3_DUNGEON_ROOM_CENSUS_TEST_ROM_H_
