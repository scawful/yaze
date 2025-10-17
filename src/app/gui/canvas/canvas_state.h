#ifndef YAZE_APP_GUI_CANVAS_CANVAS_STATE_H
#define YAZE_APP_GUI_CANVAS_CANVAS_STATE_H

#include <string>
#include "app/gui/canvas/canvas_utils.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Canvas geometry calculated per-frame
 * 
 * Represents the position, size, and scroll state of a canvas
 * in both screen space and scaled space. Used by rendering
 * functions to correctly position elements.
 */
struct CanvasGeometry {
  ImVec2 canvas_p0;      // Top-left screen position
  ImVec2 canvas_p1;      // Bottom-right screen position  
  ImVec2 canvas_sz;      // Actual canvas size (unscaled)
  ImVec2 scaled_size;    // Size after applying global_scale
  ImVec2 scrolling;      // Current scroll offset
  
  CanvasGeometry() 
      : canvas_p0(0, 0), canvas_p1(0, 0), canvas_sz(0, 0), 
        scaled_size(0, 0), scrolling(0, 0) {}
};

/**
 * @brief Complete canvas state snapshot
 * 
 * Aggregates all canvas state into a single POD for easier
 * refactoring and testing. Designed to replace the scattered
 * state members in the Canvas class gradually.
 * 
 * Usage Pattern:
 * - Geometry recalculated every frame in DrawBackground()
 * - Configuration updated via user interaction
 * - Selection state modified by interaction handlers
 * - Drawing state (points, labels) updated during rendering
 */
struct CanvasState {
  // Core identification
  std::string canvas_id = "Canvas";
  std::string context_id = "CanvasContext";
  
  // Configuration (reference existing CanvasConfig)
  CanvasConfig config;
  
  // Selection (reference existing CanvasSelection)
  CanvasSelection selection;
  
  // Geometry (calculated per-frame)
  CanvasGeometry geometry;
  
  // Interaction state
  ImVec2 mouse_pos_in_canvas = ImVec2(0, 0);
  ImVec2 drawn_tile_pos = ImVec2(-1, -1);
  bool is_hovered = false;
  
  // Drawing state
  ImVector<ImVec2> points;           // Hover preview points
  ImVector<ImVector<std::string>> labels;
  int current_labels = 0;
  int highlight_tile_id = -1;
  
  CanvasState() = default;
  
  // Convenience constructor with ID
  explicit CanvasState(const std::string& id) 
      : canvas_id(id), context_id(id + "Context") {}
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_STATE_H

