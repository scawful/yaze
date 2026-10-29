#ifndef YAZE_ZELDA3_DUNGEON_TRACK_COLLISION_GENERATOR_H
#define YAZE_ZELDA3_DUNGEON_TRACK_COLLISION_GENERATOR_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/minecart_object_semantics.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {

// Collision tile types used by the Oracle of Secrets minecart system.
// The minecart sprite reads these via Sprite_GetTileAttr and dispatches
// movement based on tile value.
enum class TrackTileType : uint8_t {
  HorizStraight = 0xB0,
  VertStraight = 0xB1,
  CornerTL = 0xB2,  // top-left corner (exits: right, down)
  CornerBL = 0xB3,  // bottom-left corner (exits: right, up)
  CornerTR = 0xB4,  // top-right corner (exits: left, down)
  CornerBR = 0xB5,  // bottom-right corner (exits: left, up)
  Intersection = 0xB6,
  StopNorth = 0xB7,   // endpoint: neighbor is south, cart departs south
  StopSouth = 0xB8,   // endpoint: neighbor is north, cart departs north
  StopWest = 0xB9,    // endpoint: neighbor is east, cart departs east
  StopEast = 0xBA,    // endpoint: neighbor is west, cart departs west
  TJuncNorth = 0xBB,  // closed north; exits south, east, west
  TJuncSouth = 0xBC,  // closed south; exits north, east, west
  TJuncEast = 0xBD,   // closed east; exits north, south, west
  TJuncWest = 0xBE,   // closed west; exits north, south, east
  SwitchTL = 0xD0,
  SwitchBL = 0xD1,
  SwitchTR = 0xD2,
  SwitchBR = 0xD3,
};

struct TrackCollisionResult {
  int room_id = 0;
  CustomCollisionMap collision_map;
  int tiles_generated = 0;
  int stop_count = 0;
  int corner_count = 0;
  int switch_count = 0;
  std::string ascii_visualization;
};

struct GeneratorOptions {
  // Track rail object ID to scan for (default: 0x31)
  int track_object_id = 0x31;

  // Positions to promote from regular corners to switch corners.
  // Each pair is (tile_x, tile_y) in the 64x64 collision grid.
  std::vector<std::pair<int, int>> switch_promotions;

  // Manual stop direction overrides: (tile_x, tile_y, tile_type).
  // Useful when the auto-detected direction isn't what you want.
  std::vector<std::tuple<int, int, TrackTileType>> stop_overrides;
};

// Session-only ownership: keep the exact cell displaced by an automatic rail
// stamp. An authored cell already matching the desired rail is never adopted.
struct MinecartTrackCollisionCellOwner {
  bool baseline_override = false;
  uint8_t baseline_value = 0;
  uint8_t generated_value = 0;

  bool operator==(const MinecartTrackCollisionCellOwner&) const = default;
};

using MinecartTrackCollisionOwnership =
    std::map<size_t, MinecartTrackCollisionCellOwner>;

struct MinecartTrackCollisionEdit {
  CustomCollisionMap collision;
  MinecartTrackCollisionOwnership ownership;
};

// Synchronize changed Oracle object 0x31 straight rails with their verified 2x2
// behavior: subtypes 0/7 are horizontal, 1/6 vertical. Changed corner/crossing
// pieces require explicit behavior editing until their stencils are verified.
// Only primary-list rails are supported. Unchanged geometry, authored
// stops/switches, and unowned departing cells are preserved. Owned departing
// cells restore their baseline only while their generated value still matches.
// Conflicting shapes, changed lower-layer/out-of-room rails, and new non-rail
// collision conflicts fail atomically. No Room, ROM, or source file is mutated.
// The editor must capture collision AND ownership in its object Undo action;
// ownership is discarded on room/session reload, never inferred from ROM data.
absl::StatusOr<MinecartTrackCollisionEdit> SynchronizeMinecartTrackCollision(
    std::span<const RoomObject> before_objects,
    std::span<const RoomObject> after_objects,
    const CustomCollisionMap& before_collision,
    const MinecartTrackCollisionOwnership& before_ownership = {});

// Build a collision map from rail objects in a room.
// Reads Object 0x31 (rail) positions, builds an occupancy grid,
// then classifies each tile by its neighbor connectivity.
absl::StatusOr<TrackCollisionResult> GenerateTrackCollision(
    Room* room, const GeneratorOptions& options = {});

// Write a generated collision map into the ROM. Reuses a room's uniquely owned
// blob when the replacement fits; otherwise appends encoded single-tile data
// and updates the pointer table at kCustomCollisionRoomPointers.
absl::Status WriteTrackCollision(Rom* rom, int room_id,
                                 const CustomCollisionMap& map);

// Generate an ASCII visualization of a collision map for debug/review.
std::string VisualizeCollisionMap(const CustomCollisionMap& map);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_TRACK_COLLISION_GENERATOR_H
