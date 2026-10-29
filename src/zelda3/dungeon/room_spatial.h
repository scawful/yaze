#ifndef YAZE_ZELDA3_DUNGEON_ROOM_SPATIAL_H_
#define YAZE_ZELDA3_DUNGEON_ROOM_SPATIAL_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/room_collision.h"
#include "zelda3/dungeon/room_layer_registers.h"

namespace yaze::zelda3 {

class DungeonState;

// All spatial coordinates are room-local Tile8 units, never sprite Tile16 or
// screen pixels. Bounds are half-open; a room is [0,64) on both axes.
struct RoomSpatialBounds {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  int right() const { return x + width; }
  int bottom() const { return y + height; }
};

struct RoomSpatialCell {
  int x = 0;
  int y = 0;
  // Yaze buffers: 0=upper/BG1, 1=lower/BG2; NOT the stream list index or
  // hardware BG number (upper/TILEMAPA uses hardware BG2).
  uint8_t layer = 0;
  uint16_t word = 0;
};

struct RoomObjectFootprint {
  size_t object_index =
      0;  // Index in its original layout or room-object vector.
  int object_id = 0;
  uint8_t size = 0;
  uint8_t list_index = 0;
  int anchor_x = 0;
  int anchor_y = 0;
  RoomSpatialBounds bounds;
  bool measured = false;
  // Strict placement must require this, not merely a nonempty clipped trace.
  bool full_bounds_measured = false;
  bool outside_room = false;
  bool is_layout = false;
  std::string measurement_source;
  ObjectLayerSemantics layers;
  // Actual trace writes, deduplicated by (layer,y,x), in deterministic order.
  // Geometry can identify an unclipped extent outside the room; trace cells
  // remain clipped by the existing renderer and must not claim outside cells.
  std::vector<RoomSpatialCell> cells;
  std::vector<std::string> diagnostics;
};

// Uses the active, already-bound project custom-object context. Copies the
// object, traces isolated ObjectDrawer writes and measures model bounds;
// never falls back to guessed dimensions or mutates ROM bytes. Invalid encoding
// and unsupported/draw failures are errors. A null state is the draw routine's
// default branch, not a statement about live gameplay state.
absl::StatusOr<RoomObjectFootprint> MeasureRoomObjectFootprint(
    Rom* rom, int room_id, const RoomObject& object,
    const DungeonState* state = nullptr);

struct RoomSpatialDoor {
  size_t index = 0;
  uint8_t position = 0;
  int direction = 0;
  int type = 0;
  int anchor_x = 0;
  int anchor_y = 0;
  RoomSpatialBounds bounds;
  bool valid_position = false;
};

struct RoomSpatialOverlap {
  size_t first_index = 0;
  size_t second_index = 0;
  bool first_is_layout = false;
  bool second_is_layout = false;
  size_t upper_cells = 0;
  size_t lower_cells = 0;
  // Describes shared writes in painter order, not a gameplay collision error.
  std::string classification;
};

struct RoomSpatialOptions {
  const DungeonState* object_state = nullptr;
  UnderworldRoomLoadState load_state;
  // Optional caller-rendered maps for this exact ROM/context/state. No render
  // or implicit default-state map creation is performed by the analysis.
  const RoomTilemaps* tilemaps = nullptr;
};

struct RoomSpatialAnalysis {
  int room_id = 0;
  int layout_id = 0;
  int floor1 = 0;  // Yaze upper/BG1 graphics selector; not a collision type.
  int floor2 = 0;  // Yaze lower/BG2 graphics selector; not a gameplay floor ID.
  int layer2_mode = 0;
  int collision_mode = 0;
  int effect = 0;
  int tag1 = 0;
  int tag2 = 0;
  RoomLayerRegisters registers;
  bool explicit_object_state = false;
  UnderworldRoomLoadState load_state;
  // Layout remains separate; objects are ordered primary, BG2 overlay, BG1
  // overlay, then special-table objects. Original vector indices are retained.
  std::vector<RoomObjectFootprint> layout;
  std::vector<RoomObjectFootprint> objects;
  std::vector<RoomSpatialDoor> doors;
  std::vector<RoomSpatialOverlap> overlaps;
  std::optional<RoomCollisionMaps> collision;
  size_t custom_collision_overrides = 0;
  std::vector<std::string> diagnostics;
};

// Read-only model preview. Visual overlap, tile priority and collision planes
// are reported separately. No generic passability or engine qualification is
// inferred, and absent/unsupported footprints stay explicitly unmeasured.
absl::StatusOr<RoomSpatialAnalysis> AnalyzeRoomSpatial(
    Rom* rom, const Room& room, const RoomSpatialOptions& options = {});

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_SPATIAL_H_
