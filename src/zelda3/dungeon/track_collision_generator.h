#ifndef YAZE_ZELDA3_DUNGEON_TRACK_COLLISION_GENERATOR_H
#define YAZE_ZELDA3_DUNGEON_TRACK_COLLISION_GENERATOR_H

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_collision.h"
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
  StopNorth = 0xB7,  // endpoint: neighbor is south, cart departs south
  StopSouth = 0xB8,  // endpoint: neighbor is north, cart departs north
  StopWest = 0xB9,   // endpoint: neighbor is east, cart departs east
  StopEast = 0xBA,   // endpoint: neighbor is west, cart departs west
  TJuncNorth = 0xBB,
  TJuncSouth = 0xBC,
  TJuncEast = 0xBD,
  TJuncWest = 0xBE,
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

// Build a collision map from rail objects in a room.
// Reads Object 0x31 (rail) positions, builds an occupancy grid,
// then classifies each tile by its neighbor connectivity.
absl::StatusOr<TrackCollisionResult> GenerateTrackCollision(
    Room* room, const GeneratorOptions& options = {});

// Write a generated collision map into the ROM.
// Updates the pointer table at kCustomCollisionRoomPointers and appends
// encoded data in single-tile format after existing collision data.
absl::Status WriteTrackCollision(Rom* rom, int room_id,
                                 const CustomCollisionMap& map);

// Generate an ASCII visualization of a collision map for debug/review.
std::string VisualizeCollisionMap(const CustomCollisionMap& map);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_TRACK_COLLISION_GENERATOR_H
