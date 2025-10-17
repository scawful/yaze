#ifndef YAZE_APP_GUI_CANVAS_CANVAS_AUTOMATION_API_H
#define YAZE_APP_GUI_CANVAS_CANVAS_AUTOMATION_API_H

#include <functional>
#include <tuple>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// Forward declaration
class Canvas;

/**
 * @brief Programmatic interface for controlling canvas operations.
 *
 * Enables z3ed CLI, AI agents, GUI automation, and remote control via gRPC.
 * All operations work with logical tile coordinates, independent of zoom/scroll.
 */
class CanvasAutomationAPI {
 public:
  /**
   * @brief Selection state returned by GetSelection().
   */
  struct SelectionState {
    bool has_selection = false;
    std::vector<ImVec2> selected_tiles;
    ImVec2 selection_start = {0, 0};
    ImVec2 selection_end = {0, 0};
  };

  /**
   * @brief Canvas dimensions in logical tile units.
   */
  struct Dimensions {
    int width_tiles = 0;
    int height_tiles = 0;
    int tile_size = 16;
  };

  /**
   * @brief Visible region in logical tile coordinates.
   */
  struct VisibleRegion {
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
  };

  explicit CanvasAutomationAPI(Canvas* canvas);

  // ============================================================================
  // Tile Operations
  // ============================================================================

  /**
   * @brief Paint a single tile at logical coordinates.
   * @param x Logical X coordinate (tile units)
   * @param y Logical Y coordinate (tile units)
   * @param tile_id Tile ID to paint
   * @return true if successful, false if out of bounds
   */
  bool SetTileAt(int x, int y, int tile_id);

  /**
   * @brief Query tile ID at logical coordinates.
   * @param x Logical X coordinate
   * @param y Logical Y coordinate
   * @return Tile ID at position, or -1 if out of bounds
   */
  int GetTileAt(int x, int y) const;

  /**
   * @brief Paint multiple tiles in a batch operation.
   * @param tiles Vector of (x, y, tile_id) tuples
   * @return true if all tiles painted successfully
   */
  bool SetTiles(const std::vector<std::tuple<int, int, int>>& tiles);

  // ============================================================================
  // Selection Operations
  // ============================================================================

  /**
   * @brief Select a single tile.
   * @param x Logical X coordinate
   * @param y Logical Y coordinate
   */
  void SelectTile(int x, int y);

  /**
   * @brief Select a rectangular region of tiles.
   * @param x1 Top-left X coordinate (logical)
   * @param y1 Top-left Y coordinate (logical)
   * @param x2 Bottom-right X coordinate (logical)
   * @param y2 Bottom-right Y coordinate (logical)
   */
  void SelectTileRect(int x1, int y1, int x2, int y2);

  /**
   * @brief Query current selection state.
   * @return Selection state with all selected tiles
   */
  SelectionState GetSelection() const;

  /**
   * @brief Clear current selection.
   */
  void ClearSelection();

  // ============================================================================
  // View Operations
  // ============================================================================

  /**
   * @brief Scroll canvas to make tile visible.
   * @param x Logical X coordinate
   * @param y Logical Y coordinate
   * @param center If true, center the tile; otherwise just ensure visibility
   */
  void ScrollToTile(int x, int y, bool center = false);

  /**
   * @brief Center canvas view on a specific tile.
   * @param x Logical X coordinate
   * @param y Logical Y coordinate
   */
  void CenterOn(int x, int y);

  /**
   * @brief Set canvas zoom level.
   * @param zoom Zoom factor (0.25 to 4.0, default 1.0)
   */
  void SetZoom(float zoom);

  /**
   * @brief Get current zoom level.
   * @return Current zoom factor
   */
  float GetZoom() const;

  // ============================================================================
  // Query Operations
  // ============================================================================

  /**
   * @brief Get canvas dimensions in logical tile units.
   * @return Canvas dimensions
   */
  Dimensions GetDimensions() const;

  /**
   * @brief Get currently visible tile region.
   * @return Visible region bounds in logical coordinates
   */
  VisibleRegion GetVisibleRegion() const;

  /**
   * @brief Check if a tile is currently visible.
   * @param x Logical X coordinate
   * @param y Logical Y coordinate
   * @return true if tile is visible on canvas
   */
  bool IsTileVisible(int x, int y) const;

  /**
   * @brief Check if coordinates are within canvas bounds.
   * @param x Logical X coordinate
   * @param y Logical Y coordinate
   * @return true if coordinates are valid
   */
  bool IsInBounds(int x, int y) const;

  // ============================================================================
  // Coordinate Conversion
  // ============================================================================

  /**
   * @brief Convert logical tile coordinates to canvas pixel coordinates.
   * @param x Logical X coordinate
   * @param y Logical Y coordinate
   * @return Canvas pixel position
   */
  ImVec2 TileToCanvas(int x, int y) const;

  /**
   * @brief Convert canvas pixel coordinates to logical tile coordinates.
   * @param canvas_pos Canvas pixel position
   * @return Logical tile position
   */
  ImVec2 CanvasToTile(ImVec2 canvas_pos) const;

  // ============================================================================
  // Callback Registration (for external integrations)
  // ============================================================================

  /**
   * @brief Set callback for tile painting operations.
   * Allows external systems (CLI, AI agents) to implement custom tile logic.
   */
  using TilePaintCallback = std::function<bool(int x, int y, int tile_id)>;
  void SetTilePaintCallback(TilePaintCallback callback);

  /**
   * @brief Set callback for tile querying operations.
   * Allows external systems to provide tile data.
   */
  using TileQueryCallback = std::function<int(int x, int y)>;
  void SetTileQueryCallback(TileQueryCallback callback);

 private:
  Canvas* canvas_;
  TilePaintCallback tile_paint_callback_;
  TileQueryCallback tile_query_callback_;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_AUTOMATION_API_H

