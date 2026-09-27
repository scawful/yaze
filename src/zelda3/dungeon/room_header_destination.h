#ifndef YAZE_ZELDA3_DUNGEON_ROOM_HEADER_DESTINATION_H_
#define YAZE_ZELDA3_DUNGEON_ROOM_HEADER_DESTINATION_H_

#include <cstdint>

namespace yaze::zelda3 {

// Room headers store stair and holewarp destinations as one byte. The game
// writes that byte to $A0 in 8-bit mode and keeps $A1, the high byte of the
// current room:
//   stairs    $01:C3D0 LDA.l $7EC001,X / $01:C3D4 STA.b $A0
//   pits      $07:94BA LDA.l $7EC000   / $07:94BE STA.b $A0
//   warp tile $07:D146 LDA.l $7EC000   / $07:D14A STA.b $A0
// So a stair byte $1D in room $119 leads to $11D, not $01D.
constexpr int ResolveHeaderDestinationRoom(int source_room_id,
                                           uint8_t header_byte) {
  return (source_room_id & 0xFF00) | header_byte;
}

// Inverse of ResolveHeaderDestinationRoom: the header byte that reaches
// `destination_room_id` from `source_room_id`, or -1 when the destination is
// on another $A1 page (0x000-0x0FF vs 0x100-0x1FF) and cannot be encoded.
constexpr int EncodeHeaderDestinationRoom(int source_room_id,
                                          int destination_room_id) {
  if (destination_room_id < 0 ||
      (destination_room_id & 0xFF00) != (source_room_id & 0xFF00)) {
    return -1;
  }
  return destination_room_id & 0xFF;
}

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_HEADER_DESTINATION_H_
