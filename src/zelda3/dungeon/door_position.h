#ifndef YAZE_ZELDA3_DUNGEON_DOOR_POSITION_H
#define YAZE_ZELDA3_DUNGEON_DOOR_POSITION_H

#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

#include "door_types.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Manages door position snapping and coordinate conversion
 *
 * Doors in ALTTP can only be placed at specific positions along room walls.
 * The position is encoded as a 5-bit value that maps to tile coordinates.
 *
 * Position Encoding:
 * - ROM stores position as a full byte, but only bits 0-4 are used
 * - Tile coordinate = (position & 0x1F) * 2
 * - This gives 32 possible positions at tiles 0, 2, 4, ..., 62
 *
 * Wall Edges:
 * - North: doors placed at y=0 (top row)
 * - South: doors placed at y=61 (3 tiles from bottom for door height)
 * - West: doors placed at x=0 (left column)
 * - East: doors placed at x=61 (3 tiles from right for door width)
 */
class DoorPositionManager {
 public:
  // Room dimensions in tiles (64x64 for standard rooms)
  static constexpr int kRoomWidthTiles = 64;
  static constexpr int kRoomHeightTiles = 64;
  static constexpr int kTileSize = 8;  // Pixels per tile
  static constexpr int kMaxDoorPositions = 32;  // 5-bit encoding = 32 positions

  // Wall detection thresholds (in tiles from edge)
  static constexpr int kWallDetectionThreshold = 8;

  /**
   * @brief Get all valid snap positions for a given direction
   * @param direction Which wall the door is on
   * @return Vector of valid tile positions (0, 2, 4, ..., 62)
   */
  static std::vector<int> GetSnapPositions(DoorDirection direction);

  /**
   * @brief Convert canvas coordinates to nearest valid door position
   *
   * This snaps the mouse position to the nearest valid door placement
   * position on the specified wall.
   *
   * @param canvas_x X coordinate on canvas (pixels)
   * @param canvas_y Y coordinate on canvas (pixels)
   * @param direction Which wall the door is on
   * @return Encoded position byte for ROM storage (0-31)
   */
  static uint8_t SnapToNearestPosition(int canvas_x, int canvas_y,
                                        DoorDirection direction);

  /**
   * @brief Convert encoded position to tile coordinates
   *
   * @param position Encoded position byte from ROM (0-31)
   * @param direction Door direction
   * @return Pair of (tile_x, tile_y) coordinates
   */
  static std::pair<int, int> PositionToTileCoords(uint8_t position,
                                                   DoorDirection direction);

  /**
   * @brief Convert encoded position to pixel coordinates
   *
   * @param position Encoded position byte from ROM (0-31)
   * @param direction Door direction
   * @return Pair of (pixel_x, pixel_y) coordinates
   */
  static std::pair<int, int> PositionToPixelCoords(uint8_t position,
                                                    DoorDirection direction);

  /**
   * @brief Get the wall edge coordinate for a direction
   *
   * Returns the fixed coordinate for the wall edge:
   * - North: y=0
   * - South: y=61 (accounting for door height)
   * - West: x=0
   * - East: x=61 (accounting for door width)
   *
   * @param direction Door direction
   * @return Tile coordinate of the wall edge
   */
  static int GetWallEdge(DoorDirection direction);

  /**
   * @brief Check if a position is valid for door placement
   *
   * @param position Encoded position (0-31)
   * @param direction Door direction
   * @return true if position is valid
   */
  static bool IsValidPosition(uint8_t position, DoorDirection direction);

  /**
   * @brief Detect which wall the cursor is near
   *
   * Based on cursor position relative to room edges, determines which
   * wall (if any) the user is trying to place a door on.
   *
   * @param canvas_x X coordinate on canvas (pixels)
   * @param canvas_y Y coordinate on canvas (pixels)
   * @param out_direction Output: detected direction (if near a wall)
   * @return true if near a wall edge
   */
  static bool DetectWallFromPosition(int canvas_x, int canvas_y,
                                      DoorDirection& out_direction);

  /**
   * @brief Detect wall with inner/outer section information
   *
   * Extended version that also detects middle seams and indicates
   * whether the position is on the outer wall or inner seam.
   *
   * Position ranges per direction:
   * - North: Outer (0-5), Inner (6-11)
   * - South: Inner (0-5), Outer (6-11)
   * - West:  Outer (0-5), Inner (6-11)
   * - East:  Inner (0-5), Outer (6-11)
   *
   * @param canvas_x X coordinate on canvas (pixels)
   * @param canvas_y Y coordinate on canvas (pixels)
   * @param out_direction Output: detected direction
   * @param out_is_inner Output: true if at inner seam, false if outer wall
   * @return true if near any wall or seam
   */
  static bool DetectWallSection(int canvas_x, int canvas_y,
                                 DoorDirection& out_direction,
                                 bool& out_is_inner);

  /**
   * @brief Get the starting position index for outer/inner section
   *
   * @param direction Door direction
   * @param is_inner Whether at inner seam (vs outer wall)
   * @return Starting position index (0 or 6)
   */
  static uint8_t GetSectionStartPosition(DoorDirection direction, bool is_inner);

  /**
   * @brief Encode door data for ROM storage
   *
   * @param position Encoded position (0-31)
   * @param type Door type
   * @param direction Door direction
   * @return Pair of (byte1, byte2) for ROM storage
   */
  static std::pair<uint8_t, uint8_t> EncodeDoorBytes(uint8_t position,
                                                      DoorType type,
                                                      DoorDirection direction);

  /**
   * @brief Get the bounding rectangle for a door
   *
   * @param position Encoded position
   * @param direction Door direction
   * @return Tuple of (x, y, width, height) in pixels
   */
  static std::tuple<int, int, int, int> GetDoorBounds(uint8_t position,
                                                       DoorDirection direction);
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DOOR_POSITION_H
