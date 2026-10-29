#ifndef YAZE_ZELDA3_DUNGEON_MINECART_OBJECT_SEMANTICS_H_
#define YAZE_ZELDA3_DUNGEON_MINECART_OBJECT_SEMANTICS_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace yaze::zelda3 {

// The expanded catalog changes four existing 0x31 slots. Callers must select it
// from the current project's explicit runtime/asset binding, never ROM size or
// the global custom-object feature flag.
enum class MinecartRailProfile { kLegacy, kExpanded };

inline constexpr size_t kMinecartRailProfileMarkerPc = 0x160017;

inline MinecartRailProfile DetectMinecartRailProfile(
    std::span<const uint8_t> rom) {
  static constexpr std::array<uint8_t, 8> kMarker = {'M', 'C', 'R', '2',
                                                     16,  13,  2,   0};
  if (rom.size() < kMinecartRailProfileMarkerPc + kMarker.size()) {
    return MinecartRailProfile::kLegacy;
  }
  for (size_t index = 0; index < kMarker.size(); ++index) {
    if (rom[kMinecartRailProfileMarkerPc + index] != kMarker[index]) {
      return MinecartRailProfile::kLegacy;
    }
  }
  return MinecartRailProfile::kExpanded;
}

enum class MinecartRailStyle { kFloor, kPit };
enum class MinecartRailShape {
  kHorizontal,
  kVertical,
  kCornerTL,
  kCornerTR,
  kCornerBL,
  kCornerBR,
  kCrossing,
  // Junction names identify the closed side, matching collision BB..BE.
  kTJunctionNorth,
  kTJunctionSouth,
  kTJunctionEast,
  kTJunctionWest,
};

struct MinecartRailDescriptor {
  int object_id;
  int subtype;
  MinecartRailStyle style;
  MinecartRailShape shape;
  const char* label;
  const char* filename;
  // Row-major cells on the 8px collision grid, anchored to the 2x2 object.
  std::array<uint8_t, 4> collision_stencil;
  bool collision_verified;
  // A T junction follows the existing node contract: only its top-left 8px
  // anchor is owned. Its other artwork cells retain their existing behavior.
  uint8_t collision_mask = 0x0F;
};

