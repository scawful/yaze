#include "zelda3/dungeon/track_collision_generator.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "core/features.h"
#include "rom/snes.h"
#include "rom/write_fence.h"
#include "util/macro.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

namespace {

constexpr int kGridSize = 64;
constexpr uint16_t kCollisionSingleTileMarker = 0xF0F0;
constexpr uint16_t kCollisionEndMarker = 0xFFFF;
constexpr int kFallbackTrackFootprintWidthTiles = 2;
constexpr int kFallbackTrackFootprintHeightTiles = 2;

struct CollisionBlob {
  int room_id = 0;
  uint32_t start = 0;
  uint32_t end = 0;
};

absl::StatusOr<uint32_t> FindCollisionBlobEnd(const std::vector<uint8_t>& data,
                                              uint32_t start, size_t safe_end,
                                              int room_id) {
  size_t cursor = start;
  bool single_mode = false;

  while (cursor + 1 < safe_end) {
    const uint16_t value = data[cursor] | (data[cursor + 1] << 8);
    cursor += 2;
    if (value == kCollisionEndMarker) {
      return static_cast<uint32_t>(cursor);
    }
    if (value == kCollisionSingleTileMarker) {
      single_mode = true;
      continue;
    }

    if (single_mode) {
      if (cursor >= safe_end) {
        break;
      }
      ++cursor;
      continue;
    }

    if (cursor + 1 >= safe_end) {
      break;
    }
    const size_t width = data[cursor];
    const size_t height = data[cursor + 1];
    cursor += 2;
    const size_t payload_size = width * height;
    if (payload_size > safe_end - cursor) {
      break;
    }
    cursor += payload_size;
  }

  return absl::FailedPreconditionError(absl::StrFormat(
      "Custom collision data for room 0x%02X is unterminated before "
      "WaterFill reserved region",
      room_id));
}

bool BlobsOverlap(const CollisionBlob& lhs, const CollisionBlob& rhs) {
  return lhs.start < rhs.end && rhs.start < lhs.end;
}

// Map corner type to its switch equivalent for promotion.
TrackTileType PromoteCornerToSwitch(TrackTileType corner) {
  switch (corner) {
    case TrackTileType::CornerTL:
      return TrackTileType::SwitchTL;
    case TrackTileType::CornerBL:
      return TrackTileType::SwitchBL;
    case TrackTileType::CornerTR:
      return TrackTileType::SwitchTR;
    case TrackTileType::CornerBR:
      return TrackTileType::SwitchBR;
    default:
      return corner;
  }
}

bool IsCornerTile(uint8_t tile) {
  return tile >= 0xB2 && tile <= 0xB5;
}

// Classify a tile based on its 4-neighbor connectivity.
//
// The algorithm: for each occupied tile in the grid, check which of the
// 4 cardinal neighbors are also occupied. The pattern of neighbors uniquely
// determines the tile type:
//   - 1 neighbor (endpoint) → stop tile, direction based on which neighbor
//   - 2 neighbors (line or corner) → straight or corner
//   - 3 neighbors → T-junction
//   - 4 neighbors → intersection
uint8_t ClassifyTile(bool up, bool down, bool left, bool right) {
  int count = (up ? 1 : 0) + (down ? 1 : 0) + (left ? 1 : 0) + (right ? 1 : 0);

  if (count == 0) {
    // Isolated tile — treat as intersection (shouldn't happen in practice)
    return static_cast<uint8_t>(TrackTileType::Intersection);
  }

  if (count == 1) {
    // Endpoint → stop tile. Direction is where the neighbor IS, because
    // the cart arrives from that direction and will depart back that way.
    if (down)
      return static_cast<uint8_t>(TrackTileType::StopNorth);
    if (up)
      return static_cast<uint8_t>(TrackTileType::StopSouth);
    if (right)
      return static_cast<uint8_t>(TrackTileType::StopWest);
    if (left)
      return static_cast<uint8_t>(TrackTileType::StopEast);
  }

  if (count == 2) {
    // Two neighbors — either a straight line or a corner
    if (left && right)
      return static_cast<uint8_t>(TrackTileType::HorizStraight);
    if (up && down)
      return static_cast<uint8_t>(TrackTileType::VertStraight);
    if (down && right)
      return static_cast<uint8_t>(TrackTileType::CornerTL);
    if (up && right)
      return static_cast<uint8_t>(TrackTileType::CornerBL);
    if (down && left)
      return static_cast<uint8_t>(TrackTileType::CornerTR);
    if (up && left)
      return static_cast<uint8_t>(TrackTileType::CornerBR);
  }

  if (count == 3) {
    // Oracle names T junctions for the CLOSED side. Its minecart input and
    // default-direction tables turn away from that edge (e.g. $BB blocks north).
    if (!up)
      return static_cast<uint8_t>(TrackTileType::TJuncNorth);
    if (!down)
      return static_cast<uint8_t>(TrackTileType::TJuncSouth);
    if (!left)
      return static_cast<uint8_t>(TrackTileType::TJuncWest);
    if (!right)
      return static_cast<uint8_t>(TrackTileType::TJuncEast);
  }

  // count == 4: full intersection
  return static_cast<uint8_t>(TrackTileType::Intersection);
}

// Get the tile label character for ASCII visualization.
char TileToChar(uint8_t tile) {
  switch (tile) {
    case 0xB0:
      return '-';  // horiz straight
    case 0xB1:
      return '|';  // vert straight
    case 0xB2:
      return '/';  // corner TL (down+right)
    case 0xB3:
      return '\\';  // corner BL (up+right)
    case 0xB4:
      return '\\';  // corner TR (down+left)
    case 0xB5:
      return '/';  // corner BR (up+left)
    case 0xB6:
      return '+';  // intersection
    case 0xB7:
      return 'N';  // stop north
    case 0xB8:
      return 'S';  // stop south
    case 0xB9:
      return 'W';  // stop west
    case 0xBA:
      return 'E';  // stop east
    case 0xBB:
      return 'T';  // T-junc north
    case 0xBC:
      return 'T';  // T-junc south
    case 0xBD:
      return 'T';  // T-junc east
    case 0xBE:
      return 'T';  // T-junc west
    case 0xD0:
      return '@';  // switch TL
    case 0xD1:
      return '@';  // switch BL
    case 0xD2:
      return '@';  // switch TR
    case 0xD3:
      return '@';  // switch BR
    default:
      return '.';
  }
}

DimensionService::DimensionResult ResolveTrackObjectDimensions(
    const RoomObject& obj, const GeneratorOptions& options,
    const DimensionService& dimension_service) {
  auto dims = dimension_service.GetDimensions(obj);

  // Object 0x31's size byte selects a custom-object subtype; it is not a
  // vanilla width/height nibble. Without project custom-object geometry,
  // DimensionService falls through to that vanilla interpretation and gives
  // non-zero subtypes incorrect variable-width footprints. Oracle's track
  // assets are all authored as 2x2 pieces, so use that canonical footprint in
  // the project-free CLI path. Configured non-0x31 track IDs continue to use
  // DimensionService normally.
  const bool is_canonical_oos_track =
      obj.id_ == 0x31 &&
      obj.id_ == static_cast<int16_t>(options.track_object_id);
  if (is_canonical_oos_track &&
      !core::FeatureFlags::get().kEnableCustomObjects) {
    dims.offset_x_tiles = 0;
    dims.offset_y_tiles = 0;
    dims.width_tiles = kFallbackTrackFootprintWidthTiles;
    dims.height_tiles = kFallbackTrackFootprintHeightTiles;
    return dims;
  }

  // Also recover when custom objects are enabled but a configured subtype has
  // no loaded payload and therefore measures as the 1x1 fallback.
  if (is_canonical_oos_track && dims.offset_x_tiles == 0 &&
      dims.offset_y_tiles == 0 && dims.width_tiles == 1 &&
      dims.height_tiles == 1) {
    dims.width_tiles = kFallbackTrackFootprintWidthTiles;
    dims.height_tiles = kFallbackTrackFootprintHeightTiles;
  }

  return dims;
}

struct CanonicalRailStamp {
  int x;
  int y;
  int layer;
  MinecartRailShape shape;
  std::array<uint8_t, 4> stencil;
  bool verified;
  uint8_t mask;

