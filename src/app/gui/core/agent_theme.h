#ifndef YAZE_APP_EDITOR_AGENT_AGENT_UI_THEME_H
#define YAZE_APP_EDITOR_AGENT_AGENT_UI_THEME_H

#include "app/gui/core/color.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @struct AgentUITheme
 * @brief Centralized theme colors for Agent UI components
 *
 * All hardcoded colors from AgentChatWidget, AgentEditor, and
 * AgentChatHistoryPopup are consolidated here and derived from the current
 * theme.
 */
struct AgentUITheme {
  // Message colors
  ImVec4 user_message_color;
  ImVec4 agent_message_color;
  ImVec4 system_message_color;

  ImVec4 text_secondary_color;

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
  ImVec4 provider_openai;

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

  // Unified editor colors
  ImVec4 editor_background;
  ImVec4 editor_grid;
  ImVec4 editor_cursor;
  ImVec4 editor_selection;

  // Interaction colors
  ImVec4 selection_primary;
  ImVec4 selection_secondary;
  ImVec4 selection_hover;
  ImVec4 selection_pulsing;
  ImVec4 selection_handle;
  ImVec4 drag_preview;
  ImVec4 drag_preview_outline;

  // Entity colors
  ImVec4 entrance_color;
  ImVec4 hole_color;
  ImVec4 exit_color;
  ImVec4 item_color;
  ImVec4 sprite_color;
  ImVec4 transport_color;
  ImVec4 music_zone_color;

  // Dungeon editor colors
  ImVec4 dungeon_selection_primary;        // Primary selection (yellow)
  ImVec4 dungeon_selection_secondary;      // Secondary selection (cyan)
  ImVec4 dungeon_selection_pulsing;        // Animated pulsing selection
  ImVec4 dungeon_selection_handle;         // Selection corner handles
  ImVec4 dungeon_drag_preview;             // Semi-transparent drag preview
  ImVec4 dungeon_drag_preview_outline;     // Drag preview outline
  ImVec4 dungeon_object_wall;              // Wall objects
  ImVec4 dungeon_object_floor;             // Floor objects
  ImVec4 dungeon_object_chest;             // Chest objects (gold)
  ImVec4 dungeon_object_door;              // Door objects
  ImVec4 dungeon_object_pot;               // Pot objects
  ImVec4 dungeon_object_stairs;            // Stairs (yellow)
  ImVec4 dungeon_object_decoration;        // Decoration objects
  ImVec4 dungeon_object_default;           // Default object color
  ImVec4 dungeon_grid_cell_highlight;      // Grid cell highlight (light green)
  ImVec4 dungeon_grid_cell_selected;       // Grid cell selected (green)
  ImVec4 dungeon_grid_cell_border;         // Grid cell border
  ImVec4 dungeon_grid_text;                // Grid text overlay
  ImVec4 dungeon_room_border;              // Room boundary
  ImVec4 dungeon_room_border_dark;         // Darker room border
  ImVec4 dungeon_sprite_layer0;            // Sprite layer 0 (green)
  ImVec4 dungeon_sprite_layer1;            // Sprite layer 1 (blue)
  ImVec4 dungeon_sprite_layer2;            // Sprite layer 2 (blue)
  ImVec4 dungeon_outline_layer0;           // Outline layer 0 (red)
  ImVec4 dungeon_outline_layer1;           // Outline layer 1 (green)
  ImVec4 dungeon_outline_layer2;           // Outline layer 2 (blue)
  ImVec4 text_primary;                     // Primary text color (white)
  ImVec4 text_secondary_gray;              // Secondary gray text
  ImVec4 text_info;                        // Info text (blue)
  ImVec4 text_warning_yellow;              // Warning text (yellow)
  ImVec4 text_error_red;                   // Error text (red)
  ImVec4 text_success_green;               // Success text (green)
  ImVec4 box_bg_dark;                      // Dark box background
  ImVec4 box_border;                       // Box border
  ImVec4 box_text;                         // Box text color

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

void RenderSectionHeader(const char* icon, const char* label,
                         const ImVec4& color);
void RenderStatusIndicator(const char* label, bool active);
void RenderProviderBadge(const char* provider);

// Status badge for tests/processes
enum class ButtonColor { Success, Warning, Error, Info, Default };
void StatusBadge(const char* text, ButtonColor color);

// Spacing helpers
void VerticalSpacing(float amount = 8.0f);
void HorizontalSpacing(float amount = 8.0f);

// Common button styles
bool StyledButton(const char* label, const ImVec4& color,
                  const ImVec2& size = ImVec2(0, 0));
bool IconButton(const char* icon, const char* tooltip = nullptr);

}  // namespace AgentUI

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_UI_THEME_H
