#ifndef YAZE_APP_GUI_WIDGETS_THEMED_WIDGETS_H_
#define YAZE_APP_GUI_WIDGETS_THEMED_WIDGETS_H_

#include <functional>
#include <string>

#include "app/gui/core/ui_config.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// Standardized themed widgets that automatically respect the current theme.
// These abstract away the repetitive PushStyleColor/PopStyleColor calls.

// ============================================================================
// Icon Size Presets
// ============================================================================

namespace IconSize {
inline ImVec2 Small() {
  return {UIConfig::kIconButtonSmall, UIConfig::kIconButtonSmall};
}
inline ImVec2 Medium() {
  return {UIConfig::kIconButtonMedium, UIConfig::kIconButtonMedium};
}
inline ImVec2 Large() {
  return {UIConfig::kIconButtonLarge, UIConfig::kIconButtonLarge};
}
inline ImVec2 Toolbar() {
  return {UIConfig::kIconButtonToolbar, UIConfig::kIconButtonToolbar};
}
inline ImVec2 ActivityBar() {
  return {UIConfig::kActivityBarIconWidth, UIConfig::kActivityBarIconHeight};
}
}  // namespace IconSize

/**
 * @brief Draw a button with animated click ripple effect.
 *
 * @param label The button label text
 * @param size The size of the button (default: 0,0 = auto)
 * @param ripple_color Optional ripple color (default: white)
 * @param panel_id Optional animation scope key (panel ID)
 * @param anim_id Optional animation key (defaults to ImGui ID)
 * @return true if clicked
 */
bool RippleButton(const char* label, const ImVec2& size = ImVec2(0, 0),
                  const ImVec4& ripple_color = ImVec4(1.0f, 1.0f, 1.0f, 0.3f),
                  const char* panel_id = nullptr,
                  const char* anim_id = nullptr);

/**
 * @brief Draw a bouncy animated button that scales on press.
 *
 * @param label The button label text
 * @param size The size of the button (default: 0,0 = auto)
 * @param panel_id Optional animation scope key (panel ID)
 * @param anim_id Optional animation key (defaults to ImGui ID)
 * @return true if clicked
 */
bool BouncyButton(const char* label, const ImVec2& size = ImVec2(0, 0),
                  const char* panel_id = nullptr,
                  const char* anim_id = nullptr);

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
                      const ImVec2& size = ImVec2(0, 0), bool is_active = false,
                      bool is_disabled = false, const char* panel_id = nullptr,
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
 * @brief Draw a success action button (green color).
 *
 * @param label The button label text
 * @param size The size of the button (default: 0,0 = auto)
 * @param panel_id Optional animation scope key (panel ID)
 * @param anim_id Optional animation key (defaults to ImGui ID)
 */
bool SuccessButton(const char* label, const ImVec2& size = ImVec2(0, 0),
                   const char* panel_id = nullptr,
                   const char* anim_id = nullptr);

/**
 * @brief Convenience wrapper for toolbar-sized icon buttons.
 *
 * Wraps ThemedIconButton with IconSize::Toolbar() preset.
 *
 * @param icon The icon string (e.g., ICON_MD_SETTINGS)
 * @param tooltip Optional tooltip text
 * @param is_active Whether the button is in an active/toggled state
 * @return true if clicked
 */
bool ToolbarIconButton(const char* icon, const char* tooltip = nullptr,
                       bool is_active = false);

/**
 * @brief Convenience wrapper for small inline icon buttons.
 *
 * Wraps ThemedIconButton with IconSize::Small() preset.
 * Suitable for status bars, property rows, and inline actions.
 *
 * @param icon The icon string (e.g., ICON_MD_UNDO)
 * @param tooltip Optional tooltip text
 * @param is_active Whether the button is in an active/toggled state
 * @return true if clicked
 */
bool InlineIconButton(const char* icon, const char* tooltip = nullptr,
                      bool is_active = false);

/**
 * @brief Draw a section header.
 */
void SectionHeader(const char* label);

/**
 * @brief A stylized tab bar with "Mission Control" branding.
 */
bool BeginThemedTabBar(const char* id, ImGuiTabBarFlags flags = 0);
void EndThemedTabBar();

/**
 * @brief Provide visual "flash" feedback when a value changes.
 *
 * Triggers a brief color pulse on the widget background or border
 * to confirm data entry/mutation.
 *
 * @param changed The result from an ImGui input (e.g., InputHexByte)
 * @param id Unique ID for the animation state
 */
void ValueChangeFlash(bool changed, const char* id);

/**
 * @brief Draw a stylized Heads-Up Display (HUD) for canvas status.
 *
 * Encloses text/icons in a semi-transparent dark container with a
 * subtle accent border. Used for Zoom, Coords, etc.
 *
 * @param label Unique ID for the HUD window
 * @param pos Screen position for the HUD
 * @param size HUD dimensions
 * @param draw_content Callback to render HUD contents
 */
void DrawCanvasHUD(const char* label, const ImVec2& pos, const ImVec2& size,
                   std::function<void()> draw_content);

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
                        bool is_selected, bool is_modified, const ImVec2& size,
                        const char* panel_id = nullptr,
                        const char* anim_id = nullptr);

/**
 * @brief Draw a panel header with consistent styling.
 *
 * @param panel_id Optional animation scope key (panel ID)
 */
void PanelHeader(const char* title, const char* icon = nullptr,
                 bool* p_open = nullptr, const char* panel_id = nullptr);

/**
 * @brief Draw a tooltip with theme-aware background and borders.
 */
void ThemedTooltip(const char* text);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_THEMED_WIDGETS_H_