  bool operator==(const CanonicalRailStamp&) const = default;
  bool operator<(const CanonicalRailStamp& other) const {
    return std::tie(x, y, layer, shape, stencil, verified, mask) <
           std::tie(other.x, other.y, other.layer, other.shape, other.stencil,
                    other.verified, other.mask);
  }
};

std::vector<CanonicalRailStamp> CanonicalRailStamps(
    std::span<const RoomObject> objects, MinecartRailProfile profile) {
  std::vector<CanonicalRailStamp> stamps;
  for (const auto& object : objects) {
    const auto* rail =
        FindMinecartRailDescriptor(object.id_, object.size_, profile);
    if (rail == nullptr) {
      continue;
    }
    stamps.push_back({object.x_, object.y_, object.GetLayerValue(), rail->shape,
                      rail->collision_stencil, rail->collision_verified,
                      rail->collision_mask});
  }
  std::sort(stamps.begin(), stamps.end());
  return stamps;
}

bool HasValidRailFootprint(const CanonicalRailStamp& stamp) {
  return stamp.layer == 0 && stamp.x >= 0 && stamp.y >= 0 &&
         stamp.x <= kGridSize - 2 && stamp.y <= kGridSize - 2;
}

struct CanonicalRailGrid {
  std::array<uint8_t, kGridSize * kGridSize> values{};
  std::bitset<kGridSize * kGridSize> present;
  std::bitset<kGridSize * kGridSize> conflicts;
};

CanonicalRailGrid BuildCanonicalRailGrid(
    const std::vector<CanonicalRailStamp>& stamps) {
  CanonicalRailGrid grid;
  for (const auto& stamp : stamps) {
    // Unchanged unsupported geometry does not block unrelated object edits.
    if (!HasValidRailFootprint(stamp)) {
      continue;
    }
    for (int dy = 0; dy < 2; ++dy) {
      for (int dx = 0; dx < 2; ++dx) {
        if ((stamp.mask & (1 << (dy * 2 + dx))) == 0) {
          continue;
        }
        const size_t offset = (stamp.y + dy) * kGridSize + stamp.x + dx;
        const uint8_t value = stamp.stencil[dy * 2 + dx];
        if (!stamp.verified ||
            (grid.present.test(offset) && grid.values[offset] != value)) {
          grid.conflicts.set(offset);
        }
        grid.values[offset] = value;
        grid.present.set(offset);
      }
    }
  }
  return grid;
}

bool IsOrdinaryRail(uint8_t value) {
  return value >= 0xB0 && value <= 0xB6;
}

bool IsProtectedRailBehavior(uint8_t value) {
  return (value >= 0xB7 && value <= 0xBE) || (value >= 0xD0 && value <= 0xD3);
}

absl::StatusOr<TrackCollisionResult> GenerateCanonicalTrackCollision(
    Room& room, const GeneratorOptions& options) {
  const auto stamps =
      CanonicalRailStamps(room.GetTileObjects(), options.rail_profile);
  for (const auto& stamp : stamps) {
    if (!stamp.verified || !HasValidRailFootprint(stamp)) {
      return absl::FailedPreconditionError(
          "Track generation requires verified upper-layer rail pieces fully "
          "inside the room");
    }
  }
  const auto grid = BuildCanonicalRailGrid(stamps);
  if (grid.conflicts.any()) {
    return absl::FailedPreconditionError(
        "Conflicting rail shapes overlap collision cells");
  }

  TrackCollisionResult result;
  result.room_id = room.id();
  // Batch generation is also a candidate operation: preserve all existing
  // overrides, including explicit floor and manually authored node behavior.
  result.collision_map = room.custom_collision();
  for (size_t offset = 0; offset < grid.values.size(); ++offset) {
    if (!grid.present.test(offset)) {
      continue;
    }
    const uint8_t current = result.collision_map.tiles[offset];
    if (result.collision_map.HasOverride(offset)) {
      if (current != 0 && !IsOrdinaryRail(current) &&
          !IsProtectedRailBehavior(current)) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Other collision occupies rail cell %zu; edit collision explicitly",
            offset));
      }
      // Unlike a new object placement, a batch has no before/after geometry
      // proving that a saved cell should change. Its authored stencil wins.
    } else {
      result.collision_map.SetOverride(offset, grid.values[offset]);
    }
    ++result.tiles_generated;
    const uint8_t value = result.collision_map.tiles[offset];
    result.stop_count += value >= 0xB7 && value <= 0xBA;
    result.corner_count += IsCornerTile(value);
    result.switch_count += value >= 0xD0 && value <= 0xD3;
  }
  return result;
}

}  // namespace

