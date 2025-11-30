#include "app/editor/agent/agent_ui_theme.h"

#include <cstring>

namespace yaze {
namespace editor {

AgentUITheme AgentUITheme::FromCurrentTheme() {
  AgentUITheme t;
  const auto& theme = yaze::gui::ThemeManager::Get().GetCurrentTheme();

  // Message colors
  t.user_message_color = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
  t.agent_message_color = ImVec4(0.4f, 0.9f, 0.4f, 1.0f);
  t.system_message_color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

  t.text_secondary_color = theme.text_secondary;

  // Content colors
  t.json_text_color = ImVec4(0.9f, 0.7f, 0.4f, 1.0f);
  t.command_text_color = ImVec4(0.9f, 0.4f, 0.4f, 1.0f);
  t.code_bg_color = ImVec4(0.1f, 0.1f, 0.12f, 1.0f);

  // UI element colors
  t.panel_bg_color = theme.window_bg;
  t.panel_bg_darker = ImVec4(theme.window_bg.red * 0.8f, theme.window_bg.green * 0.8f,
                             theme.window_bg.blue * 0.8f, 1.0f);
  t.panel_border_color = theme.border;
  t.accent_color = theme.primary;

  // Status colors
  t.status_active = theme.success;
  t.status_inactive = theme.text_disabled;
  t.status_success = theme.success;
  t.status_warning = theme.warning;
  t.status_error = theme.error;

  // Provider colors
  t.provider_ollama = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
  t.provider_gemini = ImVec4(0.3f, 0.6f, 0.9f, 1.0f);
  t.provider_mock = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

  // Collaboration colors
  t.collaboration_active = theme.success;
  t.collaboration_inactive = theme.text_disabled;

  // Proposal colors
  t.proposal_panel_bg = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
  t.proposal_accent = ImVec4(0.4f, 0.6f, 0.9f, 1.0f);

  // Button colors
  t.button_copy = ImVec4(0.3f, 0.3f, 0.35f, 1.0f);
  t.button_copy_hover = ImVec4(0.4f, 0.4f, 0.45f, 1.0f);

  // Dungeon editor colors
  t.dungeon_selection_primary = ImVec4(1.0f, 0.9f, 0.2f, 0.6f);    // Yellow
  t.dungeon_selection_secondary = ImVec4(0.2f, 0.9f, 1.0f, 0.6f);  // Cyan
  t.dungeon_selection_pulsing = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);    // White pulse
  t.dungeon_selection_handle = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);     // White handle
  t.dungeon_drag_preview = ImVec4(0.5f, 0.5f, 1.0f, 0.4f);         // Blueish
  t.dungeon_drag_preview_outline = ImVec4(0.6f, 0.6f, 1.0f, 0.8f);
  t.dungeon_object_wall = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
  t.dungeon_object_floor = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
  t.dungeon_object_chest = ImVec4(1.0f, 0.84f, 0.0f, 1.0f);
  t.dungeon_object_door = ImVec4(0.55f, 0.27f, 0.07f, 1.0f);
  t.dungeon_object_pot = ImVec4(0.8f, 0.4f, 0.2f, 1.0f);
  t.dungeon_object_stairs = ImVec4(0.9f, 0.9f, 0.3f, 1.0f);
  t.dungeon_object_decoration = ImVec4(0.6f, 0.8f, 0.6f, 1.0f);
  t.dungeon_object_default = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
  t.dungeon_grid_cell_highlight = ImVec4(0.3f, 0.8f, 0.3f, 0.3f);
  t.dungeon_grid_cell_selected = ImVec4(0.2f, 0.7f, 0.2f, 0.5f);
  t.dungeon_grid_cell_border = ImVec4(0.4f, 0.4f, 0.4f, 0.5f);
  t.dungeon_grid_text = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
  t.dungeon_room_border = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
  t.dungeon_room_border_dark = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
  t.dungeon_sprite_layer0 = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
  t.dungeon_sprite_layer1 = ImVec4(0.3f, 0.3f, 0.8f, 1.0f);
  t.dungeon_sprite_layer2 = ImVec4(0.3f, 0.3f, 0.8f, 1.0f);
  t.dungeon_outline_layer0 = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
  t.dungeon_outline_layer1 = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
  t.dungeon_outline_layer2 = ImVec4(0.2f, 0.2f, 1.0f, 1.0f);
  t.text_primary = theme.text_primary;
  t.text_secondary_gray = theme.text_secondary;
  t.text_info = theme.primary;
  t.text_warning_yellow = theme.warning;
  t.text_error_red = theme.error;
  t.text_success_green = theme.success;
  t.box_bg_dark = theme.window_bg;
  t.box_border = theme.border;
  t.box_text = theme.text_primary;

