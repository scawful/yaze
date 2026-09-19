#ifndef YAZE_ZELDA3_DUNGEON_ROOM_LAYER_REGISTERS_H
#define YAZE_ZELDA3_DUNGEON_ROOM_LAYER_REGISTERS_H

#include <cstdint>
#include <vector>

#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {

// The PPU layer settings the game uses for a room right after it loads, from
// its WRAM shadows: TM ($1C, main screen layers), TS ($1D, sub screen layers)
// and CGADSUB ($9A, color math). Bit 0 of TM/TS is hardware BG1, which shows
// the lower tilemap TILEMAPB ($7E4000, yaze's BG2 buffers); bit 1 is hardware
// BG2, the upper tilemap TILEMAPA ($7E2000, yaze's BG1 buffers)
// (Intro_InitializeBackgroundSettings: BG1SC=$13, BG2SC=$03).
struct RoomLayerRegisters {
  uint8_t tm = 0x16;
  uint8_t ts = 0x00;
  uint8_t cgadsub = 0x20;

  // yaze's BG2 buffers (TILEMAPB) appear on screen at all.
  bool LowerTilemapShown() const { return ((tm | ts) & 0x01) != 0; }
  // Both tilemaps are on the main screen and interleave by tile priority,
  // with the lower tilemap (hardware BG1) winning ties.
  bool TilemapsShareMainScreen() const { return (tm & 0x03) == 0x03; }
  // The lower tilemap is blended onto the upper one through color math.
  bool LowerTilemapBlended() const {
    return (ts & 0x01) != 0 && (cgadsub & 0x02) != 0;
  }
  bool BlendSubtracts() const { return (cgadsub & 0x80) != 0; }
  bool BlendHalves() const { return (cgadsub & 0x40) != 0; }
};

// Derives the settings from the room header, as Underworld_LoadHeader,
// Module06_UnderworldLoad and Underworld_ResetTorchBackgroundAndPlayer do:
//  - bgact: header byte 0 bits 7-5 ($0414);
//  - dark: header byte 0 bit 0 (subtractive color math, $9A = $B3);
//  - effect: header byte 4 ($AD); tag2: header byte 6 ($AF);
//  - room_flags: the room's persistent word ($7EF000 + 2*room), 0 for a room
//    as first entered;
//  - objects whose draw routines clear $0414 in that state:
//    0xDA RoomDraw_WaterOverlayB when the water flag is clear, 0xD8
//    RoomDraw_WaterOverlayA when it is set, and the dam-gate merged stairs
//    0x133 (North_MergedLayer_B) and 0xFB3 (South_MergedLayer) when tag2 is
//    0x1B and flag 0x0100 is clear.
// This is the state on entering through an entrance, with torches unlit and
// no lamp; Underworld_HandleLayerEffect can change it every frame afterwards.
RoomLayerRegisters DeriveRoomLayerRegisters(
    uint8_t bgact, bool dark, int effect, int tag2,
    const std::vector<RoomObject>& objects, uint16_t room_flags = 0);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_LAYER_REGISTERS_H