absl::StatusOr<MinecartTrackCollisionEdit> SynchronizeMinecartTrackCollision(
    std::span<const RoomObject> before_objects,
    std::span<const RoomObject> after_objects,
    const CustomCollisionMap& before_collision,
    const MinecartTrackCollisionOwnership& before_ownership,
    MinecartRailProfile profile) {
  MinecartTrackCollisionEdit result{before_collision, before_ownership};
  const auto before_stamps = CanonicalRailStamps(before_objects, profile);
  const auto after_stamps = CanonicalRailStamps(after_objects, profile);
  std::vector<CanonicalRailStamp> changed;
  std::set_symmetric_difference(before_stamps.begin(), before_stamps.end(),
                                after_stamps.begin(), after_stamps.end(),
                                std::back_inserter(changed));
  if (changed.empty()) {
    return result;
  }

  std::bitset<kGridSize * kGridSize> touched;
  for (const auto& stamp : changed) {
    if (!stamp.verified) {
      return absl::FailedPreconditionError(
          "This rail has no verified automatic collision stencil");
    }
    if (!HasValidRailFootprint(stamp)) {
      return absl::FailedPreconditionError(
          "Automatic rail collision requires upper-layer 16 pixel pieces "
          "fully inside the room");
    }
    for (int dy = 0; dy < 2; ++dy) {
      for (int dx = 0; dx < 2; ++dx) {
        if ((stamp.mask & (1 << (dy * 2 + dx))) != 0) {
          touched.set((stamp.y + dy) * kGridSize + stamp.x + dx);
        }
      }
    }
  }
  const auto before_grid = BuildCanonicalRailGrid(before_stamps);
  const auto after_grid = BuildCanonicalRailGrid(after_stamps);
  for (size_t offset = 0; offset < before_collision.tiles.size(); ++offset) {
    if (!touched.test(offset)) {
      continue;
    }
    if (before_grid.conflicts.test(offset) ||
        after_grid.conflicts.test(offset)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Conflicting rail shapes overlap collision cell %zu", offset));
    }
    const bool desired_present = after_grid.present.test(offset);
    if (before_grid.present.test(offset) == desired_present &&
        (!desired_present ||
         before_grid.values[offset] == after_grid.values[offset])) {
      continue;
    }

    const uint8_t desired = after_grid.values[offset];
    auto owner = result.ownership.find(offset);
    if (owner != result.ownership.end()) {
      if (before_collision.tiles[offset] != owner->second.generated_value ||
          !before_collision.HasOverride(offset)) {
        // A manual edit invalidates ownership; never restore over that edit.
        result.ownership.erase(owner);
      } else if (!desired_present) {
        result.collision.tiles[offset] = owner->second.baseline_value;
        result.collision.overrides.set(offset, owner->second.baseline_override);
        result.ownership.erase(owner);
        continue;
      } else {
        result.collision.SetOverride(offset, desired);
        owner->second.generated_value = desired;
        continue;
      }
    }
    if (!desired_present) {
      continue;  // Existing authored rails have no session ownership.
    }
    const uint8_t current = before_collision.tiles[offset];
    if (IsProtectedRailBehavior(current)) {
      continue;
    }
    if (before_collision.HasOverride(offset) && current != 0 &&
        !IsOrdinaryRail(current)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Other collision occupies rail cell %zu; edit collision explicitly",
          offset));
    }
    if (current == desired && before_collision.HasOverride(offset)) {
      continue;  // Do not adopt a matching authored rail.
    }
    result.ownership[offset] = {before_collision.overrides.test(offset),
                                current, desired};
    result.collision.SetOverride(offset, desired);
  }
  return result;
}

