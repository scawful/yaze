#ifndef YAZE_APP_GFX_TYPES_SNES_COLOR_MATH_H_
#define YAZE_APP_GFX_TYPES_SNES_COLOR_MATH_H_

#include <algorithm>
#include <cstdint>

namespace yaze::gfx {

// CGADSUB operates on five-bit channels. Half-add divides the unsaturated sum;
// clamping before the division incorrectly darkens bright water/lighting.
constexpr uint8_t SnesColorMathChannel(uint8_t main, uint8_t sub, bool subtract,
                                       bool half) {
  int value =
      subtract ? std::max(0, int(main) - int(sub)) : int(main) + int(sub);
  if (half)
    value >>= 1;
  return static_cast<uint8_t>(std::min(31, value));
}

constexpr uint8_t ExpandSnesChannel(uint8_t value) {
  return static_cast<uint8_t>((value << 3) | (value >> 2));
}

}  // namespace yaze::gfx
#endif  // YAZE_APP_GFX_TYPES_SNES_COLOR_MATH_H_
