#include "canvas_geometry.h"

#include <algorithm>
#include "app/gui/canvas/canvas_utils.h"

namespace yaze {
namespace gui {

CanvasGeometry CalculateCanvasGeometry(
    const CanvasConfig& config,
    ImVec2 requested_size,
    ImVec2 cursor_screen_pos,
    ImVec2 content_region_avail) {
  
  CanvasGeometry geometry;
  
  // Set canvas top-left position (screen space)
  geometry.canvas_p0 = cursor_screen_pos;
  
  // Calculate canvas size using existing utility function
  ImVec2 canvas_sz = CanvasUtils::CalculateCanvasSize(
      content_region_avail, config.canvas_size, config.custom_canvas_size);
  
  // Override with explicit size if provided
  if (requested_size.x != 0) {
    canvas_sz = requested_size;
  }
  
  geometry.canvas_sz = canvas_sz;
  
  // Calculate scaled canvas bounds
  geometry.scaled_size = CanvasUtils::CalculateScaledCanvasSize(
      canvas_sz, config.global_scale);
  
  // CRITICAL: Ensure minimum size to prevent ImGui assertions
  if (geometry.scaled_size.x <= 0.0f) {
    geometry.scaled_size.x = 1.0f;
  }
  if (geometry.scaled_size.y <= 0.0f) {
    geometry.scaled_size.y = 1.0f;
  }
  
  // Calculate bottom-right position
  geometry.canvas_p1 = ImVec2(
      geometry.canvas_p0.x + geometry.scaled_size.x,
      geometry.canvas_p0.y + geometry.scaled_size.y);
  
  // Copy scroll offset from config (will be updated by interaction)
  geometry.scrolling = config.scrolling;
  
  return geometry;
}

ImVec2 CalculateMouseInCanvas(
    const CanvasGeometry& geometry,
    ImVec2 mouse_screen_pos) {
  
  // Calculate origin (locked scrolled origin as used throughout canvas.cc)
  ImVec2 origin = GetCanvasOrigin(geometry);
  
  // Convert screen space to canvas space
  return ImVec2(
      mouse_screen_pos.x - origin.x,
      mouse_screen_pos.y - origin.y);
}

bool IsPointInCanvasBounds(
    const CanvasGeometry& geometry,
    ImVec2 point) {
  
  return point.x >= geometry.canvas_p0.x && 
         point.x <= geometry.canvas_p1.x &&
         point.y >= geometry.canvas_p0.y && 
         point.y <= geometry.canvas_p1.y;
}

void ApplyScrollDelta(CanvasGeometry& geometry, ImVec2 delta) {
  geometry.scrolling.x += delta.x;
  geometry.scrolling.y += delta.y;
}

}  // namespace gui
}  // namespace yaze

