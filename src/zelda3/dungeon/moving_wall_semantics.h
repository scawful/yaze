#ifndef YAZE_ZELDA3_DUNGEON_MOVING_WALL_SEMANTICS_H_
#define YAZE_ZELDA3_DUNGEON_MOVING_WALL_SEMANTICS_H_

#include <array>

namespace yaze {
namespace zelda3 {
namespace moving_wall {

// USDASM uses the high two size bits for the vertical platform repetition and
// the low two bits for the horizontal fill-column count.
inline constexpr std::array<int, 4> kDirections = {5, 7, 11, 15};
inline constexpr std::array<int, 4> kObjectCounts = {8, 16, 24, 32};

constexpr int DirectionForSize(int size) {
  return kDirections[(size >> 2) & 0x03];
}

constexpr int ObjectCountForSize(int size) {
  return kObjectCounts[size & 0x03];
}

}  // namespace moving_wall
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_MOVING_WALL_SEMANTICS_H_
