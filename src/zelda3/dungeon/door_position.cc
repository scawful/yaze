#include "door_position.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace yaze {
namespace zelda3 {

namespace {

constexpr int TilemapOffsetToTileX(uint16_t offset) {
  return static_cast<int>((offset % 0x80) / 2);
}

constexpr int TilemapOffsetToTileY(uint16_t offset) {
  return static_cast<int>(offset / 0x80) - 4;
}

constexpr std::array<uint16_t, 12> kNorthDoorTilemapOffsets = {
    0x021C, 0x023C, 0x025C, 0x039C, 0x03BC, 0x03DC,
    0x121C, 0x123C, 0x125C, 0x139C, 0x13BC, 0x13DC};

constexpr std::array<uint16_t, 12> kSouthDoorTilemapOffsets = {
    0x0D1C, 0x0D3C, 0x0D5C, 0x0B9C, 0x0BBC, 0x0BDC,
    0x1D1C, 0x1D3C, 0x1D5C, 0x1B9C, 0x1BBC, 0x1BDC};

constexpr std::array<uint16_t, 12> kWestDoorTilemapOffsets = {
    0x0784, 0x0F84, 0x1784, 0x078A, 0x0F8A, 0x178A,
    0x07C4, 0x0FC4, 0x17C4, 0x07CA, 0x0FCA, 0x17CA};

constexpr std::array<uint16_t, 12> kEastDoorTilemapOffsets = {
    0x07B4, 0x0FB4, 0x17B4, 0x07AE, 0x0FAE, 0x17AE,
    0x07F4, 0x0FF4, 0x17F4, 0x07EE, 0x0FEE, 0x17EE};

const std::array<uint16_t, 12>& DoorTilemapOffsets(DoorDirection direction) {
  switch (direction) {
    case DoorDirection::North:
      return kNorthDoorTilemapOffsets;
    case DoorDirection::South:
      return kSouthDoorTilemapOffsets;
    case DoorDirection::West:
      return kWestDoorTilemapOffsets;
    case DoorDirection::East:
      return kEastDoorTilemapOffsets;
  }

  return kNorthDoorTilemapOffsets;
}

}  // namespace

// ROM addresses for door position tables (PC addresses, bank $00)
// Each direction has TWO consecutive tables for a total of 12 positions:
//   - Positions 0-5: Wall table (outer edges of room)
//   - Positions 6-11: Middle table (internal seams between quadrants)
//
// Table layout in ROM (consecutive in memory):
//   NorthWall ($997E): $021C,$023C,$025C,$039C,$03BC,$03DC (6 entries)
//   NorthMiddle ($998A): $121C,$123C,$125C,$139C,$13BC,$13DC (6 entries)
//   SouthMiddle ($9996): $0D1C,$0D3C,$0D5C,$0B9C,$0BBC,$0BDC,$1D1C,$1D3C,$1D5C (9 entries)
//   LowerLayerEntrance ($99A8): $1B9C,$1BBC,$1BDC (3 entries)
//   WestWall ($99AE): $0784,$0F84,$1784,$078A,$0F8A,$178A (6 entries)
//   WestMiddle ($99BA): $07C4,$0FC4,$17C4,$07CA,$0FCA,$17CA (6 entries)
//   EastMiddle ($99C6): $07B4,$0FB4,$17B4,$07AE,$0FAE,$17AE (6 entries)
//   EastWall ($99D2): $07F4,$0FF4,$17F4,$07EE,$0FEE,$17EE (6 entries)
//
// VRAM offset to room tile conversion:
//   room_x = (offset % 0x80) / 2  (NO margin subtraction for editor bitmap)
//   room_y = (offset / 0x80) - 4  (subtract 4-tile top margin for Y only)
// Note: The 2-tile left margin should NOT be subtracted for the editor's
// 64x64 tile bitmap, as the room content fills the entire bitmap.
//
// Room layout: 64x64 tiles divided into 4 quadrants (32x32 each)
// X positions for N/S doors: 14, 30, 46 (left/center/right, +2 X offset from VRAM)
// Y positions for E/W doors: 15, 31, 47 (distributed across 64 tiles, +4 Y offset from VRAM)
[[maybe_unused]] constexpr int kDoorPosNorthAddr = 0x197E;
[[maybe_unused]] constexpr int kDoorPosSouthAddr = 0x198A;
[[maybe_unused]] constexpr int kDoorPosWestAddr = 0x1996;
[[maybe_unused]] constexpr int kDoorPosEastAddr = 0x19A2;

std::vector<int> DoorPositionManager::GetSnapPositions(
    DoorDirection direction) {
  // Return valid placement rows/columns based on the actual ROM tilemap tables.
  // Outer and inner sections share the same three positions along their moving axis.
  switch (direction) {
    case DoorDirection::North:
      return {14, 30, 46};
    case DoorDirection::South:
      return {14, 30, 46};
    case DoorDirection::West:
      return {11, 27, 43};
    case DoorDirection::East:
      return {11, 27, 43};
  }
  return {};
}

uint8_t DoorPositionManager::SnapToNearestPosition(int canvas_x, int canvas_y,
                                                   DoorDirection direction) {
  // First detect which section (outer wall vs inner seam) we're on
  DoorDirection detected_dir;
  bool is_inner = false;
  if (!DetectWallSection(canvas_x, canvas_y, detected_dir, is_inner)) {
    // Fallback: use outer wall positions
    is_inner = false;
  }

  // Get the starting position index for this section
  uint8_t start_pos = GetSectionStartPosition(direction, is_inner);

  // Convert canvas pixels to tile coordinates
  int tile_x = canvas_x / kTileSize;
  int tile_y = canvas_y / kTileSize;

  // Determine which coordinate to snap based on direction
  // For North/South walls, we snap the X position (horizontal placement)
  // For East/West walls, we snap the Y position (vertical placement)
  int coord =
      (direction == DoorDirection::North || direction == DoorDirection::South)
          ? tile_x
          : tile_y;

  // Get valid snap positions for this direction
  auto valid_positions = GetSnapPositions(direction);
  if (valid_positions.empty()) {
    return start_pos;  // Fallback
  }

  // Find the nearest valid X/Y position (index 0, 1, or 2)
  int nearest_idx = 0;
  int min_dist = std::abs(coord - valid_positions[0]);
  for (size_t i = 1; i < valid_positions.size(); ++i) {
    int dist = std::abs(coord - valid_positions[i]);
    if (dist < min_dist) {
      min_dist = dist;
      nearest_idx = static_cast<int>(i);
    }
  }

  // Return position index offset by section start
  // Positions 0,1,2 and 3,4,5 have same X coords but different Y for layer variation
  // For simplicity, we return the base position (0,1,2 offset by start_pos)
  return static_cast<uint8_t>(start_pos + nearest_idx);
}

std::pair<int, int> DoorPositionManager::PositionToTileCoords(
    uint8_t position, DoorDirection direction) {
  int pos_idx = position & 0x0F;
  if (pos_idx > 11) {
    pos_idx = 11;
  }

  const uint16_t offset = DoorTilemapOffsets(direction)[pos_idx];
  return {TilemapOffsetToTileX(offset), TilemapOffsetToTileY(offset)};
}

std::pair<int, int> DoorPositionManager::PositionToPixelCoords(
    uint8_t position, DoorDirection direction) {
  auto [tile_x, tile_y] = PositionToTileCoords(position, direction);
  return {tile_x * kTileSize, tile_y * kTileSize};
}

int DoorPositionManager::GetWallEdge(DoorDirection direction) {
  switch (direction) {
    case DoorDirection::North:
      return 0;
    case DoorDirection::South:
      return kRoomHeightTiles - 3;  // 3 tiles from bottom for door height
    case DoorDirection::West:
      return 0;
    case DoorDirection::East:
      return kRoomWidthTiles - 3;  // 3 tiles from right for door width
  }
  return 0;
}

bool DoorPositionManager::IsValidPosition(uint8_t position,
                                          DoorDirection direction) {
  // Position must fit in 5 bits
  if (position > 0x1F) {
    return false;
  }

  // Check that resulting tile position is within room bounds
  // and leaves room for door dimensions
  int tile = (position & 0x1F) * 2;
  auto dims = GetDoorDimensions(direction);

  // For horizontal doors (N/S), check X doesn't overflow
  // For vertical doors (E/W), check Y doesn't overflow
  if (direction == DoorDirection::North || direction == DoorDirection::South) {
    return tile + dims.width_tiles <= kRoomWidthTiles;
  } else {
    return tile + dims.height_tiles <= kRoomHeightTiles;
  }
}

bool DoorPositionManager::DetectWallFromPosition(int canvas_x, int canvas_y,
                                                 DoorDirection& out_direction) {
  // Convert to tile coordinates
  int tile_x = canvas_x / kTileSize;
  int tile_y = canvas_y / kTileSize;

  // Check each wall edge with threshold
  int threshold = kWallDetectionThreshold;

  // North wall (top edge)
  if (tile_y < threshold) {
    out_direction = DoorDirection::North;
    return true;
  }

  // South wall (bottom edge)
  if (tile_y >= kRoomHeightTiles - threshold) {
    out_direction = DoorDirection::South;
    return true;
  }

  // West wall (left edge)
  if (tile_x < threshold) {
    out_direction = DoorDirection::West;
    return true;
  }

  // East wall (right edge)
  if (tile_x >= kRoomWidthTiles - threshold) {
    out_direction = DoorDirection::East;
    return true;
  }

  return false;
}

bool DoorPositionManager::DetectWallSection(int canvas_x, int canvas_y,
                                            DoorDirection& out_direction,
                                            bool& out_is_inner) {
  // Convert to tile coordinates
  int tile_x = canvas_x / kTileSize;
  int tile_y = canvas_y / kTileSize;

  // Room is 64x64 tiles, divided into 4 quadrants at tile 32
  constexpr int kMiddleSeam = 32;
  constexpr int kSeamThreshold = 6;  // Detection range around seam
  int threshold = kWallDetectionThreshold;

  // Check outer walls first (edges of room)
  // North wall (top edge)
  if (tile_y < threshold) {
    out_direction = DoorDirection::North;
    out_is_inner = false;
    return true;
  }

  // South wall (bottom edge)
  if (tile_y >= kRoomHeightTiles - threshold) {
    out_direction = DoorDirection::South;
    out_is_inner = false;  // South outer wall = positions 6-11
    return true;
  }

  // West wall (left edge)
  if (tile_x < threshold) {
    out_direction = DoorDirection::West;
    out_is_inner = false;
    return true;
  }

  // East wall (right edge)
  if (tile_x >= kRoomWidthTiles - threshold) {
    out_direction = DoorDirection::East;
    out_is_inner = false;  // East outer wall = positions 6-11
    return true;
  }

  // Check inner seams (middle of room between quadrants)
  // Horizontal seam at Y=32 (between top and bottom quadrants)
  if (std::abs(tile_y - kMiddleSeam) < kSeamThreshold) {
    // Determine if North or South based on which side of seam
    if (tile_y < kMiddleSeam) {
      out_direction = DoorDirection::North;
    } else {
      out_direction = DoorDirection::South;
    }
    out_is_inner = true;
    return true;
  }

  // Vertical seam at X=32 (between left and right quadrants)
  if (std::abs(tile_x - kMiddleSeam) < kSeamThreshold) {
    // Determine if West or East based on which side of seam
    if (tile_x < kMiddleSeam) {
      out_direction = DoorDirection::West;
    } else {
      out_direction = DoorDirection::East;
    }
    out_is_inner = true;
    return true;
  }

  return false;
}

uint8_t DoorPositionManager::GetSectionStartPosition(DoorDirection direction,
                                                     bool is_inner) {
  // Position ranges per direction:
  // - North: Outer (0-5), Inner (6-11)
  // - South: Inner (0-5), Outer (6-11)  <- inverted!
  // - West:  Outer (0-5), Inner (6-11)
  // - East:  Inner (0-5), Outer (6-11)  <- inverted!
  switch (direction) {
    case DoorDirection::North:
    case DoorDirection::West:
      return is_inner ? 6 : 0;

    case DoorDirection::South:
    case DoorDirection::East:
      // South/East have inverted mapping
      return is_inner ? 0 : 6;
  }
  return 0;
}

std::pair<uint8_t, uint8_t> DoorPositionManager::EncodeDoorBytes(
    uint8_t position, DoorType type, DoorDirection direction) {
  // Byte 1: position in bits 4-7, direction in bits 0-1
  // This matches FromRomBytes decoding: position = (b1 >> 4) & 0x0F,
  //                                      direction = b1 & 0x03
  uint8_t byte1 =
      ((position & 0x0F) << 4) | (static_cast<uint8_t>(direction) & 0x03);

  // Byte 2: door type (full byte, values 0x00, 0x02, 0x04, etc.)
  uint8_t byte2 = static_cast<uint8_t>(type);

  return {byte1, byte2};
}

std::tuple<int, int, int, int> DoorPositionManager::GetDoorBounds(
    uint8_t position, DoorDirection direction) {
  auto [pixel_x, pixel_y] = PositionToPixelCoords(position, direction);
  auto dims = GetDoorDimensions(direction);

  return {pixel_x, pixel_y, dims.width_pixels(), dims.height_pixels()};
}

}  // namespace zelda3
}  // namespace yaze
