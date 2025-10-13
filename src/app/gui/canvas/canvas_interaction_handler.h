#ifndef YAZE_APP_GUI_CANVAS_CANVAS_INTERACTION_HANDLER_H
#define YAZE_APP_GUI_CANVAS_CANVAS_INTERACTION_HANDLER_H

#include <vector>
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {
namespace canvas {

/**
 * @brief Tile interaction mode for canvas
 */
enum class TileInteractionMode {
  kNone,              // No interaction
  kPaintSingle,       // Paint single tiles
  kPaintDrag,         // Paint while dragging
  kSelectSingle,      // Select single tile
  kSelectRectangle,   // Select rectangular region
  kColorPaint         // Paint with solid color
};

/**
 * @brief Result of a tile interaction operation
 */
struct TileInteractionResult {
  bool interaction_occurred = false;
  ImVec2 tile_position = ImVec2(-1, -1);
  std::vector<ImVec2> selected_tiles;
  int tile_id = -1;
  
  void Reset() {
    interaction_occurred = false;
    tile_position = ImVec2(-1, -1);
    selected_tiles.clear();
    tile_id = -1;
  }
};

/**
 * @brief Handles all tile-based interactions for Canvas
 * 
 * Consolidates tile painting, selection, and multi-selection logic
 * that was previously scattered across Canvas methods. Provides a
 * unified interface for common tile interaction patterns.
 * 
 * Key Features:
 * - Single tile painting with preview
 * - Drag painting for continuous tile placement
 * - Single tile selection
 * - Rectangle selection for multi-tile operations
 * - Color painting mode
 * - Grid-aligned positioning
 * - Hover preview
 */
class CanvasInteractionHandler {
 public:
  CanvasInteractionHandler() = default;
  
  /**
   * @brief Initialize the interaction handler
   */
  void Initialize(const std::string& canvas_id);
  
  /**
   * @brief Set the interaction mode
   */
  void SetMode(TileInteractionMode mode) { current_mode_ = mode; }
  TileInteractionMode GetMode() const { return current_mode_; }
  
  /**
   * @brief Update interaction state (call once per frame)
   * @param canvas_p0 Canvas top-left screen position
   * @param scrolling Canvas scroll offset
   * @param global_scale Canvas zoom scale
   * @param tile_size Logical tile size
   * @param canvas_size Canvas dimensions
   * @param is_hovered Whether mouse is over canvas
   * @return Interaction result for this frame
   */
  TileInteractionResult Update(ImVec2 canvas_p0, ImVec2 scrolling, 
                               float global_scale, float tile_size,
                               ImVec2 canvas_size, bool is_hovered);
  
  /**
   * @brief Draw tile painter (preview + interaction)
   * @param bitmap Tile bitmap to paint
   * @param draw_list ImGui draw list
   * @param canvas_p0 Canvas top-left position
   * @param scrolling Canvas scroll offset
   * @param global_scale Canvas zoom scale
   * @param tile_size Logical tile size
   * @param is_hovered Whether mouse is over canvas
   * @return True if tile was painted
   */
  bool DrawTilePainter(const gfx::Bitmap& bitmap, ImDrawList* draw_list,
                      ImVec2 canvas_p0, ImVec2 scrolling, float global_scale,
                      float tile_size, bool is_hovered);
  
  /**
   * @brief Draw tilemap painter (preview + interaction)
   */
  bool DrawTilemapPainter(gfx::Tilemap& tilemap, int current_tile, 
                         ImDrawList* draw_list, ImVec2 canvas_p0, 
                         ImVec2 scrolling, float global_scale, bool is_hovered);
  
  /**
   * @brief Draw solid color painter
   */
  bool DrawSolidTilePainter(const ImVec4& color, ImDrawList* draw_list,
                           ImVec2 canvas_p0, ImVec2 scrolling, 
                           float global_scale, float tile_size, bool is_hovered);
  
  /**
   * @brief Draw tile selector (single tile selection)
   */
  bool DrawTileSelector(ImDrawList* draw_list, ImVec2 canvas_p0, 
                       ImVec2 scrolling, float tile_size, bool is_hovered);
  
  /**
   * @brief Draw rectangle selector (multi-tile selection)
   * @param current_map Map ID for coordinate calculation
   * @param draw_list ImGui draw list
   * @param canvas_p0 Canvas position
   * @param scrolling Scroll offset
   * @param global_scale Zoom scale
   * @param tile_size Tile size
   * @param is_hovered Whether mouse is over canvas
   * @return True if selection was made
   */
  bool DrawSelectRect(int current_map, ImDrawList* draw_list, ImVec2 canvas_p0,
                     ImVec2 scrolling, float global_scale, float tile_size,
                     bool is_hovered);
  
  /**
   * @brief Get current hover points (for DrawOverlay)
   */
  const ImVector<ImVec2>& GetHoverPoints() const { return hover_points_; }
  
  /**
   * @brief Get selected points (for DrawOverlay)
   */
  const ImVector<ImVec2>& GetSelectedPoints() const { return selected_points_; }
  
  /**
   * @brief Get selected tiles from last rectangle selection
   */
  const std::vector<ImVec2>& GetSelectedTiles() const { return selected_tiles_; }
  
  /**
   * @brief Get last drawn tile position
   */
  ImVec2 GetDrawnTilePosition() const { return drawn_tile_pos_; }
  
  /**
   * @brief Get current mouse position in canvas space
   */
  ImVec2 GetMousePositionInCanvas() const { return mouse_pos_in_canvas_; }
  
  /**
   * @brief Clear all interaction state
   */
  void ClearState();
  
  /**
   * @brief Check if rectangle selection is active
   */
  bool IsRectSelectActive() const { return rect_select_active_; }
  
  /**
   * @brief Get selected tile position (for single selection)
   */
  ImVec2 GetSelectedTilePosition() const { return selected_tile_pos_; }
  
  /**
   * @brief Set selected tile position
   */
  void SetSelectedTilePosition(ImVec2 pos) { selected_tile_pos_ = pos; }

 private:
  std::string canvas_id_;
  TileInteractionMode current_mode_ = TileInteractionMode::kNone;
  
  // Interaction state
  ImVector<ImVec2> hover_points_;           // Current hover preview points
  ImVector<ImVec2> selected_points_;        // Selected rectangle points
  std::vector<ImVec2> selected_tiles_;      // Selected tiles from rect
  ImVec2 drawn_tile_pos_ = ImVec2(-1, -1);  // Last drawn tile position
  ImVec2 mouse_pos_in_canvas_ = ImVec2(0, 0); // Current mouse in canvas space
  ImVec2 selected_tile_pos_ = ImVec2(-1, -1); // Single tile selection
  bool rect_select_active_ = false;
  
  // Helper methods
  ImVec2 AlignPosToGrid(ImVec2 pos, float grid_step);
  ImVec2 GetMousePosition(ImVec2 canvas_p0, ImVec2 scrolling);
  bool IsMouseClicked(ImGuiMouseButton button);
  bool IsMouseDoubleClicked(ImGuiMouseButton button);
  bool IsMouseDragging(ImGuiMouseButton button);
  bool IsMouseReleased(ImGuiMouseButton button);
};

}  // namespace canvas
}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_INTERACTION_HANDLER_H
