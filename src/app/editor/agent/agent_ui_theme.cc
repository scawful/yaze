#include "app/editor/agent/agent_ui_theme.h"

#include "app/gui/theme_manager.h"
#include "app/gui/color.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

// Global theme instance
static AgentUITheme g_agent_theme;
static bool g_theme_initialized = false;

AgentUITheme AgentUITheme::FromCurrentTheme() {
  AgentUITheme theme;
  const auto& current = gui::ThemeManager::Get().GetCurrentTheme();
  
  // Message colors - derived from theme primary/secondary
  theme.user_message_color = ImVec4(
    current.primary.red * 1.1f,
    current.primary.green * 0.95f,
    current.primary.blue * 0.6f,
    1.0f
  );
  
  theme.agent_message_color = ImVec4(
    current.secondary.red * 0.9f,
    current.secondary.green * 1.3f,
    current.secondary.blue * 1.0f,
    1.0f
  );
  
  theme.system_message_color = ImVec4(
    current.info.red,
    current.info.green,
    current.info.blue,
    1.0f
  );
  
  // Content colors
  theme.json_text_color = ImVec4(0.78f, 0.83f, 0.90f, 1.0f);
  theme.command_text_color = ImVec4(1.0f, 0.647f, 0.0f, 1.0f);
  theme.code_bg_color = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
  
  // UI element colors
  theme.panel_bg_color = ImVec4(0.12f, 0.14f, 0.18f, 0.95f);
  theme.panel_bg_darker = ImVec4(0.08f, 0.10f, 0.14f, 0.95f);
  theme.panel_border_color = ConvertColorToImVec4(current.border);
  theme.accent_color = ConvertColorToImVec4(current.accent);
  
  // Status colors
  theme.status_active = ConvertColorToImVec4(current.success);
  theme.status_inactive = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
  theme.status_success = ConvertColorToImVec4(current.success);
  theme.status_warning = ConvertColorToImVec4(current.warning);
  theme.status_error = ConvertColorToImVec4(current.error);
  
  // Provider-specific colors
  theme.provider_ollama = ImVec4(0.2f, 0.8f, 0.4f, 1.0f);      // Green
  theme.provider_gemini = ImVec4(0.196f, 0.6f, 0.8f, 1.0f);   // Blue
  theme.provider_mock = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);       // Gray
  
  // Collaboration colors
  theme.collaboration_active = ImVec4(0.133f, 0.545f, 0.133f, 1.0f);   // Forest green
  theme.collaboration_inactive = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
  
  // Proposal colors
  theme.proposal_panel_bg = ImVec4(0.20f, 0.35f, 0.20f, 0.35f);
  theme.proposal_accent = ImVec4(0.8f, 1.0f, 0.8f, 1.0f);
  
  // Button colors
  theme.button_copy = ImVec4(0.3f, 0.3f, 0.4f, 0.6f);
  theme.button_copy_hover = ImVec4(0.4f, 0.4f, 0.5f, 0.8f);
  
  // Gradient colors
  theme.gradient_top = ImVec4(0.18f, 0.22f, 0.28f, 1.0f);
  theme.gradient_bottom = ImVec4(0.12f, 0.16f, 0.22f, 1.0f);
  
  return theme;
}

namespace AgentUI {

const AgentUITheme& GetTheme() {
  if (!g_theme_initialized) {
    RefreshTheme();
  }
  return g_agent_theme;
}

void RefreshTheme() {
  g_agent_theme = AgentUITheme::FromCurrentTheme();
  g_theme_initialized = true;
}

void PushPanelStyle() {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_color);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
}

void PopPanelStyle() {
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
}

void RenderSectionHeader(const char* icon, const char* label, const ImVec4& color) {
  ImGui::TextColored(color, "%s %s", icon, label);
  ImGui::Separator();
}

void RenderStatusIndicator(const char* label, bool active) {
  const auto& theme = GetTheme();
  ImVec4 color = active ? theme.status_active : theme.status_inactive;
  
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 pos = ImGui::GetCursorScreenPos();
  float radius = 4.0f;
  
  pos.x += radius + 2;
  pos.y += ImGui::GetTextLineHeight() * 0.5f;
  
  draw_list->AddCircleFilled(pos, radius, ImGui::GetColorU32(color));
  
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + radius * 2 + 8);
  ImGui::Text("%s", label);
}

void RenderProviderBadge(const char* provider) {
  const auto& theme = GetTheme();
  
  ImVec4 badge_color;
  if (strcmp(provider, "ollama") == 0) {
    badge_color = theme.provider_ollama;
  } else if (strcmp(provider, "gemini") == 0) {
    badge_color = theme.provider_gemini;
  } else {
    badge_color = theme.provider_mock;
  }
  
  ImGui::PushStyleColor(ImGuiCol_Button, badge_color);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
  ImGui::SmallButton(provider);
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
}

void StatusBadge(const char* text, ButtonColor color) {
  const auto& theme = GetTheme();
  
  ImVec4 badge_color;
  switch (color) {
    case ButtonColor::Success: badge_color = theme.status_success; break;
    case ButtonColor::Warning: badge_color = theme.status_warning; break;
    case ButtonColor::Error: badge_color = theme.status_error; break;
    case ButtonColor::Info: badge_color = theme.accent_color; break;
    default: badge_color = theme.status_inactive; break;
  }
  
  ImGui::PushStyleColor(ImGuiCol_Button, badge_color);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
  ImGui::SmallButton(text);
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
}

void VerticalSpacing(float amount) {
  ImGui::Dummy(ImVec2(0, amount));
}

void HorizontalSpacing(float amount) {
  ImGui::Dummy(ImVec2(amount, 0));
  ImGui::SameLine();
}

bool StyledButton(const char* label, const ImVec4& color, const ImVec2& size) {
  ImGui::PushStyleColor(ImGuiCol_Button, color);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
    ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, color.w));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
    ImVec4(color.x * 0.8f, color.y * 0.8f, color.z * 0.8f, color.w));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
  
  bool result = ImGui::Button(label, size);
  
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(3);
  
  return result;
}

bool IconButton(const char* icon, const char* tooltip) {
  bool result = ImGui::SmallButton(icon);
  
  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  
  return result;
}

}  // namespace AgentUI

}  // namespace editor
}  // namespace yaze
