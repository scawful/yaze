#ifndef YAZE_APP_GUI_CANVAS_CANVAS_INTERACTION_H
#define YAZE_APP_GUI_CANVAS_CANVAS_INTERACTION_H

#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gui/canvas/canvas_events.h"
#include "app/gui/canvas/canvas_state.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @file canvas_interaction.h
 * @brief Free functions for canvas interaction handling
 *
 * Phase 2 of Canvas refactoring: Extract interaction logic into event-driven
 * free functions. These functions replace the stateful CanvasInteractionHandler
 * methods with pure functions that return event payloads.
 *
 * Design Pattern:
 * - Input: Canvas geometry, mouse state, interaction parameters
 * - Output: Event payload struct (TilePaintEvent, RectSelectionEvent, etc.)
 * - No hidden state, fully testable
 * - Editors consume events and respond accordingly
 */

// ============================================================================
// Rectangle Selection (Phase 2.1)
// ============================================================================

/**
 * @brief Handle rectangle selection interaction
 *
 * Processes right-click drag to select multiple tiles in a rectangular region.
 * Returns event when selection completes.
 *
 * @param geometry Canvas geometry (position, size, scale)
 * @param current_map Current map ID for coordinate calculation
 * @param tile_size Logical tile size (before scaling)
 * @param draw_list ImGui draw list for preview rendering
 * @param mouse_button Mouse button for selection (default: right)
 * @return RectSelectionEvent with selection results
 */
RectSelectionEvent HandleRectangleSelection(
    const CanvasGeometry& geometry, int current_map, float tile_size,
    ImDrawList* draw_list,
    ImGuiMouseButton mouse_button = ImGuiMouseButton_Right);

/**
 * @brief Handle single tile selection (right-click)
 *
 * Processes single right-click to select one tile.
 *
 * @param geometry Canvas geometry
 * @param current_map Current map ID
 * @param tile_size Logical tile size
 * @param mouse_button Mouse button for selection
 * @return TileSelectionEvent with selected tile
 */
TileSelectionEvent HandleTileSelection(
    const CanvasGeometry& geometry, int current_map, float tile_size,
    ImGuiMouseButton mouse_button = ImGuiMouseButton_Right);

// ============================================================================
// Tile Painting (Phase 2.2)
// ============================================================================

/**
 * @brief Handle tile painting interaction
 *
 * Processes left-click/drag to paint tiles on tilemap.
 * Returns event when paint action occurs.
 *
 * @param geometry Canvas geometry
 * @param tile_id Current tile ID to paint
 * @param tile_size Logical tile size
 * @param mouse_button Mouse button for painting
 * @return TilePaintEvent with paint results
 */
TilePaintEvent HandleTilePaint(
    const CanvasGeometry& geometry, int tile_id, float tile_size,
    ImGuiMouseButton mouse_button = ImGuiMouseButton_Left);

/**
 * @brief Handle tile painter with bitmap preview
 *
 * Renders preview of tile at hover position and handles paint interaction.
 *
 * @param geometry Canvas geometry
 * @param bitmap Tile bitmap to paint
 * @param tile_size Logical tile size
 * @param draw_list ImGui draw list for preview
 * @param mouse_button Mouse button for painting
 * @return TilePaintEvent with paint results
 */
TilePaintEvent HandleTilePaintWithPreview(
    const CanvasGeometry& geometry, const gfx::Bitmap& bitmap, float tile_size,
    ImDrawList* draw_list,
    ImGuiMouseButton mouse_button = ImGuiMouseButton_Left);

/**
 * @brief Handle tilemap painting interaction
 *
 * Processes painting with tilemap data (multiple tiles).
 *
 * @param geometry Canvas geometry
 * @param tilemap Tilemap containing tile data
 * @param current_tile Current tile index in tilemap
 * @param draw_list ImGui draw list for preview
 * @param mouse_button Mouse button for painting
 * @return TilePaintEvent with paint results
 */
TilePaintEvent HandleTilemapPaint(
    const CanvasGeometry& geometry, const gfx::Tilemap& tilemap,
    int current_tile, ImDrawList* draw_list,
    ImGuiMouseButton mouse_button = ImGuiMouseButton_Left);

// ============================================================================
// Hover and Preview (Phase 2.3)
// ============================================================================

/**
 * @brief Update hover state for canvas
 *
 * Calculates hover position and grid-aligned preview position.
 *
 * @param geometry Canvas geometry
 * @param tile_size Logical tile size
 * @return HoverEvent with hover state
 */
HoverEvent HandleHover(const CanvasGeometry& geometry, float tile_size);

/**
 * @brief Render hover preview overlay
 *
 * Draws preview rectangle at hover position.
 *
 * @param geometry Canvas geometry
 * @param hover Hover event from HandleHover
 * @param tile_size Logical tile size
 * @param draw_list ImGui draw list
 * @param color Preview color (default: white with alpha)
 */
void RenderHoverPreview(const CanvasGeometry& geometry, const HoverEvent& hover,
                        float tile_size, ImDrawList* draw_list,
                        ImU32 color = IM_COL32(255, 255, 255, 80));

// ============================================================================
// Entity Interaction (Phase 2.4 - Future)
// ============================================================================

/**
 * @brief Handle entity interaction (hover, click, drag)
 *
 * Processes entity manipulation events.
 *
 * @param geometry Canvas geometry
 * @param entity_id Entity being interacted with
 * @param entity_position Current entity position
 * @return EntityInteractionEvent with interaction results
 */
EntityInteractionEvent HandleEntityInteraction(const CanvasGeometry& geometry,
                                               int entity_id,
                                               ImVec2 entity_position);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Align position to grid
 *
 * Snaps canvas position to nearest grid cell.
 *
 * @param pos Canvas position
 * @param grid_step Grid cell size
 * @return Grid-aligned position
 */
ImVec2 AlignToGrid(ImVec2 pos, float grid_step);

/**
 * @brief Get mouse position in canvas space
 *
 * Converts screen-space mouse position to canvas-space coordinates.
 *
 * @param geometry Canvas geometry (includes origin)
 * @return Mouse position in canvas space
 */
ImVec2 GetMouseInCanvasSpace(const CanvasGeometry& geometry);

/**
 * @brief Check if mouse is in canvas bounds
 *
 * @param geometry Canvas geometry
 * @return True if mouse is within canvas
 */
bool IsMouseInCanvas(const CanvasGeometry& geometry);

/**
 * @brief Calculate tile grid indices from canvas position
 *
 * @param canvas_pos Canvas-space position
 * @param tile_size Logical tile size
 * @param global_scale Canvas scale factor
 * @return Tile grid position (x, y)
 */
ImVec2 CanvasToTileGrid(ImVec2 canvas_pos, float tile_size, float global_scale);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_INTERACTION_H
