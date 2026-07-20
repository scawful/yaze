#ifndef YAZE_ZELDA3_DUNGEON_DUNGEON_BLOCK_CODEC_H_
#define YAZE_ZELDA3_DUNGEON_DUNGEON_BLOCK_CODEC_H_

#include <cstdint>

namespace yaze {
namespace zelda3 {

// Pure encode/decode helpers for the vanilla pushable-block save table at
// `SpecialUnderworldObjects_pushable_block` ($04:F1DE..$04:F3DD, 512 bytes,
// 128 max entries × 4 bytes). The runtime scan in bank_01.asm:1162 walks the
// flat table entry-by-entry and matches on `room_id`; there is no per-room
// terminator. The renderer entry `RoomDraw_PushableBlock` at $01:B4D6 reads
// the encoded word with `AND.w #$3FFF` before using it as a tilemap index.
// That 14-bit index contains a 13-bit 64x64 tile position plus bit 13, which
// selects the lower/BG2 tilemap. `PushBlock_CheckForPit` at $01:D8D4 tests bit
// 14 independently when selecting the behavior/collision layer.
//
// Word layout (b4 << 8 | b3):
//   bit 0      : unused (always 0; LSL-by-1 padding)
//   bits 1..6  : px (6 bits, range 0..63)
//   bits 7..12 : py (6 bits, range 0..63)
//   bit 13     : draw target (0 = upper/BG1, 1 = lower/BG2)
//   bit 14     : behavior/pit selector (0 = upper, 1 = lower)
//   bit 15     : unused (always 0 in vanilla)
//
// This module replaces ad-hoc bit twiddling that previously lived in
// `Room::LoadBlocks`. Bit 13 was previously misidentified as the high bit of
// py, causing three vanilla lower-layer blocks (rooms 0xA8, 0x66, and 0x2C)
// to load 64 tiles too low and render on BG1.

struct PushableBlockEntry {
  uint16_t room_id = 0;
  uint8_t px = 0;              // 0..63
  uint8_t py = 0;              // 0..63
  uint8_t draw_layer = 0;      // bit 13: 0=upper/BG1, 1=lower/BG2
  uint8_t behavior_layer = 0;  // bit 14: pit/collision layer selector
};

struct PushableBlockBytes {
  uint8_t b1 = 0;  // room_id low byte
  uint8_t b2 = 0;  // room_id high byte
  uint8_t b3 = 0;  // position+layer word, low byte
  uint8_t b4 = 0;  // position+layer word, high byte
};

inline PushableBlockEntry DecodePushableBlockEntry(
    const PushableBlockBytes& bytes) {
  PushableBlockEntry out;
  out.room_id = static_cast<uint16_t>(bytes.b1 | (bytes.b2 << 8));
  const uint16_t word = static_cast<uint16_t>(bytes.b3 | (bytes.b4 << 8));
  out.px = static_cast<uint8_t>((word >> 1) & 0x3F);
  out.py = static_cast<uint8_t>((word >> 7) & 0x3F);
  out.draw_layer = static_cast<uint8_t>((word >> 13) & 0x01);
  out.behavior_layer = static_cast<uint8_t>((word >> 14) & 0x01);
  return out;
}

inline PushableBlockBytes EncodePushableBlockEntry(
    const PushableBlockEntry& entry) {
  PushableBlockBytes out;
  out.b1 = static_cast<uint8_t>(entry.room_id & 0xFF);
  out.b2 = static_cast<uint8_t>((entry.room_id >> 8) & 0xFF);
  const uint16_t word = static_cast<uint16_t>(
      ((entry.px & 0x3F) << 1) | ((entry.py & 0x3F) << 7) |
      ((entry.draw_layer & 0x01) << 13) |
      ((entry.behavior_layer & 0x01) << 14));
  out.b3 = static_cast<uint8_t>(word & 0xFF);
  out.b4 = static_cast<uint8_t>((word >> 8) & 0xFF);
  return out;
}

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DUNGEON_BLOCK_CODEC_H_
