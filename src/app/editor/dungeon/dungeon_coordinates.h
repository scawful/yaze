#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_COORDINATES_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_COORDINATES_H_

#include <cstdint>
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
 * - Camera coordinates: Absolute world position (16-bit, used by sprites/tracks)
 *
 * Camera Coordinate System:
 * - Base offset: $1000 (4096) for dungeons
 * - Room grid: 16 columns x 16 rows (256 room slots, though not all used)
 * - Each room is 512x512 pixels (2 "screens" in each dimension)
 * - Camera X = base + (room_col * 512) + local_pixel_x
 * - Camera Y = base + (room_row * 512) + local_pixel_y
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

// Camera/World coordinate constants
constexpr uint16_t kCameraBaseOffset = 0x1000;  // Base offset for dungeon camera
constexpr int kDungeonGridWidth = 16;           // Rooms per row in dungeon grid

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

// ============================================================================
// Camera/World Coordinate System
// ============================================================================

/**
 * @brief Get the grid position (column, row) for a room ID
 * @param room_id Room ID (0-295)
 * @return Pair of (column, row) in dungeon grid
 */
inline std::pair<int, int> RoomIdToGridPosition(int room_id) {
  return {room_id % kDungeonGridWidth, room_id / kDungeonGridWidth};
}

/**
 * @brief Calculate absolute camera X coordinate from room and local position
 * 
 * This is the format used by sprites, minecart tracks, and other game entities
 * that need absolute world positioning.
 *
 * @param room_id Room ID (0-295)
 * @param local_pixel_x Local X position within room (0-511 pixels)
 * @return 16-bit camera X coordinate
 */
inline uint16_t CalculateCameraX(int room_id, int local_pixel_x) {
  auto [col, row] = RoomIdToGridPosition(room_id);
  return kCameraBaseOffset + (col * kRoomPixelWidth) + local_pixel_x;
}

/**
 * @brief Calculate absolute camera Y coordinate from room and local position
 *
 * @param room_id Room ID (0-295)
 * @param local_pixel_y Local Y position within room (0-511 pixels)
 * @return 16-bit camera Y coordinate
 */
inline uint16_t CalculateCameraY(int room_id, int local_pixel_y) {
  auto [col, row] = RoomIdToGridPosition(room_id);
  return kCameraBaseOffset + (row * kRoomPixelWidth) + local_pixel_y;
}

/**
 * @brief Calculate camera coordinates from room and tile position
 *
 * Convenience function that converts tile coordinates to camera coordinates.
 *
 * @param room_id Room ID (0-295)
 * @param tile_x Tile X position within room (0-63)
 * @param tile_y Tile Y position within room (0-63)
 * @return Pair of (camera_x, camera_y) 16-bit coordinates
 */
inline std::pair<uint16_t, uint16_t> TileToCameraCoords(int room_id, 
                                                         int tile_x, 
                                                         int tile_y) {
  int pixel_x = tile_x * kTileSize;
  int pixel_y = tile_y * kTileSize;
  return {CalculateCameraX(room_id, pixel_x), 
          CalculateCameraY(room_id, pixel_y)};
}

/**
 * @brief Convert camera coordinates back to room ID and local position
 *
 * @param camera_x 16-bit camera X coordinate
 * @param camera_y 16-bit camera Y coordinate
 * @return Tuple-like struct with room_id, local_pixel_x, local_pixel_y
 */
struct CameraToLocalResult {
  int room_id;
  int local_pixel_x;
  int local_pixel_y;
  int local_tile_x;
  int local_tile_y;
};

inline CameraToLocalResult CameraToLocalCoords(uint16_t camera_x, 
                                                uint16_t camera_y) {
  // Remove base offset
  int world_x = camera_x - kCameraBaseOffset;
  int world_y = camera_y - kCameraBaseOffset;
  
  // Calculate grid position
  int col = world_x / kRoomPixelWidth;
  int row = world_y / kRoomPixelHeight;
  
  // Calculate local position
  int local_x = world_x % kRoomPixelWidth;
  int local_y = world_y % kRoomPixelHeight;
  
  // Handle negative values (shouldn't happen normally)
  if (world_x < 0) { col = 0; local_x = 0; }
  if (world_y < 0) { row = 0; local_y = 0; }
  
  return {
    row * kDungeonGridWidth + col,  // room_id
    local_x,                         // local_pixel_x
    local_y,                         // local_pixel_y
    local_x / kTileSize,            // local_tile_x
    local_y / kTileSize             // local_tile_y
  };
}

/**
 * @brief Calculate sprite-format coordinates (16-pixel units)
 *
 * Sprites use a different coordinate format with 16-pixel granularity.
 *
 * @param room_id Room ID (0-295)
 * @param sprite_x Sprite X position (0-31)
 * @param sprite_y Sprite Y position (0-31)
 * @return Pair of (camera_x, camera_y) 16-bit coordinates
 */
inline std::pair<uint16_t, uint16_t> SpriteToCameraCoords(int room_id,
                                                           int sprite_x,
                                                           int sprite_y) {
  int pixel_x = sprite_x * kSpriteTileSize;
  int pixel_y = sprite_y * kSpriteTileSize;
  return {CalculateCameraX(room_id, pixel_x),
          CalculateCameraY(room_id, pixel_y)};
}

}  // namespace dungeon_coords
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_COORDINATES_H_
