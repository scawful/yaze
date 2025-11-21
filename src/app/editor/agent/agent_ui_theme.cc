#include "app/editor/agent/agent_ui_theme.h"

#include "app/gui/core/color.h"
#include "app/gui/core/theme_manager.h"
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
  theme.user_message_color =
      ImVec4(current.primary.red * 1.1f, current.primary.green * 0.95f,
             current.primary.blue * 0.6f, 1.0f);

  theme.agent_message_color =
      ImVec4(current.secondary.red * 0.9f, current.secondary.green * 1.3f,
             current.secondary.blue * 1.0f, 1.0f);

  theme.system_message_color =
      ImVec4(current.info.red, current.info.green, current.info.blue, 1.0f);

  // Content colors
  theme.json_text_color = ConvertColorToImVec4(current.text_secondary);
  theme.command_text_color = ConvertColorToImVec4(current.accent);
  theme.code_bg_color = ConvertColorToImVec4(current.code_background);

  theme.text_secondary_color = ConvertColorToImVec4(current.text_secondary);

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
  theme.provider_ollama = ImVec4(0.2f, 0.8f, 0.4f, 1.0f);    // Green
  theme.provider_gemini = ImVec4(0.196f, 0.6f, 0.8f, 1.0f);  // Blue
  theme.provider_mock = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);      // Gray

  // Collaboration colors
  theme.collaboration_active =
      ImVec4(0.133f, 0.545f, 0.133f, 1.0f);  // Forest green
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

  // Dungeon editor colors - high-contrast entity colors at 0.85f alpha
  theme.dungeon_selection_primary = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow
  theme.dungeon_selection_secondary = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);  // Cyan
  theme.dungeon_selection_pulsing = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);  // Cyan (animated)
  theme.dungeon_selection_handle = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);  // Cyan handles
  theme.dungeon_drag_preview = ImVec4(0.0f, 1.0f, 1.0f, 0.25f);  // Semi-transparent cyan
  theme.dungeon_drag_preview_outline = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);  // Cyan outline

  // Object type colors
  theme.dungeon_object_wall = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray
  theme.dungeon_object_floor = ImVec4(0.545f, 0.271f, 0.075f, 1.0f);  // Brown
  theme.dungeon_object_chest = ImVec4(1.0f, 0.843f, 0.0f, 1.0f);  // Gold
  theme.dungeon_object_door = ImVec4(0.545f, 0.271f, 0.075f, 1.0f);  // Brown
  theme.dungeon_object_pot = ImVec4(0.627f, 0.322f, 0.176f, 1.0f);  // Saddle brown
  theme.dungeon_object_stairs = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow (high-contrast)
  theme.dungeon_object_decoration = ImVec4(0.412f, 0.412f, 0.412f, 1.0f);  // Dim gray
  theme.dungeon_object_default = ImVec4(0.376f, 0.376f, 0.376f, 1.0f);  // Default gray

  // Grid colors
  theme.dungeon_grid_cell_highlight = ImVec4(0.565f, 0.933f, 0.565f, 1.0f);  // Light green
  theme.dungeon_grid_cell_selected = ImVec4(0.0f, 0.784f, 0.0f, 1.0f);  // Green
  theme.dungeon_grid_cell_border = ImVec4(0.314f, 0.314f, 0.314f, 0.784f);  // Gray border
  theme.dungeon_grid_text = ImVec4(0.863f, 0.863f, 0.863f, 1.0f);  // Light gray text

  // Room colors
  theme.dungeon_room_border = ImVec4(0.118f, 0.118f, 0.118f, 1.0f);  // Dark border
  theme.dungeon_room_border_dark = ImVec4(0.196f, 0.196f, 0.196f, 1.0f);  // Border

  // Sprite layer colors at 0.85f alpha for visibility
  theme.dungeon_sprite_layer0 = ImVec4(0.2f, 0.8f, 0.2f, 0.85f);  // Green
  theme.dungeon_sprite_layer1 = ImVec4(0.2f, 0.2f, 0.8f, 0.85f);  // Blue
  theme.dungeon_sprite_layer2 = ImVec4(0.2f, 0.2f, 0.8f, 0.85f);  // Blue

  // Outline layer colors at 0.5f alpha
  theme.dungeon_outline_layer0 = ImVec4(1.0f, 0.0f, 0.0f, 0.5f);  // Red
  theme.dungeon_outline_layer1 = ImVec4(0.0f, 1.0f, 0.0f, 0.5f);  // Green
  theme.dungeon_outline_layer2 = ImVec4(0.0f, 0.0f, 1.0f, 0.5f);  // Blue

  // Text colors
  theme.text_primary = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White
  theme.text_secondary_gray = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray
  theme.text_info = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);  // Info blue
  theme.text_warning_yellow = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);  // Warning yellow
  theme.text_error_red = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Error red
  theme.text_success_green = ImVec4(0.4f, 0.8f, 0.4f, 1.0f);  // Success green

  // Box colors
  theme.box_bg_dark = ImVec4(0.157f, 0.157f, 0.176f, 1.0f);  // Dark background
  theme.box_border = ImVec4(0.392f, 0.392f, 0.392f, 1.0f);  // Border gray
  theme.box_text = ImVec4(0.706f, 0.706f, 0.706f, 1.0f);  // Text gray

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

void RenderSectionHeader(const char* icon, const char* label,
                         const ImVec4& color) {
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
    case ButtonColor::Success:
      badge_color = theme.status_success;
      break;
    case ButtonColor::Warning:
      badge_color = theme.status_warning;
      break;
    case ButtonColor::Error:
      badge_color = theme.status_error;
      break;
    case ButtonColor::Info:
      badge_color = theme.accent_color;
      break;
    default:
      badge_color = theme.status_inactive;
      break;
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
  ImGui::PushStyleColor(
      ImGuiCol_ButtonHovered,
      ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, color.w));
  ImGui::PushStyleColor(
      ImGuiCol_ButtonActive,
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
