#ifndef YAZE_APP_GUI_CANVAS_COORDINATE_MAPPER_H
#define YAZE_APP_GUI_CANVAS_COORDINATE_MAPPER_H

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "app/gui/canvas/canvas_state.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// Forward declaration (defined in canvas.h)
struct CanvasRuntime;

/**
 * @brief Information about a tile at a specific screen location
 *
 * Contains all data needed to identify and locate a tile, including:
 * - Tile IDs (8x8 and 16x16)
 * - ROM offset for the tile data
 * - Palette information
 * - Position in canvas space
 */
struct TileHitInfo {
  int tile_id = -1;            ///< 8x8 tile ID (-1 if invalid)
  int tile16_index = -1;       ///< 16x16 tile index (-1 if invalid)
  uint32_t rom_offset = 0;     ///< ROM offset for tile data
  int palette_group = 0;       ///< Palette group index
  int palette_index = 0;       ///< Palette index within group
  ImVec2 tile_origin_px;       ///< Top-left of tile in canvas space (pixels)
  ImVec2 tile_size_px;         ///< Size of the tile in pixels
  bool is_valid = false;       ///< Whether the hit is valid

  // Extended information for debugging
  int map_x = -1;              ///< Tile X coordinate in map space
  int map_y = -1;              ///< Tile Y coordinate in map space
  int local_map_id = -1;       ///< Local map ID (for overworld)
  std::string canvas_id;       ///< ID of the canvas that was hit
};

/**
 * @brief Screen coordinate mapping result
 *
 * Contains the full transformation from screen space to tile space,
 * including all intermediate coordinates.
 */
struct ScreenToTileResult {
  // Input coordinates
  float screen_x = 0.0f;       ///< Original screen X coordinate
  float screen_y = 0.0f;       ///< Original screen Y coordinate

  // Canvas-relative coordinates
  float canvas_x = 0.0f;       ///< X position relative to canvas origin
  float canvas_y = 0.0f;       ///< Y position relative to canvas origin

  // Scaled coordinates (accounting for zoom)
  float scaled_x = 0.0f;       ///< X after applying inverse scale
  float scaled_y = 0.0f;       ///< Y after applying inverse scale

  // Scroll-adjusted coordinates
  float content_x = 0.0f;      ///< X in content space (with scroll)
  float content_y = 0.0f;      ///< Y in content space (with scroll)

  // Tile information
  TileHitInfo tile_info;       ///< Full tile information

  // Validity
  bool in_canvas_bounds = false;  ///< Whether point is within canvas
};

/**
 * @brief Configuration for coordinate mapping
 */
struct CoordinateMapperConfig {
  float tile_size = 16.0f;        ///< Size of tiles in pixels (8 or 16)
  int tiles_per_row = 32;         ///< Number of tiles per row in the map
  int tiles_per_col = 32;         ///< Number of tiles per column
  int local_map_size = 512;       ///< Size of local map in pixels (512 for OW)
  bool use_tile16 = true;         ///< Use 16x16 tile indexing
};

/**
 * @class CoordinateMapper
 * @brief Maps screen coordinates to tile/ROM data
 *
 * Provides comprehensive coordinate transformation for mapping user interactions
 * (mouse clicks, touches) to underlying tile data in the ROM. Handles all
 * necessary transformations including:
 *
 * - Screen space to canvas space
 * - Scroll offset compensation
 * - Zoom/scale adjustment
 * - Tile grid snapping
 * - ROM offset calculation
 *
 * Usage:
 * @code
 *   CoordinateMapper mapper;
 *   mapper.SetGeometry(canvas.geometry());
 *   mapper.SetConfig({.tile_size = 16.0f, .tiles_per_row = 32});
 *
 *   auto result = mapper.ScreenToTile(mouse_x, mouse_y);
 *   if (result.in_canvas_bounds && result.tile_info.is_valid) {
 *     int tile_id = result.tile_info.tile_id;
 *     uint32_t rom_offset = result.tile_info.rom_offset;
 *   }
 * @endcode
 */
class CoordinateMapper {
 public:
  CoordinateMapper() = default;
  ~CoordinateMapper() = default;

  // Configuration
  void SetConfig(const CoordinateMapperConfig& config) { config_ = config; }
  const CoordinateMapperConfig& GetConfig() const { return config_; }

  void SetGeometry(const CanvasGeometry& geometry) { geometry_ = geometry; }
  const CanvasGeometry& GetGeometry() const { return geometry_; }

  void SetScale(float scale) { scale_ = scale; }
  float GetScale() const { return scale_; }

  void SetCanvasId(const std::string& id) { canvas_id_ = id; }
  const std::string& GetCanvasId() const { return canvas_id_; }

  // --- Core Transformation Methods ---