absl::StatusOr<TrackCollisionResult> GenerateTrackCollision(
    Room* room, const GeneratorOptions& options) {
  if (!room) {
    return absl::InvalidArgumentError("Room pointer is null");
  }

  // An empty object list can be a valid unsaved edit. The explicit loaded flag
  // is the only safe authority for deciding whether ROM data must be parsed.
  room->EnsureObjectsLoaded();

  TrackCollisionResult result;
  if (options.track_object_id == 0x31) {
    ASSIGN_OR_RETURN(result, GenerateCanonicalTrackCollision(*room, options));
  } else {
    result.room_id = room->id();
    result.collision_map.tiles.fill(0);

    // Step 1: Build occupancy grid from rail objects.
    // Rail objects (ID 0x31) have coordinates in the room's tile space.
    // RoomObject x_ and y_ are in tile coordinates (each tile = 8 pixels).
    // The collision grid is 64x64 (covering 512x512 pixels = full room).
    std::array<bool, kGridSize * kGridSize> occupied{};
    auto& dimension_service = DimensionService::Get();

    for (const auto& obj : room->GetTileObjects()) {
      if (obj.id_ != static_cast<int16_t>(options.track_object_id)) {
        continue;
      }
      const int subtype = obj.size_ & 0x1F;
      if (!IsMinecartTrackGraphicsSubtype(options.track_object_id, subtype,
                                          options.rail_profile)) {
        continue;
      }

      const auto dims =
          ResolveTrackObjectDimensions(obj, options, dimension_service);
      int base_x = obj.x_ + dims.offset_x_tiles;
      int base_y = obj.y_ + dims.offset_y_tiles;
      int w = std::max(1, dims.width_tiles);
      int h = std::max(1, dims.height_tiles);

      for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
          int gx = base_x + dx;
          int gy = base_y + dy;
          if (gx >= 0 && gx < kGridSize && gy >= 0 && gy < kGridSize) {
            occupied[gy * kGridSize + gx] = true;
          }
        }
      }
    }

    // Step 2: Classify each occupied tile by neighbor connectivity.
    for (int y = 0; y < kGridSize; ++y) {
      for (int x = 0; x < kGridSize; ++x) {
        if (!occupied[y * kGridSize + x])
          continue;

        bool up = (y > 0) && occupied[(y - 1) * kGridSize + x];
        bool down = (y < kGridSize - 1) && occupied[(y + 1) * kGridSize + x];
        bool left = (x > 0) && occupied[y * kGridSize + (x - 1)];
        bool right = (x < kGridSize - 1) && occupied[y * kGridSize + (x + 1)];

        uint8_t tile = ClassifyTile(up, down, left, right);
        result.collision_map.tiles[y * kGridSize + x] = tile;
        result.tiles_generated++;

        if (tile >= 0xB7 && tile <= 0xBA)
          result.stop_count++;
        if (tile >= 0xB2 && tile <= 0xB5)
          result.corner_count++;
      }
    }
  }

  // Step 3: Apply switch promotions.
  for (const auto& [sx, sy] : options.switch_promotions) {
    if (sx < 0 || sx >= kGridSize || sy < 0 || sy >= kGridSize)
      continue;
    size_t idx = sy * kGridSize + sx;
    uint8_t tile = result.collision_map.tiles[idx];
    if (IsCornerTile(tile)) {
      result.collision_map.tiles[idx] = static_cast<uint8_t>(
          PromoteCornerToSwitch(static_cast<TrackTileType>(tile)));
      result.corner_count--;
      result.switch_count++;
    }
  }

  // Step 4: Apply manual stop overrides.
  for (const auto& [ox, oy, otype] : options.stop_overrides) {
    if (ox < 0 || ox >= kGridSize || oy < 0 || oy >= kGridSize)
      continue;
    size_t idx = oy * kGridSize + ox;
    if (result.collision_map.tiles[idx] != 0) {
      result.collision_map.tiles[idx] = static_cast<uint8_t>(otype);
    }
  }

  result.collision_map.has_data =
      result.collision_map.has_data || result.tiles_generated > 0;
  result.ascii_visualization = VisualizeCollisionMap(result.collision_map);

  return result;
}

