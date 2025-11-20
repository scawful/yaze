#ifndef YAZE_APP_GUI_CANVAS_CANVAS_RENDERING_H
#define YAZE_APP_GUI_CANVAS_CANVAS_RENDERING_H

#include <string>
#include <vector>
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gui/canvas/canvas_state.h"
#include "app/gui/canvas/canvas_utils.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Render canvas background and border
 * 
 * Draws the canvas background rectangle (dark) and border (white)
 * at the calculated geometry positions. Extracted from Canvas::DrawBackground().
 * 
 * @param draw_list ImGui draw list for rendering
 * @param geometry Canvas geometry for this frame
 */
void RenderCanvasBackground(ImDrawList* draw_list,
                            const CanvasGeometry& geometry);

/**
 * @brief Render canvas grid with optional highlighting
 * 
 * Draws grid lines, hex labels, and optional tile highlighting.
 * Extracted from Canvas::DrawGrid().
 * 
 * @param draw_list ImGui draw list for rendering
 * @param geometry Canvas geometry
 * @param config Canvas configuration (grid settings)
 * @param highlight_tile_id Tile ID to highlight (-1 = no highlight)
 */
void RenderCanvasGrid(ImDrawList* draw_list, const CanvasGeometry& geometry,
                      const CanvasConfig& config, int highlight_tile_id = -1);

/**
 * @brief Render canvas overlay (hover and selection points)
 * 
 * Draws hover preview points and selection rectangle points.
 * Extracted from Canvas::DrawOverlay().
 * 
 * @param draw_list ImGui draw list for rendering
 * @param geometry Canvas geometry
 * @param config Canvas configuration (scale)
 * @param points Hover preview points
 * @param selected_points Selection rectangle points
 */
void RenderCanvasOverlay(ImDrawList* draw_list, const CanvasGeometry& geometry,
                         const CanvasConfig& config,
                         const ImVector<ImVec2>& points,
                         const ImVector<ImVec2>& selected_points);

/**
 * @brief Render canvas labels on grid
 * 
 * Draws custom text labels on canvas tiles.
 * Extracted from Canvas::DrawInfoGrid().
 * 
 * @param draw_list ImGui draw list for rendering
 * @param geometry Canvas geometry
 * @param config Canvas configuration
 * @param labels Label arrays (one per label set)
 * @param current_labels Active label set index
 * @param tile_id_offset Tile ID offset for calculation
 */
void RenderCanvasLabels(ImDrawList* draw_list, const CanvasGeometry& geometry,
                        const CanvasConfig& config,
                        const ImVector<ImVector<std::string>>& labels,
                        int current_labels, int tile_id_offset);

/**
 * @brief Render bitmap on canvas (border offset variant)
 * 
 * Draws a bitmap with a border offset from canvas origin.
 * Extracted from Canvas::DrawBitmap().
 * 
 * @param draw_list ImGui draw list
 * @param geometry Canvas geometry
 * @param bitmap Bitmap to render
 * @param border_offset Offset from canvas edges
 * @param scale Rendering scale
 */
void RenderBitmapOnCanvas(ImDrawList* draw_list, const CanvasGeometry& geometry,
                          gfx::Bitmap& bitmap, int border_offset, float scale);

/**
 * @brief Render bitmap on canvas (x/y offset variant)
 * 
 * Draws a bitmap at specified x/y offset with optional alpha.
 * Extracted from Canvas::DrawBitmap().
 * 
 * @param draw_list ImGui draw list
 * @param geometry Canvas geometry
 * @param bitmap Bitmap to render
 * @param x_offset X offset from canvas origin
 * @param y_offset Y offset from canvas origin
 * @param scale Rendering scale
 * @param alpha Alpha transparency (0-255)
 */
void RenderBitmapOnCanvas(ImDrawList* draw_list, const CanvasGeometry& geometry,
                          gfx::Bitmap& bitmap, int x_offset, int y_offset,
                          float scale, int alpha);

/**
 * @brief Render bitmap on canvas (custom source/dest regions)
 * 
 * Draws a bitmap with explicit source and destination rectangles.
 * Extracted from Canvas::DrawBitmap().
 * 
 * @param draw_list ImGui draw list
 * @param geometry Canvas geometry
 * @param bitmap Bitmap to render
 * @param dest_pos Destination position (canvas-relative)
 * @param dest_size Destination size
 * @param src_pos Source position in bitmap
 * @param src_size Source size in bitmap
 */
void RenderBitmapOnCanvas(ImDrawList* draw_list, const CanvasGeometry& geometry,
                          gfx::Bitmap& bitmap, ImVec2 dest_pos,
                          ImVec2 dest_size, ImVec2 src_pos, ImVec2 src_size);

/**
 * @brief Render group of bitmaps from tilemap
 * 
 * Draws multiple tiles for multi-tile selection preview.
 * Extracted from Canvas::DrawBitmapGroup().
 * 
 * @param draw_list ImGui draw list
 * @param geometry Canvas geometry
 * @param group Vector of tile IDs to draw
 * @param tilemap Tilemap containing the tiles
 * @param tile_size Size of each tile
 * @param scale Rendering scale
 * @param local_map_size Size of local map in pixels (default 512)
 * @param total_map_size Total map size for boundary clamping
 */
void RenderBitmapGroup(ImDrawList* draw_list, const CanvasGeometry& geometry,
                       std::vector<int>& group, gfx::Tilemap& tilemap,
                       int tile_size, float scale, int local_map_size,
                       ImVec2 total_map_size);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_RENDERING_H
