#ifndef YAZE_ZELDA3_DUNGEON_DUNGEON_TORCH_CODEC_H_
#define YAZE_ZELDA3_DUNGEON_DUNGEON_TORCH_CODEC_H_

#include <cstdint>

namespace yaze::zelda3 {

// Pure encode/decode helpers for entries in the lightable-torch table at
// `kTorchData`. `RoomDraw_LightableTorch` ($01:B509) tests bit 15 to select
// the lit tile data, then masks the word with `AND.w #$3FFF` before drawing.
// Bits 0..13 therefore contain the complete tilemap position and bit 14 is
// the two-state special selector retained independently of the draw target.
//
// Word layout (high << 8 | low):
//   bit 0      : unused (always 0; LSL-by-1 padding)
//   bits 1..6  : px (6 bits, range 0..63)
//   bits 7..13 : py (7 bits, range 0..127)
//   bit 14     : special selector (0 or 1; does not select the draw buffer)
//   bit 15     : initially lit

struct LightableTorchEntry {
  uint8_t px = 0;
  uint8_t py = 0;
  uint8_t selector = 0;
  bool lit = false;
};

struct LightableTorchBytes {
  uint8_t low = 0;
  uint8_t high = 0;
};

inline LightableTorchEntry DecodeLightableTorchEntry(
    const LightableTorchBytes& bytes) {
  const uint16_t word = static_cast<uint16_t>(bytes.low | (bytes.high << 8));
  const uint16_t address = static_cast<uint16_t>((word & 0x3FFF) >> 1);
  return LightableTorchEntry{
      .px = static_cast<uint8_t>(address & 0x3F),
      .py = static_cast<uint8_t>((address >> 6) & 0x7F),
      .selector = static_cast<uint8_t>((word >> 14) & 0x01),
      .lit = (word & 0x8000) != 0,
  };
}

inline LightableTorchBytes EncodeLightableTorchEntry(
    const LightableTorchEntry& entry) {
  const uint16_t word = static_cast<uint16_t>(
      ((entry.px & 0x3F) << 1) | ((entry.py & 0x7F) << 7) |
      ((entry.selector & 0x01) << 14) | (entry.lit ? 0x8000 : 0));
  return LightableTorchBytes{
      .low = static_cast<uint8_t>(word & 0xFF),
      .high = static_cast<uint8_t>((word >> 8) & 0xFF),
  };
}

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_DUNGEON_TORCH_CODEC_H_
