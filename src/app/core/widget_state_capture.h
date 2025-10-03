#ifndef YAZE_CORE_WIDGET_STATE_CAPTURE_H
#define YAZE_CORE_WIDGET_STATE_CAPTURE_H

#include <string>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace core {

// Widget state snapshot for debugging test failures
struct WidgetState {
  std::string focused_window;
  std::string focused_widget;
  std::string hovered_widget;
  std::vector<std::string> visible_windows;
  std::vector<std::string> open_popups;
  int frame_count = 0;
  float frame_rate = 0.0f;
  
  // Navigation state
  ImGuiID nav_id = 0;
  bool nav_active = false;
  
  // Input state
  bool mouse_down[5] = {false};
  float mouse_pos_x = 0.0f;
  float mouse_pos_y = 0.0f;
  
  // Keyboard state
  bool ctrl_pressed = false;
  bool shift_pressed = false;
  bool alt_pressed = false;
};

// Capture current ImGui widget state for debugging
// Returns JSON-formatted string representing the widget hierarchy and state
std::string CaptureWidgetState();

// Serialize widget state to JSON format
std::string SerializeWidgetStateToJson(const WidgetState& state);

}  // namespace core
}  // namespace yaze

#endif  // YAZE_CORE_WIDGET_STATE_CAPTURE_H
