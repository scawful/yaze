#include "door_position.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace yaze {
namespace zelda3 {

// ROM addresses for door position tables (PC addresses, bank $00)
// These tables contain 6 VRAM tilemap offsets per direction
// Format: Each entry is a 16-bit VRAM offset
//
// SNES Address → PC Address:
//   $00:997E → 0x197E (DoorTilemapPositions_North)
//   $00:998A → 0x198A (DoorTilemapPositions_South)
//   $00:9996 → 0x1996 (DoorTilemapPositions_West)
//   $00:99A2 → 0x19A2 (DoorTilemapPositions_East)
//
// VRAM offset to tile coordinate conversion:
//   tile_x = (offset % 0x80) / 2
//   tile_y = offset / 0x80
//   room_x = tile_x - 2 (VRAM has 2-tile left margin)
//   room_y = tile_y - 4 (VRAM has 4-tile top margin)
//
// North wall VRAM offsets: $021C, $023C, $025C, $039C, $03BC, $03DC
//   → room X positions: 12, 28, 44, 12, 28, 44
// South wall offsets are similar, adjusted for bottom edge
// West wall VRAM offsets give Y positions: 11, 27, 43, 11, 27, 43
// East wall offsets are similar, adjusted for right edge
[[maybe_unused]] constexpr int kDoorPosNorthAddr = 0x197E;
[[maybe_unused]] constexpr int kDoorPosSouthAddr = 0x198A;
[[maybe_unused]] constexpr int kDoorPosWestAddr = 0x1996;
[[maybe_unused]] constexpr int kDoorPosEastAddr = 0x19A2;

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
  // Door positions are indices (0-5) into lookup tables from ROM.
  // These values are derived from DoorTilemapPositions_* tables at:
  //   North: $00:997E (PC 0x197E)
  //   South: $00:998A (PC 0x198A)
  //   West:  $00:9996 (PC 0x1996)
  //   East:  $00:99A2 (PC 0x19A2)
  //
  // VRAM tilemap offsets are converted to room tile coordinates:
  //   tile_x = (offset % 0x80) / 2 - 2  (subtract VRAM left margin)
  //   tile_y = (offset / 0x80) - 4      (subtract VRAM top margin)
  //
  // North wall positions (from ROM $00:997E):
  //   VRAM: $021C, $023C, $025C, $039C, $03BC, $03DC
  //   → room X: 12, 28, 44, 12, 28, 44 (Y fixed at wall edge)
  //   Positions 0-2 are for normal/small rooms
  //   Positions 3-5 are for large rooms (second row of doors)
  //
  // West/East wall positions similarly give Y coordinates:
  //   → room Y: 11, 27, 43, 11, 27, 43 (X fixed at wall edge)

  // Clamp position index to valid range (0-5 for most walls)
  int pos_idx = position & 0x0F;
  if (pos_idx > 5) pos_idx = 5;

  // Door X positions for North/South walls (derived from ROM)
  // VRAM cols 14, 30, 46 → room cols 12, 28, 44
  static constexpr int kDoorXPositions[] = {12, 28, 44, 12, 28, 44};

  // Door Y positions for West/East walls (derived from ROM)
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
