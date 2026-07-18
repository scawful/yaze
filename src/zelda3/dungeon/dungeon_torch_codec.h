#ifndef YAZE_ZELDA3_DUNGEON_DUNGEON_TORCH_CODEC_H_
#define YAZE_ZELDA3_DUNGEON_DUNGEON_TORCH_CODEC_H_

#include <cstdint>

namespace yaze::zelda3 {

// Pure encode/decode helpers for entries in the lightable-torch table at
// `kTorchData`. `RoomDraw_LightableTorch` ($01:B509) tests bit 15 to select
// the lit tile data, then masks the word with `AND.w #$3FFF` before drawing.
// The retained bit 13 adds $2000 to the upper-layer $7E2000 tilemap base and
// therefore selects the lower/BG2 tilemap at $7E4000. Bit 14 is reserved by
// the torch table and must be preserved independently of the draw target.
//
// Word layout (high << 8 | low):
//   bit 0      : unused (always 0; LSL-by-1 padding)
//   bits 1..6  : px (6 bits, range 0..63)
//   bits 7..12 : py (6 bits, range 0..63)
//   bit 13     : draw target (0 = upper/BG1, 1 = lower/BG2)
//   bit 14     : reserved (preserved on load/save)
//   bit 15     : initially lit
//
// The codec represents the complete 6-bit coordinate fields. SaveAllTorches
// separately restricts editable 2x2 torch anchors to 0..62 so their art stays
// within the 64x64 room tilemap.

struct LightableTorchEntry {
  uint8_t px = 0;
  uint8_t py = 0;
  uint8_t draw_layer = 0;
  uint8_t reserved = 0;
  bool lit = false;
};

struct LightableTorchBytes {
  uint8_t low = 0;
  uint8_t high = 0;
};

inline LightableTorchEntry DecodeLightableTorchEntry(
    const LightableTorchBytes& bytes) {
  const uint16_t word = static_cast<uint16_t>(bytes.low | (bytes.high << 8));
  return LightableTorchEntry{
      .px = static_cast<uint8_t>((word >> 1) & 0x3F),
      .py = static_cast<uint8_t>((word >> 7) & 0x3F),
      .draw_layer = static_cast<uint8_t>((word >> 13) & 0x01),
      .reserved = static_cast<uint8_t>((word >> 14) & 0x01),
      .lit = (word & 0x8000) != 0,
  };
}

inline LightableTorchBytes EncodeLightableTorchEntry(
    const LightableTorchEntry& entry) {
  const uint16_t word = static_cast<uint16_t>(
      ((entry.px & 0x3F) << 1) | ((entry.py & 0x3F) << 7) |
      ((entry.draw_layer & 0x01) << 13) | ((entry.reserved & 0x01) << 14) |
      (entry.lit ? 0x8000 : 0));
  return LightableTorchBytes{
      .low = static_cast<uint8_t>(word & 0xFF),
      .high = static_cast<uint8_t>((word >> 8) & 0xFF),
  };
}

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_DUNGEON_TORCH_CODEC_H_
