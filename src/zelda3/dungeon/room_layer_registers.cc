#include "zelda3/dungeon/room_layer_registers.h"

#include <array>

namespace yaze::zelda3 {
namespace {

// Underworld_SubscreenEnable (usdasm bank_02): TS per BGACT; $FF moves the
// lower tilemap to the main screen instead (TM=$17, TS=0).
constexpr std::array<uint8_t, 8> kSubscreenEnable = {0x00, 0x01, 0x01, 0xFF,
                                                     0x01, 0x01, 0x01, 0x01};

}  // namespace

RoomLayerRegisters DeriveRoomLayerRegisters(
    uint8_t bgact, bool dark, int effect, int tag2,
    const std::vector<RoomObject>& objects, uint16_t room_flags) {
  // $0403 bit 3 (the room word's low-byte bit 7): water drained/filled.
  const bool water_flag = (room_flags & 0x0080) != 0;
  const bool dam_gate_open = (room_flags & 0x0100) != 0;
  for (const RoomObject& object : objects) {
    const bool clears =
        (object.id_ == 0xDA && !water_flag) ||  // $01:9684
        (object.id_ == 0xD8 && water_flag) ||   // $01:9568
        ((object.id_ == 0x133 || object.id_ == 0xFB3) && tag2 == 0x1B &&
         !dam_gate_open);  // AutoStairsNorthMergedStart, BecomeMultiC
    if (clears) {
      bgact = 0;
    }
  }
  bgact &= 0x07;

  RoomLayerRegisters registers;
  const uint8_t subscreen = kSubscreenEnable[bgact];
  if (subscreen == 0xFF) {
    registers.tm = 0x17;
    registers.ts = 0x00;
  } else {
    registers.tm = 0x16;
    registers.ts = bgact == 2 ? 0x03 : subscreen;
  }

  // Module06_UnderworldLoad ($02:82D2-82F8).
  if (bgact == 7) {
    registers.cgadsub = 0x32;  // full add
  } else if (bgact == 4) {
    registers.cgadsub = 0x62;  // half add
  } else {
    registers.cgadsub = 0x20;
  }
  if (dark) {
    registers.cgadsub = 0xB3;  // subtract: the room is unlit
  }

  // Underworld_HandleLayerEffect state on the first frames after load.
  switch (effect) {
    case 5:  // Agahnim 2
    case 6:  // invisible floor
      registers.ts = 0x02;
      break;
    case 7:  // Ganon, torches unlit
      registers.ts = 0x00;
      registers.cgadsub = 0x70;
      break;
    default:
      break;
  }
  return registers;
}

}  // namespace yaze::zelda3
