#include "app/core/widget_state_capture.h"

#include "absl/strings/str_format.h"
#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
#include "imgui.h"
#include "imgui_internal.h"
#else
#include "imgui/imgui.h"
#endif
#include "nlohmann/json.hpp"

namespace yaze {
namespace core {

std::string CaptureWidgetState() {
  WidgetState state;
  
#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  // Check if ImGui context is available
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (!ctx) {
    return R"({"error": "ImGui context not available"})";
  }

  ImGuiIO& io = ImGui::GetIO();
  
  // Capture frame information
  state.frame_count = ImGui::GetFrameCount();
  state.frame_rate = io.Framerate;
  
  // Capture focused window
  ImGuiWindow* current = ImGui::GetCurrentWindow();
  if (current && !current->Hidden) {
    state.focused_window = current->Name;
  }
  
  // Capture active widget (focused for input)
  ImGuiID active_id = ImGui::GetActiveID();
  if (active_id != 0) {
    state.focused_widget = absl::StrFormat("0x%08X", active_id);
  }
  
  // Capture hovered widget
  ImGuiID hovered_id = ImGui::GetHoveredID();
  if (hovered_id != 0) {
    state.hovered_widget = absl::StrFormat("0x%08X", hovered_id);
  }
  
  // Traverse visible windows
  for (ImGuiWindow* window : ctx->Windows) {
    if (window && window->Active && !window->Hidden) {
      state.visible_windows.push_back(window->Name);
    }
  }
  
  // Capture open popups
  for (int i = 0; i < ctx->OpenPopupStack.Size; i++) {
    ImGuiPopupData& popup = ctx->OpenPopupStack[i];
    if (popup.Window && !popup.Window->Hidden) {
      state.open_popups.push_back(popup.Window->Name);
    }
  }
  
  // Capture navigation state
  state.nav_id = ctx->NavId;
  state.nav_active = ctx->NavWindow != nullptr;
  
  // Capture mouse state
  for (int i = 0; i < 5; i++) {
    state.mouse_down[i] = io.MouseDown[i];
  }
  state.mouse_pos_x = io.MousePos.x;
  state.mouse_pos_y = io.MousePos.y;
  
  // Capture keyboard modifiers
  state.ctrl_pressed = io.KeyCtrl;
  state.shift_pressed = io.KeyShift;
  state.alt_pressed = io.KeyAlt;
  
#else
  // When UI test engine / ImGui internals aren't available, provide a minimal
  // payload so downstream systems still receive structured JSON. This keeps
  // builds that exclude the UI test engine (e.g., Windows release) working.
  return R"({"warning": "Widget state capture unavailable (UI test engine disabled)"})";
#endif

  return SerializeWidgetStateToJson(state);
}

std::string SerializeWidgetStateToJson(const WidgetState& state) {
  nlohmann::json j;
  
  // Basic state
  j["frame_count"] = state.frame_count;
  j["frame_rate"] = state.frame_rate;
  
  // Window state
  j["focused_window"] = state.focused_window;
  j["focused_widget"] = state.focused_widget;
  j["hovered_widget"] = state.hovered_widget;
  j["visible_windows"] = state.visible_windows;
  j["open_popups"] = state.open_popups;
  
  // Navigation state
  j["navigation"] = {
    {"nav_id", absl::StrFormat("0x%08X", state.nav_id)},
    {"nav_active", state.nav_active}
  };
  
  // Input state
  nlohmann::json mouse_buttons;
  for (int i = 0; i < 5; i++) {
    mouse_buttons.push_back(state.mouse_down[i]);
  }
  j["input"] = {
    {"mouse_buttons", mouse_buttons},
    {"mouse_pos", {state.mouse_pos_x, state.mouse_pos_y}},
    {"modifiers", {
      {"ctrl", state.ctrl_pressed},
      {"shift", state.shift_pressed},
      {"alt", state.alt_pressed}
    }}
  };
  
  return j.dump(2);  // Pretty print with 2-space indent
}

}  // namespace core
}  // namespace yaze
