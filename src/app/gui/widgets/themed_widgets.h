#ifndef YAZE_APP_GUI_WIDGETS_THEMED_WIDGETS_H_
#define YAZE_APP_GUI_WIDGETS_THEMED_WIDGETS_H_

#include <string>
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// Standardized themed widgets that automatically respect the current theme.
// These abstract away the repetitive PushStyleColor/PopStyleColor calls.

/**
 * @brief Draw a standard icon button with theme-aware colors.
 *
 * @param icon The icon string (e.g., ICON_MD_SETTINGS)
 * @param tooltip Optional tooltip text
 * @param size The size of the button (default: 0,0 = auto)
 * @param is_active Whether the button is in an active/toggled state
 * @param is_disabled Whether the button is disabled
 * @param panel_id Optional animation scope key (panel ID)
 * @param anim_id Optional animation key (defaults to ImGui ID)
 * @return true if clicked
 */
bool ThemedIconButton(const char* icon, const char* tooltip = nullptr,
                      const ImVec2& size = ImVec2(0, 0),
                      bool is_active = false,
                      bool is_disabled = false,
                      const char* panel_id = nullptr,
                      const char* anim_id = nullptr);

/**
 * @brief Draw a transparent icon button (hover effect only).
 *
 * @param icon The icon string (e.g., ICON_MD_SETTINGS)
 * @param size The size of the button
 * @param tooltip Optional tooltip text
 * @param is_active Whether the button is in an active/toggled state
 * @param active_color Optional custom color for active state icon
 *                     If alpha is 0, uses theme.primary instead
 * @param panel_id Optional animation scope key (panel ID)
 * @param anim_id Optional animation key (defaults to ImGui ID)
 * @return true if clicked
 */
bool TransparentIconButton(const char* icon, const ImVec2& size,
                           const char* tooltip = nullptr,
                           bool is_active = false,
                           const ImVec4& active_color = ImVec4(0, 0, 0, 0),
                           const char* panel_id = nullptr,
                           const char* anim_id = nullptr);

/**
 * @brief Draw a standard text button with theme colors.
 *
 * @param label The button label text
 * @param size The size of the button (default: 0,0 = auto)
 * @param panel_id Optional animation scope key (panel ID)
 * @param anim_id Optional animation key (defaults to ImGui ID)
 */
bool ThemedButton(const char* label, const ImVec2& size = ImVec2(0, 0),
                  const char* panel_id = nullptr,
                  const char* anim_id = nullptr);

/**
 * @brief Draw a primary action button (accented color).
 *
 * @param label The button label text
 * @param size The size of the button (default: 0,0 = auto)
 * @param panel_id Optional animation scope key (panel ID)
 * @param anim_id Optional animation key (defaults to ImGui ID)
 */
bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0),
                   const char* panel_id = nullptr,
                   const char* anim_id = nullptr);

/**
 * @brief Draw a danger action button (error color).
 *
 * @param label The button label text
 * @param size The size of the button (default: 0,0 = auto)
 * @param panel_id Optional animation scope key (panel ID)
 * @param anim_id Optional animation key (defaults to ImGui ID)
 */
bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0),
                  const char* panel_id = nullptr,
                  const char* anim_id = nullptr);

/**
 * @brief Draw a section header.
 */
void SectionHeader(const char* label);

/**
 * @brief Draw a palette color button.
 *
 * @param id The button ID
 * @param color The SNES color to display
 * @param is_selected Whether this color is currently selected
 * @param is_modified Whether this color has been modified
 * @param size The size of the button
 * @param panel_id Optional animation scope key (panel ID)
 * @param anim_id Optional animation key (defaults to ImGui ID)
 */
bool PaletteColorButton(const char* id, const struct SnesColor& color,
                        bool is_selected, bool is_modified,
                        const ImVec2& size,
                        const char* panel_id = nullptr,
                        const char* anim_id = nullptr);

/**
 * @brief Draw a panel header with consistent styling.
 *
 * @param panel_id Optional animation scope key (panel ID)
 */
void PanelHeader(const char* title, const char* icon = nullptr,
                 bool* p_open = nullptr, const char* panel_id = nullptr);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_THEMED_WIDGETS_H_
