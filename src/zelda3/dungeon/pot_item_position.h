#ifndef YAZE_ZELDA3_DUNGEON_POT_ITEM_POSITION_H_
#define YAZE_ZELDA3_DUNGEON_POT_ITEM_POSITION_H_

#include <cstdint>
#include <optional>

namespace yaze::zelda3 {
// Bank $01 stores the tilemap byte index in $0540 (e.g. $01B34C), with
// bit 13 selecting the lower layer ($01B358). RevealPotItem ($01E6DD)
// masks bit 15 before comparing that index. A 64-tile row is 128 bytes.
// Preserve all non-coordinate bits, including unknown bit 0 and bit 14.
inline constexpr uint16_t kPotItemCoordinateMask = 0x1FFE;
constexpr int PotItemPixelX(uint16_t word) {
  return ((word >> 1) & 63) * 8;
}
constexpr int PotItemPixelY(uint16_t word) {
  return ((word >> 7) & 63) * 8;
}
constexpr std::optional<uint16_t> EncodePotItemPosition(int x, int y,
                                                        uint16_t original = 0) {
  if (original == 0xFFFF || x < 0 || x > 504 || y < 0 || y > 504 ||
      x % 8 != 0 || y % 8 != 0)
    return std::nullopt;
  const uint16_t word = static_cast<uint16_t>(
      (original & ~kPotItemCoordinateMask) | ((y / 8) << 7) | ((x / 8) << 1));
  // Never turn an authored entry into the end-of-list marker.
  if (word == 0xFFFF)
    return std::nullopt;
  return word;
}
}  // namespace yaze::zelda3
#endif