absl::Status WriteTrackCollision(Rom* rom, int room_id,
                                 const CustomCollisionMap& map) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return absl::OutOfRangeError("Room ID out of range");
  }

  const auto& data = rom->vector();
  if (data.empty()) {
    return absl::FailedPreconditionError("ROM vector is empty");
  }

  const int ptrs_size = kNumberOfRooms * 3;
  if (kCustomCollisionRoomPointers + ptrs_size >
      static_cast<int>(data.size())) {
    return absl::FailedPreconditionError(
        "Custom collision pointer table not present in this ROM");
  }
  if (kCustomCollisionDataPosition >= static_cast<int>(data.size())) {
    return absl::FailedPreconditionError(
        "Custom collision data region not present in this ROM");
  }
  if (kCustomCollisionDataSoftEnd > static_cast<int>(data.size())) {
    return absl::FailedPreconditionError(
        "Custom collision data region truncated (ROM too small)");
  }

  // Save-time guardrails: only allow writes to the collision pointer table and
  // collision data bank (excluding the reserved WaterFill tail region).
  yaze::rom::WriteFence fence;
  RETURN_IF_ERROR(fence.Allow(
      static_cast<uint32_t>(kCustomCollisionRoomPointers),
      static_cast<uint32_t>(kCustomCollisionRoomPointers + ptrs_size),
      "CustomCollisionPointers"));
  RETURN_IF_ERROR(
      fence.Allow(static_cast<uint32_t>(kCustomCollisionDataPosition),
                  static_cast<uint32_t>(kCustomCollisionDataSoftEnd),
                  "CustomCollisionData"));
  yaze::rom::ScopedWriteFence scope(rom, &fence);

  // Encode collision data in single-tile format.
  // Format: [F0 F0] [offset_lo offset_hi tile] ... [FF FF]
  std::vector<uint8_t> encoded;
  encoded.push_back(0xF0);
  encoded.push_back(0xF0);

  for (int y = 0; y < kGridSize; ++y) {
    for (int x = 0; x < kGridSize; ++x) {
      uint8_t tile = map.tiles[y * kGridSize + x];
      if (!map.HasOverride(y * kGridSize + x))
        continue;
      uint16_t offset = static_cast<uint16_t>(y * kGridSize + x);
      encoded.push_back(offset & 0xFF);
      encoded.push_back(offset >> 8);
      encoded.push_back(tile);
    }
  }
  encoded.push_back(0xFF);
  encoded.push_back(0xFF);

  // Find the end of existing collision data by scanning all room pointers
  // to determine the highest used offset.
  const size_t safe_end =
      std::min(static_cast<size_t>(data.size()),
               static_cast<size_t>(kCustomCollisionDataSoftEnd));
  uint32_t max_used_pc = static_cast<uint32_t>(kCustomCollisionDataPosition);
  std::vector<CollisionBlob> blobs;
  blobs.reserve(kNumberOfRooms);
  for (int r = 0; r < kNumberOfRooms; ++r) {
    int ptr_offset = kCustomCollisionRoomPointers + (r * 3);
    if (ptr_offset + 2 >= static_cast<int>(data.size()))
      continue;

    uint32_t snes_ptr = data[ptr_offset] | (data[ptr_offset + 1] << 8) |
                        (data[ptr_offset + 2] << 16);
    if (snes_ptr == 0)
      continue;

    uint32_t pc = SnesToPc(snes_ptr);
    if (pc < static_cast<uint32_t>(kCustomCollisionDataPosition)) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Custom collision pointer for room 0x%02X points "
                          "before data region (pc=0x%06X)",
                          r, pc));
    }
    if (pc >= static_cast<uint32_t>(kCustomCollisionDataSoftEnd)) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Custom collision pointer for room 0x%02X overlaps "
                          "WaterFill reserved region (pc=0x%06X)",
                          r, pc));
    }
    if (pc >= data.size()) {
      return absl::OutOfRangeError("Custom collision pointer out of ROM range");
    }
    ASSIGN_OR_RETURN(const uint32_t end_pc,
                     FindCollisionBlobEnd(data, pc, safe_end, r));
    blobs.push_back(CollisionBlob{r, pc, end_pc});
    if (end_pc > max_used_pc) {
      max_used_pc = end_pc;
    }
  }

  // Reuse the selected room's current blob only when its entire physical span
  // is uniquely owned and the replacement fits. Aliased or overlapping blobs
  // remain copy-on-write so editing one room cannot mutate another room.
  uint32_t write_pos = max_used_pc;
  const auto target = std::find_if(
      blobs.begin(), blobs.end(),
      [room_id](const CollisionBlob& blob) { return blob.room_id == room_id; });
  if (target != blobs.end() && encoded.size() <= target->end - target->start) {
    const bool overlaps_other = std::any_of(
        blobs.begin(), blobs.end(), [&](const CollisionBlob& other) {
          return other.room_id != room_id && BlobsOverlap(*target, other);
        });
    if (!overlaps_other) {
      write_pos = target->start;
    }
  }

  // Append when there is no safe reusable span, then check available space.
  if (write_pos + encoded.size() > kCustomCollisionDataSoftEnd) {
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Not enough collision data space. Need %d bytes at 0x%06X, "
        "region ends at 0x%06X",
        encoded.size(), write_pos, kCustomCollisionDataSoftEnd));
  }

  if (write_pos + encoded.size() > data.size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("ROM too small for custom collision write (need "
                        "end=0x%06X, size=0x%06X)",
                        write_pos + encoded.size(), data.size()));
  }
  RETURN_IF_ERROR(
      rom->WriteVector(static_cast<int>(write_pos), std::move(encoded)));

  // Update pointer table: 3-byte SNES address
  uint32_t snes_addr = PcToSnes(write_pos);
  int ptr_offset = kCustomCollisionRoomPointers + (room_id * 3);
  RETURN_IF_ERROR(rom->WriteByte(ptr_offset, snes_addr & 0xFF));
  RETURN_IF_ERROR(rom->WriteByte(ptr_offset + 1, (snes_addr >> 8) & 0xFF));
  RETURN_IF_ERROR(rom->WriteByte(ptr_offset + 2, (snes_addr >> 16) & 0xFF));

  return absl::OkStatus();
}