  /**
   * @brief Convert screen coordinates to tile information
   * @param screen_x Screen X coordinate (from ImGui mouse position)
   * @param screen_y Screen Y coordinate (from ImGui mouse position)
   * @return Full transformation result with tile information
   */
  ScreenToTileResult ScreenToTile(float screen_x, float screen_y) const;

  /**
   * @brief Convert canvas-relative coordinates to tile information
   * @param canvas_x X position relative to canvas top-left
   * @param canvas_y Y position relative to canvas top-left
   * @return Tile hit information
   */
  TileHitInfo CanvasToTile(float canvas_x, float canvas_y) const;

  /**
   * @brief Convert tile coordinates to canvas position
   * @param tile_x Tile X coordinate
   * @param tile_y Tile Y coordinate
   * @return Canvas position of tile top-left corner
   */
  ImVec2 TileToCanvas(int tile_x, int tile_y) const;

  /**
   * @brief Convert tile coordinates to screen position
   * @param tile_x Tile X coordinate
   * @param tile_y Tile Y coordinate
   * @return Screen position of tile top-left corner
   */
  ImVec2 TileToScreen(int tile_x, int tile_y) const;

  /**
   * @brief Get tile ID from canvas position
   * @param canvas_x Canvas-relative X position
   * @param canvas_y Canvas-relative Y position
   * @return Tile ID or -1 if out of bounds
   */
  int GetTileId(float canvas_x, float canvas_y) const;

  /**
   * @brief Get tile16 index from canvas position
   * @param canvas_x Canvas-relative X position
   * @param canvas_y Canvas-relative Y position
   * @return Tile16 index or -1 if out of bounds
   */
  int GetTile16Index(float canvas_x, float canvas_y) const;

  // --- Bounds Checking ---

  /**
   * @brief Check if screen coordinates are within canvas bounds
   */
  bool IsInCanvasBounds(float screen_x, float screen_y) const;

  /**
   * @brief Check if tile coordinates are valid
   */
  bool IsValidTileCoord(int tile_x, int tile_y) const;

  // --- ROM Offset Calculation ---

  /**
   * @brief Calculate ROM offset for a given tile ID
   * @param tile_id The tile ID
   * @param base_offset Base ROM offset for tile data
   * @return ROM offset where tile data is stored
   *
   * Note: This is a simplified calculation. Actual ROM offsets depend on
   * the specific data type (overworld, dungeon, sprites, etc.) and may
   * need to be overridden or configured per-editor.
   */
  uint32_t CalculateRomOffset(int tile_id, uint32_t base_offset = 0) const;

  /**
   * @brief Callback type for custom ROM offset calculation
   */
  using RomOffsetCalculator = std::function<uint32_t(int tile_id)>;

  /**
   * @brief Set a custom ROM offset calculator
   */
  void SetRomOffsetCalculator(RomOffsetCalculator calculator) {
    rom_offset_calculator_ = std::move(calculator);
  }

  // --- Local Map Calculation (for Overworld) ---

  /**
   * @brief Calculate local map ID from content coordinates
   * @param content_x X coordinate in content space
   * @param content_y Y coordinate in content space
   * @return Local map ID (0-159 for light world, etc.)
   */
  int CalculateLocalMapId(float content_x, float content_y) const;

  // --- Batch Operations ---

  /**
   * @brief Convert multiple screen positions to tile info
   * @param positions Vector of (x, y) screen positions
   * @return Vector of tile hit results
   */
  std::vector<ScreenToTileResult> ScreenToTileBatch(
      const std::vector<std::pair<float, float>>& positions) const;

 private:
  // Apply scroll offset to get content position
  ImVec2 ApplyScrollOffset(float canvas_x, float canvas_y) const;

  // Apply inverse scale to get unscaled position
  ImVec2 ApplyInverseScale(float x, float y) const;

  // Snap position to tile grid
  std::pair<int, int> SnapToTileGrid(float x, float y) const;

  CoordinateMapperConfig config_;
  CanvasGeometry geometry_;
  float scale_ = 1.0f;
  std::string canvas_id_;
  RomOffsetCalculator rom_offset_calculator_;
};

// --- Free Functions for Common Operations ---

/**
 * @brief Quick screen-to-tile lookup using canvas runtime state
 */
TileHitInfo ScreenToTileQuick(const CanvasRuntime& runtime,
                               float screen_x, float screen_y,
                               float tile_size = 16.0f);

/**
 * @brief Calculate tile index from local position and grid settings
 */
int CalculateTileIndex(float local_x, float local_y,
                       float tile_size, int tiles_per_row);

/**
 * @brief Get tile bounds in canvas space
 */
ImVec4 GetTileBounds(int tile_x, int tile_y, float tile_size, float scale);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_COORDINATE_MAPPER_H
