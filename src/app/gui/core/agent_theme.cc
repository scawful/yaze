#include "app/editor/agent/agent_ui_theme.h"

#include <cstring>

namespace yaze {
namespace editor {

AgentUITheme AgentUITheme::FromCurrentTheme() {
  AgentUITheme t;
  const auto& theme = yaze::gui::ThemeManager::Get().GetCurrentTheme();

  // Message colors - derived from theme.agent
  t.user_message_color = gui::ConvertColorToImVec4(theme.agent.user_message);
  t.agent_message_color = gui::ConvertColorToImVec4(theme.agent.agent_message);
  t.system_message_color = gui::ConvertColorToImVec4(theme.agent.system_message);

  t.text_secondary_color = gui::ConvertColorToImVec4(theme.agent.text_secondary);

  // Content colors - derived from theme.agent
  t.json_text_color = gui::ConvertColorToImVec4(theme.agent.json_text);
  t.command_text_color = gui::ConvertColorToImVec4(theme.agent.command_text);
  t.code_bg_color = gui::ConvertColorToImVec4(theme.agent.code_background);

  // UI element colors - derived from theme.agent
  t.panel_bg_color = gui::ConvertColorToImVec4(theme.agent.panel_bg);
  t.panel_bg_darker = gui::ConvertColorToImVec4(theme.agent.panel_bg_darker);
  t.panel_border_color = gui::ConvertColorToImVec4(theme.agent.panel_border);
  t.accent_color = gui::ConvertColorToImVec4(theme.agent.accent);

  // Status colors - derived from theme.agent
  t.status_active = gui::ConvertColorToImVec4(theme.agent.status_active);
  t.status_inactive = gui::ConvertColorToImVec4(theme.agent.status_inactive);
  t.status_success = gui::ConvertColorToImVec4(theme.agent.status_success);
  t.status_warning = gui::ConvertColorToImVec4(theme.agent.status_warning);
  t.status_error = gui::ConvertColorToImVec4(theme.agent.status_error);

  // Provider colors - derived from theme.agent
  t.provider_ollama = gui::ConvertColorToImVec4(theme.agent.provider_ollama);
  t.provider_gemini = gui::ConvertColorToImVec4(theme.agent.provider_gemini);
  t.provider_mock = gui::ConvertColorToImVec4(theme.agent.provider_mock);
  t.provider_openai = gui::ConvertColorToImVec4(theme.agent.provider_openai);

  // Collaboration colors
  t.collaboration_active = gui::ConvertColorToImVec4(theme.agent.collaboration_active);
  t.collaboration_inactive = gui::ConvertColorToImVec4(theme.agent.collaboration_inactive);

  // Proposal colors - derived from theme.agent
  t.proposal_panel_bg = gui::ConvertColorToImVec4(theme.agent.proposal_panel_bg);
  t.proposal_accent = gui::ConvertColorToImVec4(theme.agent.proposal_accent);

  // Button colors - derived from theme.agent
  t.button_copy = gui::ConvertColorToImVec4(theme.agent.button_copy);
  t.button_copy_hover = gui::ConvertColorToImVec4(theme.agent.button_copy_hover);

  // Gradient colors - derived from theme.agent
  t.gradient_top = gui::ConvertColorToImVec4(theme.agent.gradient_top);
  t.gradient_bottom = gui::ConvertColorToImVec4(theme.agent.gradient_bottom);

  // Unified editor colors
  t.editor_background = gui::ConvertColorToImVec4(theme.editor_background);
  t.editor_grid = gui::ConvertColorToImVec4(theme.editor_grid);
  t.editor_cursor = gui::ConvertColorToImVec4(theme.editor_cursor);
  t.editor_selection = gui::ConvertColorToImVec4(theme.editor_selection);

  // Interaction colors
  t.selection_primary = gui::ConvertColorToImVec4(theme.selection_primary);
  t.selection_secondary = gui::ConvertColorToImVec4(theme.selection_secondary);
  t.selection_hover = gui::ConvertColorToImVec4(theme.selection_hover);
  t.selection_pulsing = gui::ConvertColorToImVec4(theme.selection_pulsing);
  t.selection_handle = gui::ConvertColorToImVec4(theme.selection_handle);
  t.drag_preview = gui::ConvertColorToImVec4(theme.drag_preview);
  t.drag_preview_outline = gui::ConvertColorToImVec4(theme.drag_preview_outline);

  // Entity colors
  t.entrance_color = gui::ConvertColorToImVec4(theme.entrance_color);
  t.hole_color = gui::ConvertColorToImVec4(theme.hole_color);
  t.exit_color = gui::ConvertColorToImVec4(theme.exit_color);
  t.item_color = gui::ConvertColorToImVec4(theme.item_color);
  t.sprite_color = gui::ConvertColorToImVec4(theme.sprite_color);
  t.transport_color = gui::ConvertColorToImVec4(theme.transport_color);
  t.music_zone_color = gui::ConvertColorToImVec4(theme.music_zone_color);

  // Dungeon editor colors - derived from theme.dungeon
  t.dungeon_selection_primary = gui::ConvertColorToImVec4(theme.dungeon.selection_primary);
  t.dungeon_selection_secondary = gui::ConvertColorToImVec4(theme.dungeon.selection_secondary);
  t.dungeon_selection_pulsing = gui::ConvertColorToImVec4(theme.dungeon.selection_pulsing);
  t.dungeon_selection_handle = gui::ConvertColorToImVec4(theme.dungeon.selection_handle);
  t.dungeon_drag_preview = gui::ConvertColorToImVec4(theme.dungeon.drag_preview);
  t.dungeon_drag_preview_outline = gui::ConvertColorToImVec4(theme.dungeon.drag_preview_outline);
  t.dungeon_object_wall = gui::ConvertColorToImVec4(theme.dungeon.object_wall);
  t.dungeon_object_floor = gui::ConvertColorToImVec4(theme.dungeon.object_floor);
  t.dungeon_object_chest = gui::ConvertColorToImVec4(theme.dungeon.object_chest);
  t.dungeon_object_door = gui::ConvertColorToImVec4(theme.dungeon.object_door);
  t.dungeon_object_pot = gui::ConvertColorToImVec4(theme.dungeon.object_pot);
  t.dungeon_object_stairs = gui::ConvertColorToImVec4(theme.dungeon.object_stairs);
  t.dungeon_object_decoration = gui::ConvertColorToImVec4(theme.dungeon.object_decoration);
  t.dungeon_object_default = gui::ConvertColorToImVec4(theme.dungeon.object_default);
  t.dungeon_grid_cell_highlight = gui::ConvertColorToImVec4(theme.dungeon.grid_cell_highlight);
  t.dungeon_grid_cell_selected = gui::ConvertColorToImVec4(theme.dungeon.grid_cell_selected);
  t.dungeon_grid_cell_border = gui::ConvertColorToImVec4(theme.dungeon.grid_cell_border);
  t.dungeon_grid_text = gui::ConvertColorToImVec4(theme.dungeon.grid_text);
  t.dungeon_room_border = gui::ConvertColorToImVec4(theme.dungeon.room_border);
  t.dungeon_room_border_dark = gui::ConvertColorToImVec4(theme.dungeon.room_border_dark);
  t.dungeon_sprite_layer0 = gui::ConvertColorToImVec4(theme.dungeon.sprite_layer0);
  t.dungeon_sprite_layer1 = gui::ConvertColorToImVec4(theme.dungeon.sprite_layer1);
  t.dungeon_sprite_layer2 = gui::ConvertColorToImVec4(theme.dungeon.sprite_layer2);
  t.dungeon_outline_layer0 = gui::ConvertColorToImVec4(theme.dungeon.outline_layer0);
  t.dungeon_outline_layer1 = gui::ConvertColorToImVec4(theme.dungeon.outline_layer1);
  t.dungeon_outline_layer2 = gui::ConvertColorToImVec4(theme.dungeon.outline_layer2);

  // Text colors - derived from base theme
  t.text_primary = gui::ConvertColorToImVec4(theme.text_primary);
  t.text_secondary_gray = gui::ConvertColorToImVec4(theme.text_secondary);
  t.text_info = gui::ConvertColorToImVec4(theme.primary);
  t.text_warning_yellow = gui::ConvertColorToImVec4(theme.warning);
  t.text_error_red = gui::ConvertColorToImVec4(theme.error);
  t.text_success_green = gui::ConvertColorToImVec4(theme.success);

  // Box colors - derived from base theme
  t.box_bg_dark = gui::ConvertColorToImVec4(theme.window_bg);
  t.box_border = gui::ConvertColorToImVec4(theme.border);
  t.box_text = gui::ConvertColorToImVec4(theme.text_primary);

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
  } else if (strcmp(provider, "anthropic") == 0) {
    color = theme.provider_openai;
  } else if (strcmp(provider, "openai") == 0) {
    color = theme.provider_openai;
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