std::string VisualizeCollisionMap(const CustomCollisionMap& map) {
  // Find bounding box of non-zero tiles to avoid printing the entire 64x64.
  int min_x = kGridSize, max_x = 0, min_y = kGridSize, max_y = 0;
  for (int y = 0; y < kGridSize; ++y) {
    for (int x = 0; x < kGridSize; ++x) {
      if (map.tiles[y * kGridSize + x] != 0) {
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
      }
    }
  }

  if (min_x > max_x)
    return "(empty)\n";

  // Add 1-tile padding
  min_x = std::max(0, min_x - 1);
  min_y = std::max(0, min_y - 1);
  max_x = std::min(kGridSize - 1, max_x + 1);
  max_y = std::min(kGridSize - 1, max_y + 1);

  std::stringstream ss;
  // Column header
  ss << "    ";
  for (int x = min_x; x <= max_x; ++x) {
    ss << absl::StrFormat("%X", x % 16);
  }
  ss << "\n";

  for (int y = min_y; y <= max_y; ++y) {
    ss << absl::StrFormat("%02X: ", y);
    for (int x = min_x; x <= max_x; ++x) {
      uint8_t tile = map.tiles[y * kGridSize + x];
      ss << TileToChar(tile);
    }
    ss << "\n";
  }

  return ss.str();
}

}  // namespace zelda3
}  // namespace yaze
