#include "door_position.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace yaze {
namespace zelda3 {

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
  // Return valid snap positions based on the actual ROM lookup tables
  // These are the coordinates where doors can be placed on each wall
  switch (direction) {
    case DoorDirection::North:
      // Valid X positions for north doors (left/center/right on wall)
      return {14, 30, 46};

    case DoorDirection::South:
      // Valid X positions for south doors (left/center/right on wall)
      return {14, 30, 46};

    case DoorDirection::West:
      // Valid Y positions for west doors (top/middle/bottom on wall)
      return {15, 31, 47};

    case DoorDirection::East:
      // Valid Y positions for east doors (top/middle/bottom on wall)
      return {15, 31, 47};
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
  int coord = (direction == DoorDirection::North ||
               direction == DoorDirection::South)
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
  // Door positions are indices (0-11+) into lookup tables from ROM.
  // Tables are laid out consecutively so position 6+ reads into the next table.
  //
  // Room layout: 64x64 tiles (512x512 pixels), divided into 4 quadrants
  // Each quadrant is 32x32 tiles. Doors can be on:
  //   - Outer walls (positions 0-5): edges of the room
  //   - Middle seams (positions 6-11): between quadrants
  //
  // Table layout in ROM (consecutive in memory):
  //   North: NorthWall (6) + NorthMiddle (6) = 12 positions
  //   South: SouthMiddle (9) + LowerLayerEntrance (3) = 12 positions
  //   West:  WestWall (6) + WestMiddle (6) = 12 positions
  //   East:  EastMiddle (6) + EastWall (6) = 12 positions
  //
  // VRAM offset to room tile conversion:
  //   room_x = (offset % 0x80) / 2 - 2  (subtract VRAM left margin)
  //   room_y = (offset / 0x80) - 4      (subtract VRAM top margin)

  int pos_idx = position & 0x0F;

  // Extended tables with 12 positions each (wall + middle seam)
  // X positions: 12, 28, 44 divide the 64-tile room into thirds
  // Y positions for N/S: edge vs middle seam
  // X positions for E/W: edge vs middle seam

  // North door positions (NorthWall + NorthMiddle):
  // Positions 0-5: Outer north wall (Y=0 for upper layer, Y=3 for lower layer)
  // Positions 6-11: Middle horizontal seam (Y=32 for upper, Y=35 for lower)
  // X positions: 14, 30, 46 (left/center/right on wall)
  // Note: +2 offset from raw VRAM calculation to correct for editor bitmap alignment
  static constexpr int kNorthDoorX[] = {14, 30, 46, 14, 30, 46,
                                        14, 30, 46, 14, 30, 46};
  // Y positions: +4 offset from raw VRAM to account for top margin in editor bitmap
  static constexpr int kNorthDoorY[] = {4, 4, 4, 7, 7, 7,
                                        36, 36, 36, 39, 39, 39};

  // South door positions (SouthMiddle + LowerLayerEntrance):
  // In ALTTP, south doors use SouthMiddle table which gives positions
  // relative to the current quadrant. For the 64-tile room:
  // Positions 0-2: Upper quadrant south doors (Y=22 from quadrant top)
  // Positions 3-5: Same, slightly higher (Y=19)
  // Positions 6-8: Lower quadrant south doors (Y=54 in full room)
  // Positions 9-11: Lower layer entrance (Y=51)
  // For most single-screen rooms, positions 0-5 map to the actual south wall
  // which should be at Y ≈ 60 in our 64-tile bitmap.
  // X positions: 14, 30, 46 (+2 offset from raw VRAM for editor alignment)
  static constexpr int kSouthDoorX[] = {14, 30, 46, 14, 30, 46,
                                        14, 30, 46, 14, 30, 46};
  // Y positions: +4 offset from raw VRAM, clamped to 61 max (room is 64 tiles, door is 3 tiles)
  // Pos 0-2: Quadrant seam (Y=26), Pos 3-5: Slightly higher (Y=23)
  // Pos 6-8: Outer south wall (Y=58), Pos 9-11: Slightly higher (Y=55)
  static constexpr int kSouthDoorY[] = {26, 26, 26, 23, 23, 23,
                                        58, 58, 58, 55, 55, 55};

  // West door positions (WestWall + WestMiddle):
  // Positions 0-5: Outer west wall (X=2 for upper, X=5 for lower layer)
  // Positions 6-11: Middle vertical seam (X=34 for upper, X=37 for lower)
  // Y positions: 15, 31, 47 (+4 offset from raw VRAM)
  static constexpr int kWestDoorX[] = {2, 2, 2, 5, 5, 5,
                                       34, 34, 34, 37, 37, 37};
  static constexpr int kWestDoorY[] = {15, 31, 47, 15, 31, 47,
                                       15, 31, 47, 15, 31, 47};

  // East door positions (EastMiddle + EastWall):
  // Note: East uses EastMiddle FIRST, then EastWall!
  // Positions 0-5: Middle vertical seam (X=26 for upper, X=23 for lower)
  // Positions 6-11: Outer east wall (X=58 for upper, X=55 for lower)
  // Y positions: 15, 31, 47 (+4 offset from raw VRAM)
  static constexpr int kEastDoorX[] = {26, 26, 26, 23, 23, 23,
                                       58, 58, 58, 55, 55, 55};
  static constexpr int kEastDoorY[] = {15, 31, 47, 15, 31, 47,
                                       15, 31, 47, 15, 31, 47};

  // Clamp to valid range (0-11)
  if (pos_idx > 11) pos_idx = 11;

  switch (direction) {
    case DoorDirection::North:
      return {kNorthDoorX[pos_idx], kNorthDoorY[pos_idx]};

    case DoorDirection::South:
      return {kSouthDoorX[pos_idx], kSouthDoorY[pos_idx]};

    case DoorDirection::West:
      return {kWestDoorX[pos_idx], kWestDoorY[pos_idx]};

    case DoorDirection::East:
      return {kEastDoorX[pos_idx], kEastDoorY[pos_idx]};
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
  uint8_t byte1 = ((position & 0x0F) << 4) |
                  (static_cast<uint8_t>(direction) & 0x03);

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
