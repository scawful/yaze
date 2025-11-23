// SPDX-License-Identifier: MIT
// Theme definitions for yaze UI components.
// Centralized color palette and style constants to ensure visual consistency.

#ifndef YAZE_SRC_APP_GUI_STYLE_THEME_H_
#define YAZE_SRC_APP_GUI_STYLE_THEME_H_

#include "imgui.h"

namespace yaze::gui::style {

struct Theme {
  // Primary brand color (used for titles, highlights)
  ImVec4 primary = ImVec4(0.196f, 0.6f, 0.8f, 1.0f); // teal-ish
  // Secondary accent (buttons, active states)
  ImVec4 secondary = ImVec4(0.133f, 0.545f, 0.133f, 1.0f); // forest green
  // Warning / error color
  ImVec4 warning = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
  // Success color
  ImVec4 success = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
  // Background for panels
  ImVec4 panel_bg = ImVec4(0.07f, 0.07f, 0.07f, 0.95f);
  // Text color (default)
  ImVec4 text = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  // Rounded corner radius for windows and child panels
  float rounding = 6.0f;
};

// Returns the default theme used throughout the application.
inline const Theme& DefaultTheme() {
  static Theme theme;
  return theme;
}

// Apply the theme to ImGui style (call once per frame before drawing UI).
inline void ApplyTheme(const Theme& theme) {
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = theme.rounding;
  style.ChildRounding = theme.rounding;
  style.FrameRounding = theme.rounding;
  style.GrabRounding = theme.rounding;
  style.PopupRounding = theme.rounding;
  style.ScrollbarRounding = theme.rounding;

  // Colors – we keep most defaults, but override key ones.
  style.Colors[ImGuiCol_TitleBgActive] = theme.primary;
  style.Colors[ImGuiCol_Button] = theme.secondary;
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(theme.secondary.x * 1.2f,
                                               theme.secondary.y * 1.2f,
                                               theme.secondary.z * 1.2f, 1.0f);
  style.Colors[ImGuiCol_Text] = theme.text;
  style.Colors[ImGuiCol_ChildBg] = theme.panel_bg;
}

}  // namespace yaze::gui::style

#endif  // YAZE_SRC_APP_GUI_STYLE_THEME_H_
