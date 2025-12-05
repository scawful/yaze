#include "door_position.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace yaze {
namespace zelda3 {

std::vector<int> DoorPositionManager::GetSnapPositions(
    DoorDirection direction) {
  std::vector<int> positions;
  positions.reserve(kMaxDoorPositions);

  // Doors use 5-bit position encoding: (pos & 0x1F) * 2 = tile offset
  // This gives positions at tiles 0, 2, 4, ..., 62
  for (int i = 0; i < kMaxDoorPositions; ++i) {
    positions.push_back(i * 2);
  }

  return positions;
}

uint8_t DoorPositionManager::SnapToNearestPosition(int canvas_x, int canvas_y,
                                                    DoorDirection direction) {
  // Convert canvas pixels to tile coordinates
  int tile_x = canvas_x / kTileSize;
  int tile_y = canvas_y / kTileSize;

  // Determine which coordinate to snap based on direction
  // For North/South walls, we snap the X position
  // For East/West walls, we snap the Y position
  int coord = (direction == DoorDirection::North ||
               direction == DoorDirection::South)
                  ? tile_x
                  : tile_y;

  // Clamp to valid range (0-62, leaving room for door dimensions)
  coord = std::clamp(coord, 0, 62);

  // Snap to nearest even number (door positions are at 0, 2, 4, ...)
  // Round to nearest rather than truncate
  int snapped = ((coord + 1) / 2) * 2;
  snapped = std::min(snapped, 62);  // Ensure we don't exceed max

  // Convert back to 5-bit encoded position
  return static_cast<uint8_t>(snapped / 2);
}

std::pair<int, int> DoorPositionManager::PositionToTileCoords(
    uint8_t position, DoorDirection direction) {
  // Door positions are indices (0-5) into lookup tables derived from
  // DoorTilemapPositions_* in the ASM.
  //
  // VRAM tilemap offsets converted to tile coords:
  //   tile_x = (offset % 0x80) / 2
  //   tile_y = offset / 0x80
  // 
  // North wall VRAM offsets ($021C, $023C, $025C, $039C, $03BC, $03DC):
  //   Positions 0-2: row 4, cols 14, 30, 46 (upper half)
  //   Positions 3-5: row 7, cols 14, 30, 46 (lower half - for large rooms)
  //
  // The tilemap has a 4-row offset, so subtract 4 from y to get room coords.
  // Similarly for x positions, subtract 2 for the left margin.

  // Clamp position index to valid range (0-5 for most walls)
  int pos_idx = position & 0x0F;
  if (pos_idx > 5) pos_idx = 5;

  // Door X positions for North/South walls (adjusted for room origin)
  // VRAM cols 14, 30, 46 → room cols 12, 28, 44
  static constexpr int kDoorXPositions[] = {12, 28, 44, 12, 28, 44};
  
  // Door Y positions for West/East walls (adjusted for room origin)
  // VRAM rows 15, 31, 47 → room rows 11, 27, 43
  static constexpr int kDoorYPositions[] = {11, 27, 43, 11, 27, 43};

  switch (direction) {
    case DoorDirection::North:
      // North wall: doors at top edge of room
      return {kDoorXPositions[pos_idx], 0};

    case DoorDirection::South:
      // South wall: doors at bottom edge (64 - 3 = 61)
      return {kDoorXPositions[pos_idx], kRoomHeightTiles - 3};

    case DoorDirection::West:
      // West wall: doors at left edge
      return {0, kDoorYPositions[pos_idx]};

    case DoorDirection::East:
      // East wall: doors at right edge (64 - 3 = 61)
      return {kRoomWidthTiles - 3, kDoorYPositions[pos_idx]};
  }

  return {0, 0};
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
  if (direction == DoorDirection::North ||
      direction == DoorDirection::South) {
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

std::pair<uint8_t, uint8_t> DoorPositionManager::EncodeDoorBytes(
    uint8_t position, DoorType type, DoorDirection direction) {
  // Byte 1: position value (full byte, only 5 bits used for coords)
  uint8_t byte1 = position;

  // Byte 2: (type << 4) | direction
  uint8_t byte2 = (static_cast<uint8_t>(type) << 4) |
                  (static_cast<uint8_t>(direction) & 0x0F);

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