inline std::span<const MinecartRailDescriptor> MinecartRailDescriptors(
    MinecartRailProfile profile = MinecartRailProfile::kLegacy) {
  using Style = MinecartRailStyle;
  using Shape = MinecartRailShape;
  static constexpr std::array<MinecartRailDescriptor, 14> kLegacy = {{
      {0x31,
       0,
       Style::kPit,
       Shape::kHorizontal,
       "Pit track horizontal",
       "track_LR.bin",
       {0xB0, 0xB0, 0xB0, 0xB0},
       true},
      {0x31,
       1,
       Style::kPit,
       Shape::kVertical,
       "Pit track vertical",
       "track_UD.bin",
       {0xB1, 0xB1, 0xB1, 0xB1},
       true},
      {0x31,
       2,
       Style::kPit,
       Shape::kCornerTL,
       "Pit track corner top-left",
       "track_corner_TL.bin",
       {0xB2, 0xB2, 0xB2, 0xB2},
       true},
      {0x31,
       3,
       Style::kPit,
       Shape::kCornerTR,
       "Pit track corner top-right",
       "track_corner_TR.bin",
       {0xB4, 0xB4, 0xB4, 0xB4},
       true},
      {0x31,
       4,
       Style::kPit,
       Shape::kCornerBL,
       "Pit track corner bottom-left",
       "track_corner_BL.bin",
       {0xB3, 0xB3, 0xB3, 0xB3},
       true},
      {0x31,
       5,
       Style::kPit,
       Shape::kCornerBR,
       "Pit track corner bottom-right",
       "track_corner_BR.bin",
       {0xB5, 0xB5, 0xB5, 0xB5},
       true},
      {0x31,
       6,
       Style::kFloor,
       Shape::kVertical,
       "Floor track vertical",
       "track_floor_UD.bin",
       {0xB1, 0xB1, 0xB1, 0xB1},
       true},
      {0x31,
       7,
       Style::kFloor,
       Shape::kHorizontal,
       "Floor track horizontal",
       "track_floor_LR.bin",
       {0xB0, 0xB0, 0xB0, 0xB0},
       true},
      {0x31,
       8,
       Style::kFloor,
       Shape::kCornerTL,
       "Floor track corner top-left",
       "track_floor_corner_TL.bin",
       {0xB2, 0xB2, 0xB2, 0xB2},
       true},
      {0x31,
       9,
       Style::kFloor,
       Shape::kCornerTR,
       "Floor track corner top-right",
       "track_floor_corner_TR.bin",
       {0xB4, 0xB4, 0xB4, 0xB4},
       true},
      {0x31,
       10,
       Style::kFloor,
       Shape::kCornerBL,
       "Floor track corner bottom-left",
       "track_floor_corner_BL.bin",
       {0xB3, 0xB3, 0xB3, 0xB3},
       true},
      {0x31,
       11,
       Style::kFloor,
       Shape::kCornerBR,
       "Floor track corner bottom-right",
       "track_floor_corner_BR.bin",
       {0xB5, 0xB5, 0xB5, 0xB5},
       true},
      {0x31,
       12,
       Style::kFloor,
       Shape::kCrossing,
       "Floor track any direction",
       "track_floor_any.bin",
       {0xB6, 0xB6, 0xB6, 0xB6},
       true},
      {0x31,
       14,
       Style::kPit,
       Shape::kCrossing,
       "Pit track any direction",
       "track_any.bin",
       {0xB6, 0xB6, 0xB6, 0xB6},
       true},
  }};
  static constexpr std::array<MinecartRailDescriptor, 22> kExpanded = {{
      kLegacy[0],
      kLegacy[1],
      kLegacy[2],
      kLegacy[3],
      kLegacy[4],
      kLegacy[5],
      kLegacy[6],
      kLegacy[7],
      kLegacy[8],
      kLegacy[9],
      kLegacy[10],
      kLegacy[11],
      {0x31,
       12,
       Style::kPit,
       Shape::kTJunctionNorth,
       "Pit track T: south / east / west",
       "track_tjunction_N.bin",
       {0xBB, 0, 0, 0},
       true,
       0x01},
      {0x31,
       13,
       Style::kPit,
       Shape::kTJunctionSouth,
       "Pit track T: north / east / west",
       "track_tjunction_S.bin",
       {0xBC, 0, 0, 0},
       true,
       0x01},
      {0x31,
       14,
       Style::kPit,
       Shape::kTJunctionEast,
       "Pit track T: north / south / west",
       "track_tjunction_E.bin",
       {0xBD, 0, 0, 0},
       true,
       0x01},
      {0x31,
       15,
       Style::kPit,
       Shape::kTJunctionWest,
       "Pit track T: north / south / east",
       "track_tjunction_W.bin",
       {0xBE, 0, 0, 0},
       true,
       0x01},
      {0x32,
       5,
       Style::kFloor,
       Shape::kCrossing,
       "Floor track any direction",
       "track_floor_any.bin",
       {0xB6, 0xB6, 0xB6, 0xB6},
       true},
      {0x32,
       7,
       Style::kPit,
       Shape::kCrossing,
       "Pit track any direction",
       "track_any.bin",
       {0xB6, 0xB6, 0xB6, 0xB6},
       true},
      {0x32,
       9,
       Style::kFloor,
       Shape::kTJunctionNorth,
       "Floor track T: south / east / west",
       "track_floor_tjunction_N.bin",
       {0xBB, 0, 0, 0},
       true,
       0x01},
      {0x32,
       10,
       Style::kFloor,
       Shape::kTJunctionSouth,
       "Floor track T: north / east / west",
       "track_floor_tjunction_S.bin",
       {0xBC, 0, 0, 0},
       true,
       0x01},
      {0x32,
       11,
       Style::kFloor,
       Shape::kTJunctionEast,
       "Floor track T: north / south / west",
       "track_floor_tjunction_E.bin",
       {0xBD, 0, 0, 0},
       true,
       0x01},
      {0x32,
       12,
       Style::kFloor,
       Shape::kTJunctionWest,
       "Floor track T: north / south / east",
       "track_floor_tjunction_W.bin",
       {0xBE, 0, 0, 0},
       true,
       0x01},
  }};
  return profile == MinecartRailProfile::kExpanded
             ? std::span<const MinecartRailDescriptor>(kExpanded)
             : std::span<const MinecartRailDescriptor>(kLegacy);
}

inline const MinecartRailDescriptor* FindMinecartRailDescriptor(
    int object_id, int subtype,
    MinecartRailProfile profile = MinecartRailProfile::kLegacy) {
  for (const auto& rail : MinecartRailDescriptors(profile)) {
    if (rail.object_id == object_id && rail.subtype == subtype) {
      return &rail;
    }
  }
  return nullptr;
}

// Noncanonical explicitly configured CLI IDs retain their legacy behavior.
inline bool IsMinecartTrackGraphicsSubtype(
    int object_id, int subtype,
    MinecartRailProfile profile = MinecartRailProfile::kLegacy) {
  if (object_id != 0x31 &&
      !(profile == MinecartRailProfile::kExpanded && object_id == 0x32)) {
    return true;
  }
  return FindMinecartRailDescriptor(object_id, subtype, profile) != nullptr;
}

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_MINECART_OBJECT_SEMANTICS_H_
