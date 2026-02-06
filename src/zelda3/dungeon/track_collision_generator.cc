#include "zelda3/dungeon/track_collision_generator.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

namespace {

constexpr int kGridSize = 64;
constexpr uint16_t kCollisionSingleTileMarker = 0xF0F0;
constexpr uint16_t kCollisionEndMarker = 0xFFFF;

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
  int count = (up ? 1 : 0) + (down ? 1 : 0) + (left ? 1 : 0) +
              (right ? 1 : 0);

  if (count == 0) {
    // Isolated tile — treat as intersection (shouldn't happen in practice)
    return static_cast<uint8_t>(TrackTileType::Intersection);
  }

  if (count == 1) {
    // Endpoint → stop tile. Direction is where the neighbor IS, because
    // the cart arrives from that direction and will depart back that way.
    if (down) return static_cast<uint8_t>(TrackTileType::StopNorth);
    if (up) return static_cast<uint8_t>(TrackTileType::StopSouth);
    if (right) return static_cast<uint8_t>(TrackTileType::StopWest);
    if (left) return static_cast<uint8_t>(TrackTileType::StopEast);
  }

  if (count == 2) {
    // Two neighbors — either a straight line or a corner
    if (left && right) return static_cast<uint8_t>(TrackTileType::HorizStraight);
    if (up && down) return static_cast<uint8_t>(TrackTileType::VertStraight);
    if (down && right) return static_cast<uint8_t>(TrackTileType::CornerTL);
    if (up && right) return static_cast<uint8_t>(TrackTileType::CornerBL);
    if (down && left) return static_cast<uint8_t>(TrackTileType::CornerTR);
    if (up && left) return static_cast<uint8_t>(TrackTileType::CornerBR);
  }

  if (count == 3) {
    // T-junction — named for the direction WITHOUT a neighbor
    if (!up) return static_cast<uint8_t>(TrackTileType::TJuncSouth);
    if (!down) return static_cast<uint8_t>(TrackTileType::TJuncNorth);
    if (!left) return static_cast<uint8_t>(TrackTileType::TJuncEast);
    if (!right) return static_cast<uint8_t>(TrackTileType::TJuncWest);
  }

  // count == 4: full intersection
  return static_cast<uint8_t>(TrackTileType::Intersection);
}

// Get the tile label character for ASCII visualization.
char TileToChar(uint8_t tile) {
  switch (tile) {
    case 0xB0: return '-';   // horiz straight
    case 0xB1: return '|';   // vert straight
    case 0xB2: return '/';   // corner TL (down+right)
    case 0xB3: return '\\';  // corner BL (up+right)
    case 0xB4: return '\\';  // corner TR (down+left)
    case 0xB5: return '/';   // corner BR (up+left)
    case 0xB6: return '+';   // intersection
    case 0xB7: return 'N';   // stop north
    case 0xB8: return 'S';   // stop south
    case 0xB9: return 'W';   // stop west
    case 0xBA: return 'E';   // stop east
    case 0xBB: return 'T';   // T-junc north
    case 0xBC: return 'T';   // T-junc south
    case 0xBD: return 'T';   // T-junc east
    case 0xBE: return 'T';   // T-junc west
    case 0xD0: return '@';   // switch TL
    case 0xD1: return '@';   // switch BL
    case 0xD2: return '@';   // switch TR
    case 0xD3: return '@';   // switch BR
    default: return '.';
  }
}

}  // namespace