  return t;
}

// Global theme instance
static AgentUITheme g_current_theme;

namespace AgentUI {

const AgentUITheme& GetTheme() {
  // Initialize if needed (lazy)
  if (g_current_theme.user_message_color.w == 0.0f) {
    g_current_theme = AgentUITheme::FromCurrentTheme();
  }
  return g_current_theme;
}

void RefreshTheme() {
  g_current_theme = AgentUITheme::FromCurrentTheme();
}

void PushPanelStyle() {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_color);
  ImGui::PushStyleColor(ImGuiCol_Border, theme.panel_border_color);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
}

void PopPanelStyle() {
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
}

void RenderSectionHeader(const char* icon, const char* label,
                         const ImVec4& color) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::Text("%s %s", icon, label);
  ImGui::PopStyleColor();
  ImGui::Separator();
}

void RenderStatusIndicator(const char* label, bool active) {
  const auto& theme = GetTheme();
  ImVec4 color = active ? theme.status_active : theme.status_inactive;
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::Bullet();
  ImGui::SameLine();
  ImGui::Text("%s", label);
  ImGui::PopStyleColor();
}

void RenderProviderBadge(const char* provider) {
  const auto& theme = GetTheme();
  ImVec4 color = theme.provider_mock;

  if (strcmp(provider, "ollama") == 0) {
    color = theme.provider_ollama;
  } else if (strcmp(provider, "gemini") == 0) {
    color = theme.provider_gemini;
  }

  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::Text("[%s]", provider);
  ImGui::PopStyleColor();
}

void StatusBadge(const char* text, ButtonColor color) {
  const auto& theme = GetTheme();
  ImVec4 bg_color;

  switch (color) {
    case ButtonColor::Success:
      bg_color = theme.status_success;
      break;
    case ButtonColor::Warning:
      bg_color = theme.status_warning;
      break;
    case ButtonColor::Error:
      bg_color = theme.status_error;
      break;
    case ButtonColor::Info:
      bg_color = theme.accent_color;
      break;
    case ButtonColor::Default:
    default:
      bg_color = theme.panel_bg_darker;
      break;
  }

  ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg_color);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg_color);
  ImGui::SmallButton(text);
  ImGui::PopStyleColor(3);
}

void VerticalSpacing(float amount) {
  ImGui::Dummy(ImVec2(0.0f, amount));
}

void HorizontalSpacing(float amount) {
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(amount, 0.0f));
  ImGui::SameLine();
}

bool StyledButton(const char* label, const ImVec4& color, const ImVec2& size) {
  ImGui::PushStyleColor(ImGuiCol_Button, color);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f,
                               color.w));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(color.x * 0.8f, color.y * 0.8f, color.z * 0.8f,
                               color.w));
  bool clicked = ImGui::Button(label, size);
  ImGui::PopStyleColor(3);
  return clicked;
}

bool IconButton(const char* icon, const char* tooltip) {
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  bool clicked = ImGui::Button(icon);
  ImGui::PopStyleColor();

  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  return clicked;
}

}  // namespace AgentUI

}  // namespace editor
}  // namespace yaze