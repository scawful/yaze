#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_COORDINATES_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_COORDINATES_H_

#include <utility>

namespace yaze {
namespace editor {

/**
 * @brief Coordinate conversion utilities for dungeon editing
 *
 * Dungeon coordinate systems:
 * - Room coordinates: Tile units (0-63 per axis, 8px per tile)
 * - Canvas coordinates: Unscaled pixels relative to canvas origin
 * - Screen coordinates: Scaled pixels relative to window
 *
 * All conversion functions work with UNSCALED canvas coordinates.
 * Canvas drawing functions apply scale internally.
 */
namespace dungeon_coords {

// Room constants
constexpr int kTileSize = 8;           // Dungeon tiles are 8x8 pixels
constexpr int kRoomTileWidth = 64;     // Room width in tiles
constexpr int kRoomTileHeight = 64;    // Room height in tiles
constexpr int kRoomPixelWidth = 512;   // 64 * 8
constexpr int kRoomPixelHeight = 512;  // 64 * 8
constexpr int kRoomCount = 0x128;      // 296 rooms total
constexpr int kEntranceCount = 0x8C;   // 140 entrances total

// Sprite coordinate system uses 16-pixel units
constexpr int kSpriteTileSize = 16;
constexpr int kSpriteGridMax = 31;  // 0-31 range for sprites

/**
 * @brief Convert room tile coordinates to canvas pixel coordinates
 * @param room_x Room X coordinate (0-63)
 * @param room_y Room Y coordinate (0-63)
 * @return Unscaled canvas pixel coordinates
 */
inline std::pair<int, int> RoomToCanvas(int room_x, int room_y) {
  return {room_x * kTileSize, room_y * kTileSize};
}

/**
 * @brief Convert canvas pixel coordinates to room tile coordinates
 * @param canvas_x Canvas X coordinate (unscaled pixels)
 * @param canvas_y Canvas Y coordinate (unscaled pixels)
 * @return Room tile coordinates (0-63)
 */
inline std::pair<int, int> CanvasToRoom(int canvas_x, int canvas_y) {
  return {canvas_x / kTileSize, canvas_y / kTileSize};
}

/**
 * @brief Convert screen coordinates to canvas coordinates (undo scale)
 * @param screen_x Screen X position (scaled)
 * @param screen_y Screen Y position (scaled)
 * @param scale Current canvas scale factor
 * @return Unscaled canvas coordinates
 */
inline std::pair<int, int> ScreenToCanvas(int screen_x, int screen_y,
                                          float scale) {
  if (scale <= 0.0f) scale = 1.0f;
  return {static_cast<int>(screen_x / scale),
          static_cast<int>(screen_y / scale)};
}

/**
 * @brief Convert canvas coordinates to screen coordinates (apply scale)
 * @param canvas_x Canvas X coordinate (unscaled)
 * @param canvas_y Canvas Y coordinate (unscaled)
 * @param scale Current canvas scale factor
 * @return Scaled screen coordinates
 */
inline std::pair<int, int> CanvasToScreen(int canvas_x, int canvas_y,
                                          float scale) {
  return {static_cast<int>(canvas_x * scale),
          static_cast<int>(canvas_y * scale)};
}

/**
 * @brief Check if coordinates are within room bounds
 * @param canvas_x Canvas X coordinate (unscaled pixels)
 * @param canvas_y Canvas Y coordinate (unscaled pixels)
 * @param margin Optional margin in pixels (default 0)
 */
inline bool IsWithinBounds(int canvas_x, int canvas_y, int margin = 0) {
  return canvas_x >= -margin && canvas_y >= -margin &&
         canvas_x < kRoomPixelWidth + margin &&
         canvas_y < kRoomPixelHeight + margin;
}

/**
 * @brief Clamp room tile coordinates to valid range
 * @param room_x Room X coordinate (will be clamped to 0-63)
 * @param room_y Room Y coordinate (will be clamped to 0-63)
 * @return Clamped room coordinates
 */
inline std::pair<int, int> ClampToRoom(int room_x, int room_y) {
  if (room_x < 0) room_x = 0;
  if (room_x >= kRoomTileWidth) room_x = kRoomTileWidth - 1;
  if (room_y < 0) room_y = 0;
  if (room_y >= kRoomTileHeight) room_y = kRoomTileHeight - 1;
  return {room_x, room_y};
}

/**
 * @brief Validate room ID is within valid range
 * @param room_id Room ID to validate
 * @return true if valid (0 to 295)
 */
inline bool IsValidRoomId(int room_id) {
  return room_id >= 0 && room_id < kRoomCount;
}

}  // namespace dungeon_coords
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_COORDINATES_H_
