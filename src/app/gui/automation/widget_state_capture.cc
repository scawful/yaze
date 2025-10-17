#include "app/gui/automation/widget_state_capture.h"

#include "absl/strings/str_format.h"
#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
#include "imgui.h"
#include "imgui_internal.h"
#else
#include "imgui/imgui.h"
#endif
#include <string>

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace core {

#if !defined(YAZE_WITH_JSON)
namespace {

std::string EscapeJsonString(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);
  escaped.push_back('"');

  for (unsigned char c : value) {
    switch (c) {
      case '"':
        escaped.append("\\\"");
        break;
      case '\\':
        escaped.append("\\\\");
        break;
      case '\b':
        escaped.append("\\b");
        break;
      case '\f':
        escaped.append("\\f");
        break;
      case '\n':
        escaped.append("\\n");
        break;
      case '\r':
        escaped.append("\\r");
        break;
      case '\t':
        escaped.append("\\t");
        break;
      default:
        if (c <= 0x1F) {
          escaped.append(absl::StrFormat("\\\\u%04X", static_cast<int>(c)));
        } else {
          escaped.push_back(static_cast<char>(c));
        }
        break;
    }
  }

  escaped.push_back('"');
  return escaped;
}

const char* BoolToJson(bool value) { return value ? "true" : "false"; }

std::string FormatFloat(float value) {
  // Match typical JSON formatting without trailing zeros when possible.
  return absl::StrFormat("%.4f", value);
}

std::string FormatFloatCompact(float value) {
  std::string formatted = FormatFloat(value);

  // Trim trailing zeros while keeping at least one decimal place.
  if (formatted.find('.') != std::string::npos) {
    while (!formatted.empty() && formatted.back() == '0') {
      formatted.pop_back();
    }
    if (!formatted.empty() && formatted.back() == '.') {
      formatted.push_back('0');
    }
  }
  return formatted;
}

}  // namespace
#endif  // !defined(YAZE_WITH_JSON)

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
  return "{\"warning\": \"Widget state capture unavailable (UI test engine disabled)\"}";
#endif

  return SerializeWidgetStateToJson(state);
}

std::string SerializeWidgetStateToJson(const WidgetState& state) {
#if defined(YAZE_WITH_JSON)
  nlohmann::json j;

  j["frame_count"] = state.frame_count;
  j["frame_rate"] = state.frame_rate;
  j["focused_window"] = state.focused_window;
  j["focused_widget"] = state.focused_widget;
  j["hovered_widget"] = state.hovered_widget;
  j["visible_windows"] = state.visible_windows;
  j["open_popups"] = state.open_popups;
  j["navigation"] = {
      {"nav_id", absl::StrFormat("0x%08X", state.nav_id)},
      {"nav_active", state.nav_active}};

  nlohmann::json mouse_buttons;
  for (int i = 0; i < 5; ++i) {
    mouse_buttons.push_back(state.mouse_down[i]);
  }

  j["input"] = {
      {"mouse_buttons", mouse_buttons},
      {"mouse_pos", {state.mouse_pos_x, state.mouse_pos_y}},
      {"modifiers",
       {{"ctrl", state.ctrl_pressed},
        {"shift", state.shift_pressed},
        {"alt", state.alt_pressed}}}};

  return j.dump(2);
#else
  std::string json;
  json.reserve(512);

  json.append("{\n");
  json.append("  \"frame_count\": ");
  json.append(std::to_string(state.frame_count));
  json.append(",\n");

  json.append("  \"frame_rate\": ");
  json.append(FormatFloatCompact(state.frame_rate));
  json.append(",\n");

  json.append("  \"focused_window\": ");
  json.append(EscapeJsonString(state.focused_window));
  json.append(",\n");

  json.append("  \"focused_widget\": ");
  json.append(EscapeJsonString(state.focused_widget));
  json.append(",\n");

  json.append("  \"hovered_widget\": ");
  json.append(EscapeJsonString(state.hovered_widget));
  json.append(",\n");

  json.append("  \"visible_windows\": [");
  for (size_t i = 0; i < state.visible_windows.size(); ++i) {
    if (i > 0) {
      json.append(", ");
    }
    json.append(EscapeJsonString(state.visible_windows[i]));
  }
  json.append("],\n");

  json.append("  \"open_popups\": [");
  for (size_t i = 0; i < state.open_popups.size(); ++i) {
    if (i > 0) {
      json.append(", ");
    }
    json.append(EscapeJsonString(state.open_popups[i]));
  }
  json.append("],\n");

  json.append("  \"navigation\": {\n");
  json.append("    \"nav_id\": ");
  json.append(EscapeJsonString(absl::StrFormat("0x%08X", state.nav_id)));
  json.append(",\n");
  json.append("    \"nav_active\": ");
  json.append(BoolToJson(state.nav_active));
  json.append("\n  },\n");

  json.append("  \"input\": {\n");
  json.append("    \"mouse_buttons\": [");
  for (int i = 0; i < 5; ++i) {
    if (i > 0) {
      json.append(", ");
    }
    json.append(BoolToJson(state.mouse_down[i]));
  }
  json.append("],\n");

  json.append("    \"mouse_pos\": [");
  json.append(FormatFloatCompact(state.mouse_pos_x));
  json.append(", ");
  json.append(FormatFloatCompact(state.mouse_pos_y));
  json.append("],\n");

  json.append("    \"modifiers\": {\n");
  json.append("      \"ctrl\": ");
  json.append(BoolToJson(state.ctrl_pressed));
  json.append(",\n");
  json.append("      \"shift\": ");
  json.append(BoolToJson(state.shift_pressed));
  json.append(",\n");
  json.append("      \"alt\": ");
  json.append(BoolToJson(state.alt_pressed));
  json.append("\n    }\n");
  json.append("  }\n");
  json.append("}\n");

  return json;
#endif  // defined(YAZE_WITH_JSON)
}

}  // namespace core
}  // namespace yaze
