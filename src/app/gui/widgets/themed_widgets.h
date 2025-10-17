#ifndef YAZE_APP_GUI_THEMED_WIDGETS_H
#define YAZE_APP_GUI_THEMED_WIDGETS_H

#include "app/gui/core/color.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Theme-aware widget library
 *
 * All widgets in this file automatically use the current theme from ThemeManager.
 * These are drop-in replacements for standard ImGui widgets with automatic theming.
 *
 * Usage:
 * ```cpp
 * using namespace yaze::gui;
 *
 * if (ThemedButton("Save")) {
 *     // Button uses theme colors automatically
 * }
 *
 * SectionHeader("Settings");  // Themed section header
 *
 * ThemedCard("Properties", [&]() {
 *     // Content inside themed card
 * });
 * ```
 */

// ============================================================================
// Buttons
// ============================================================================

/**
 * @brief Themed button with automatic color application
 */
bool ThemedButton(const char* label, const ImVec2& size = ImVec2(0, 0));

/**
 * @brief Themed button with icon (Material Design Icons)
 */
bool ThemedIconButton(const char* icon, const char* tooltip = nullptr);

/**
 * @brief Primary action button (uses accent color)
 */
bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));

/**
 * @brief Danger/destructive action button (uses error color)
 */
bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0));

// ============================================================================
// Headers & Sections
// ============================================================================

/**
 * @brief Themed section header with accent color
 */
void SectionHeader(const char* label);

/**
 * @brief Collapsible section with themed header
 */
bool ThemedCollapsingHeader(const char* label, ImGuiTreeNodeFlags flags = 0);

// ============================================================================
// Cards & Panels
// ============================================================================

/**
 * @brief Themed card with rounded corners and shadow
 * @param label Unique ID for the card
 * @param content Callback function to render card content
 * @param size Card size (0, 0 for auto-size)
 */
void ThemedCard(const char* label, std::function<void()> content,
                const ImVec2& size = ImVec2(0, 0));

/**
 * @brief Begin themed panel (manual version of ThemedCard)
 */
void BeginThemedPanel(const char* label, const ImVec2& size = ImVec2(0, 0));

/**
 * @brief End themed panel
 */
void EndThemedPanel();

// ============================================================================
// Inputs
// ============================================================================

/**
 * @brief Themed text input
 */
bool ThemedInputText(const char* label, char* buf, size_t buf_size,
                     ImGuiInputTextFlags flags = 0);

/**
 * @brief Themed integer input
 */
bool ThemedInputInt(const char* label, int* v, int step = 1, int step_fast = 100,
                    ImGuiInputTextFlags flags = 0);

/**
 * @brief Themed float input
 */
bool ThemedInputFloat(const char* label, float* v, float step = 0.0f,
                      float step_fast = 0.0f, const char* format = "%.3f",
                      ImGuiInputTextFlags flags = 0);

/**
 * @brief Themed checkbox
 */
bool ThemedCheckbox(const char* label, bool* v);

/**
 * @brief Themed combo box
 */
bool ThemedCombo(const char* label, int* current_item, const char* const items[],
                 int items_count, int popup_max_height_in_items = -1);

// ============================================================================
// Tables
// ============================================================================

/**
 * @brief Begin themed table with automatic styling
 */
bool BeginThemedTable(const char* str_id, int columns, ImGuiTableFlags flags = 0,
                      const ImVec2& outer_size = ImVec2(0, 0),
                      float inner_width = 0.0f);

/**
 * @brief End themed table
 */
void EndThemedTable();

// ============================================================================
// Tooltips & Help
// ============================================================================

/**
 * @brief Themed help marker with tooltip
 */
void ThemedHelpMarker(const char* desc);

/**
 * @brief Begin themed tooltip
 */
void BeginThemedTooltip();

/**
 * @brief End themed tooltip
 */
void EndThemedTooltip();

// ============================================================================
// Status & Feedback
// ============================================================================

enum class StatusType { kSuccess, kWarning, kError, kInfo };
/**
 * @brief Themed status text (success, warning, error, info)
 */
void ThemedStatusText(const char* text, StatusType type);

/**
 * @brief Themed progress bar
 */
void ThemedProgressBar(float fraction, const ImVec2& size = ImVec2(-1, 0),
                       const char* overlay = nullptr);

// ============================================================================
// Palette Editor Widgets
// ============================================================================

// NOTE: PaletteColorButton moved to color.h for consistency with other color utilities

/**
 * @brief Display color information with copy-to-clipboard functionality
 * @param color SNES color to display info for
 * @param show_snes_format Show SNES $xxxx format
 * @param show_hex_format Show #xxxxxx hex format
 */
void ColorInfoPanel(const yaze::gfx::SnesColor& color,
                   bool show_snes_format = true,
                   bool show_hex_format = true);

/**
 * @brief Modified indicator badge (displayed as text with icon)
 * @param is_modified Whether to show the badge
 * @param text Optional text to display after badge
 */
void ModifiedBadge(bool is_modified, const char* text = nullptr);

// ============================================================================
// Utility
// ============================================================================

/**
 * @brief Get current theme (shortcut)
 */
inline const EnhancedTheme& GetTheme() {
  return ThemeManager::Get().GetCurrentTheme();
}

/**
 * @brief Apply theme colors to next widget
 */
void PushThemedWidgetColors();

/**
 * @brief Restore previous colors
 */
void PopThemedWidgetColors();

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_THEMED_WIDGETS_H