absl::StatusOr<TrackCollisionResult> GenerateTrackCollision(
    Room* room, const GeneratorOptions& options) {
  if (!room) {
    return absl::InvalidArgumentError("Room pointer is null");
  }

  // Ensure objects are loaded
  if (room->GetTileObjects().empty()) {
    room->LoadObjects();
  }

  TrackCollisionResult result;
  result.room_id = room->id();
  result.collision_map.tiles.fill(0);

  // Step 1: Build occupancy grid from rail objects.
  // Rail objects (ID 0x31) have coordinates in the room's tile space.
  // RoomObject x_ and y_ are in tile coordinates (each tile = 8 pixels).
  // The collision grid is 64x64 (covering 512x512 pixels = full room).
  std::array<bool, kGridSize * kGridSize> occupied{};

  for (const auto& obj : room->GetTileObjects()) {
    if (obj.id_ != static_cast<int16_t>(options.track_object_id)) {
      continue;
    }

    // Object x_/y_ are tile coordinates in the room.
    // Object size determines extent. For rail objects (0x31), the size
    // field encodes the track subtype in bits 0-4 and direction/extent
    // information varies. We use width_/height_ which are computed during
    // object loading and represent the tile extent.
    int base_x = obj.x_;
    int base_y = obj.y_;
    int w = std::max(1, obj.width_);
    int h = std::max(1, obj.height_);

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
      if (!occupied[y * kGridSize + x]) continue;

      bool up = (y > 0) && occupied[(y - 1) * kGridSize + x];
      bool down = (y < kGridSize - 1) && occupied[(y + 1) * kGridSize + x];
      bool left = (x > 0) && occupied[y * kGridSize + (x - 1)];
      bool right = (x < kGridSize - 1) && occupied[y * kGridSize + (x + 1)];

      uint8_t tile = ClassifyTile(up, down, left, right);
      result.collision_map.tiles[y * kGridSize + x] = tile;
      result.tiles_generated++;

      if (tile >= 0xB7 && tile <= 0xBA) result.stop_count++;
      if (tile >= 0xB2 && tile <= 0xB5) result.corner_count++;
    }
  }

  // Step 3: Apply switch promotions.
  for (const auto& [sx, sy] : options.switch_promotions) {
    if (sx < 0 || sx >= kGridSize || sy < 0 || sy >= kGridSize) continue;
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
    if (ox < 0 || ox >= kGridSize || oy < 0 || oy >= kGridSize) continue;
    size_t idx = oy * kGridSize + ox;
    if (result.collision_map.tiles[idx] != 0) {
      result.collision_map.tiles[idx] = static_cast<uint8_t>(otype);
    }
  }

  result.collision_map.has_data = (result.tiles_generated > 0);
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

  auto& data = rom->mutable_vector();
  if (data.empty()) {
    return absl::FailedPreconditionError("ROM vector is empty");
  }

  // Encode collision data in single-tile format.
  // Format: [F0 F0] [offset_lo offset_hi tile] ... [FF FF]
  std::vector<uint8_t> encoded;
  encoded.push_back(0xF0);
  encoded.push_back(0xF0);

  for (int y = 0; y < kGridSize; ++y) {
    for (int x = 0; x < kGridSize; ++x) {
      uint8_t tile = map.tiles[y * kGridSize + x];
      if (tile == 0) continue;
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
  uint32_t max_used_pc = kCustomCollisionDataPosition;
  for (int r = 0; r < kNumberOfRooms; ++r) {
    int ptr_offset = kCustomCollisionRoomPointers + (r * 3);
    if (ptr_offset + 2 >= static_cast<int>(data.size())) continue;

    uint32_t snes_ptr = data[ptr_offset] | (data[ptr_offset + 1] << 8) |
                        (data[ptr_offset + 2] << 16);
    if (snes_ptr == 0) continue;

    uint32_t pc = SnesToPc(snes_ptr);
    // Walk past this room's data to find its end
    size_t cursor = pc;
    bool single_mode = false;
    while (cursor + 1 < data.size()) {
      uint16_t val = data[cursor] | (data[cursor + 1] << 8);
      cursor += 2;
      if (val == kCollisionEndMarker) break;
      if (val == kCollisionSingleTileMarker) {
        single_mode = true;
        continue;
      }
      if (!single_mode) {
        // Rectangle mode: skip width, height, then width*height bytes
        if (cursor + 1 >= data.size()) break;
        uint8_t w = data[cursor];
        uint8_t h = data[cursor + 1];
        cursor += 2;
        cursor += w * h;
      } else {
        // Single tile mode: skip 1 byte (tile value)
        cursor += 1;
      }
    }
    if (cursor > max_used_pc) {
      max_used_pc = static_cast<uint32_t>(cursor);
    }
  }

  // Check if there's enough space
  uint32_t write_pos = max_used_pc;
  if (write_pos + encoded.size() > kCustomCollisionDataEnd) {
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Not enough collision data space. Need %d bytes at 0x%06X, "
        "region ends at 0x%06X",
        encoded.size(), write_pos, kCustomCollisionDataEnd));
  }

  // Ensure ROM vector is large enough
  if (write_pos + encoded.size() > data.size()) {
    data.resize(write_pos + encoded.size(), 0);
  }

  // Write encoded data
  for (size_t i = 0; i < encoded.size(); ++i) {
    data[write_pos + i] = encoded[i];
  }

  // Update pointer table: 3-byte SNES address
  uint32_t snes_addr = PcToSnes(write_pos);
  int ptr_offset = kCustomCollisionRoomPointers + (room_id * 3);
  data[ptr_offset] = snes_addr & 0xFF;
  data[ptr_offset + 1] = (snes_addr >> 8) & 0xFF;
  data[ptr_offset + 2] = (snes_addr >> 16) & 0xFF;

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

  if (min_x > max_x) return "(empty)\n";

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
