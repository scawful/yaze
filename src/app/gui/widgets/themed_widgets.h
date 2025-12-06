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
 * @return true if clicked
 */
bool ThemedIconButton(const char* icon, const char* tooltip = nullptr,
                      const ImVec2& size = ImVec2(0, 0), 
                      bool is_active = false, 
                      bool is_disabled = false);

/**
 * @brief Draw a transparent icon button (hover effect only).
 *
 * @param icon The icon string (e.g., ICON_MD_SETTINGS)
 * @param size The size of the button
 * @param tooltip Optional tooltip text
 * @param is_active Whether the button is in an active/toggled state
 * @param active_color Optional custom color for active state icon
 *                     If alpha is 0, uses theme.primary instead
 * @return true if clicked
 */
bool TransparentIconButton(const char* icon, const ImVec2& size,
                           const char* tooltip = nullptr,
                           bool is_active = false,
                           const ImVec4& active_color = ImVec4(0, 0, 0, 0));

/**
 * @brief Draw a standard text button with theme colors.
 */
bool ThemedButton(const char* label, const ImVec2& size = ImVec2(0, 0));

/**
 * @brief Draw a primary action button (accented color).
 */
bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));

/**
 * @brief Draw a danger action button (error color).
 */
bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0));

/**
 * @brief Draw a section header.
 */
void SectionHeader(const char* label);

/**
 * @brief Draw a palette color button.
 */
bool PaletteColorButton(const char* id, const struct SnesColor& color, 
                        bool is_selected, bool is_modified, 
                        const ImVec2& size);

/**
 * @brief Draw a panel header with consistent styling.
 */
void PanelHeader(const char* title, const char* icon = nullptr,
                 bool* p_open = nullptr);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_THEMED_WIDGETS_H_
