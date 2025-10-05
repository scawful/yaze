#ifndef YAZE_APP_EDITOR_AGENT_AGENT_UI_THEME_H
#define YAZE_APP_EDITOR_AGENT_AGENT_UI_THEME_H

#include "imgui/imgui.h"
#include "app/gui/theme_manager.h"
#include "app/gui/color.h"

namespace yaze {
namespace editor {

/**
 * @struct AgentUITheme
 * @brief Centralized theme colors for Agent UI components
 * 
 * All hardcoded colors from AgentChatWidget, AgentEditor, and AgentChatHistoryPopup
 * are consolidated here and derived from the current theme.
 */
struct AgentUITheme {
  // Message colors
  ImVec4 user_message_color;
  ImVec4 agent_message_color;
  ImVec4 system_message_color;
  
  // Content colors
  ImVec4 json_text_color;
  ImVec4 command_text_color;
  ImVec4 code_bg_color;
  
  // UI element colors
  ImVec4 panel_bg_color;
  ImVec4 panel_bg_darker;
  ImVec4 panel_border_color;
  ImVec4 accent_color;
  
  // Status colors
  ImVec4 status_active;
  ImVec4 status_inactive;
  ImVec4 status_success;
  ImVec4 status_warning;
  ImVec4 status_error;
  
  // Provider colors
  ImVec4 provider_ollama;
  ImVec4 provider_gemini;
  ImVec4 provider_mock;
  
  // Collaboration colors
  ImVec4 collaboration_active;
  ImVec4 collaboration_inactive;
  
  // Proposal colors
  ImVec4 proposal_panel_bg;
  ImVec4 proposal_accent;
  
  // Button colors
  ImVec4 button_copy;
  ImVec4 button_copy_hover;
  
  // Gradient colors
  ImVec4 gradient_top;
  ImVec4 gradient_bottom;
  
  // Initialize from current theme
  static AgentUITheme FromCurrentTheme();
};

// Helper functions for common UI patterns
namespace AgentUI {

// Get current theme colors
const AgentUITheme& GetTheme();

// Refresh theme from ThemeManager
void RefreshTheme();

// Common UI components
void PushPanelStyle();
void PopPanelStyle();

void RenderSectionHeader(const char* icon, const char* label, const ImVec4& color);
void RenderStatusIndicator(const char* label, bool active);
void RenderProviderBadge(const char* provider);

// Spacing helpers
void VerticalSpacing(float amount = 8.0f);
void HorizontalSpacing(float amount = 8.0f);

// Common button styles
bool StyledButton(const char* label, const ImVec4& color, const ImVec2& size = ImVec2(0, 0));
bool IconButton(const char* icon, const char* tooltip = nullptr);

}  // namespace AgentUI

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_UI_THEME_H
