#ifndef YAZE_APP_GUI_CANVAS_CANVAS_GEOMETRY_H
#define YAZE_APP_GUI_CANVAS_CANVAS_GEOMETRY_H

#include "app/gui/canvas/canvas_state.h"
#include "app/gui/canvas/canvas_utils.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Calculate canvas geometry from configuration and ImGui context
 *
 * Extracts the geometry calculation logic from Canvas::DrawBackground().
 * Computes screen-space positions, sizes, and scroll offsets for a canvas
 * based on its configuration and the current ImGui layout state.
 *
 * @param config Canvas configuration (size, scale, custom size flag)
 * @param requested_size Explicitly requested canvas size (0,0 = use config)
 * @param cursor_screen_pos Current ImGui cursor position (from
 * GetCursorScreenPos)
 * @param content_region_avail Available content region (from
 * GetContentRegionAvail)
 * @return Calculated geometry for this frame
 */
CanvasGeometry CalculateCanvasGeometry(const CanvasConfig& config,
                                       ImVec2 requested_size,
                                       ImVec2 cursor_screen_pos,
                                       ImVec2 content_region_avail);

/**
 * @brief Calculate mouse position in canvas space
 *
 * Converts screen-space mouse coordinates to canvas-space coordinates,
 * accounting for canvas position and scroll offset. This is the correct
 * coordinate system for tile/entity placement calculations.
 *
 * @param geometry Canvas geometry (must be current frame)
 * @param mouse_screen_pos Mouse position in screen space
 * @return Mouse position in canvas space
 */
ImVec2 CalculateMouseInCanvas(const CanvasGeometry& geometry,
                              ImVec2 mouse_screen_pos);

/**
 * @brief Check if a point is within canvas bounds
 *
 * Tests whether a screen-space point lies within the canvas rectangle.
 * Useful for hit testing and hover detection.
 *
 * @param geometry Canvas geometry (must be current frame)
 * @param point Point in screen space to test
 * @return True if point is within canvas bounds
 */
bool IsPointInCanvasBounds(const CanvasGeometry& geometry, ImVec2 point);

/**
 * @brief Apply scroll delta to geometry
 *
 * Updates the scroll offset in the geometry. Used for pan operations.
 *
 * @param geometry Canvas geometry to update
 * @param delta Scroll delta (typically ImGui::GetIO().MouseDelta)
 */
void ApplyScrollDelta(CanvasGeometry& geometry, ImVec2 delta);

/**
 * @brief Get origin point (canvas top-left + scroll offset)
 *
 * Computes the "locked scrolled origin" used throughout canvas rendering.
 * This is the reference point for all canvas-space to screen-space conversions.
 *
 * @param geometry Canvas geometry
 * @return Origin point in screen space
 */
inline ImVec2 GetCanvasOrigin(const CanvasGeometry& geometry) {
  return ImVec2(geometry.canvas_p0.x + geometry.scrolling.x,
                geometry.canvas_p0.y + geometry.scrolling.y);
}

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_GEOMETRY_H
