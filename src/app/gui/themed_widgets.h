#ifndef YAZE_APP_GUI_THEMED_WIDGETS_H
#define YAZE_APP_GUI_THEMED_WIDGETS_H

#include "app/gui/color.h"
#include "app/gui/layout_helpers.h"
#include "app/gui/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Opt-in themed widget library for gradual migration
 *
 * All widgets automatically use the current theme from ThemeManager.
 * Editors can opt-in by using these widgets instead of raw ImGui calls.
 *
 * Usage:
 * ```cpp
 * using namespace yaze::gui::themed;
 *
 * if (Button("Save")) {
 *     // Button uses theme colors automatically
 * }
 *
 * Header("Settings");  // Themed section header
 *
 * Card("Properties", [&]() {
 *     // Content inside themed card
 * });
 * ```
 */
namespace themed {

// ============================================================================
// Buttons
// ============================================================================

/**
 * @brief Themed button with automatic color application
 */
bool Button(const char* label, const ImVec2& size = ImVec2(0, 0));

/**
 * @brief Themed button with icon (Material Design Icons)
 */
bool IconButton(const char* icon, const char* tooltip = nullptr);

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
void Header(const char* label);

/**
 * @brief Collapsible section with themed header
 */
bool CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags = 0);

// ============================================================================
// Cards & Panels
// ============================================================================

/**
 * @brief Themed card with rounded corners and shadow
 * @param label Unique ID for the card
 * @param content Callback function to render card content
 * @param size Card size (0, 0 for auto-size)
 */
void Card(const char* label, std::function<void()> content,
          const ImVec2& size = ImVec2(0, 0));

/**
 * @brief Begin themed panel (manual version of Card)
 */
void BeginPanel(const char* label, const ImVec2& size = ImVec2(0, 0));

/**
 * @brief End themed panel
 */
void EndPanel();

// ============================================================================
// Inputs
// ============================================================================

/**
 * @brief Themed text input
 */
bool InputText(const char* label, char* buf, size_t buf_size,
               ImGuiInputTextFlags flags = 0);

/**
 * @brief Themed integer input
 */
bool InputInt(const char* label, int* v, int step = 1, int step_fast = 100,
              ImGuiInputTextFlags flags = 0);

/**
 * @brief Themed float input
 */
bool InputFloat(const char* label, float* v, float step = 0.0f,
                float step_fast = 0.0f, const char* format = "%.3f",
                ImGuiInputTextFlags flags = 0);

/**
 * @brief Themed checkbox
 */
bool Checkbox(const char* label, bool* v);

/**
 * @brief Themed combo box
 */
bool Combo(const char* label, int* current_item, const char* const items[],
           int items_count, int popup_max_height_in_items = -1);

// ============================================================================
// Tables
// ============================================================================

/**
 * @brief Begin themed table with automatic styling
 */
bool BeginTable(const char* str_id, int columns, ImGuiTableFlags flags = 0,
                const ImVec2& outer_size = ImVec2(0, 0),
                float inner_width = 0.0f);

/**
 * @brief End themed table
 */
void EndTable();

// ============================================================================
// Tooltips & Help
// ============================================================================

/**
 * @brief Themed help marker with tooltip
 */
void HelpMarker(const char* desc);

/**
 * @brief Begin themed tooltip
 */
void BeginTooltip();

/**
 * @brief End themed tooltip
 */
void EndTooltip();

// ============================================================================
// Status & Feedback
// ============================================================================

enum class StatusType { kSuccess, kWarning, kError, kInfo };
/**
 * @brief Themed status text (success, warning, error, info)
 */
void StatusText(const char* text, StatusType type);

/**
 * @brief Themed progress bar
 */
void ProgressBar(float fraction, const ImVec2& size = ImVec2(-1, 0),
                 const char* overlay = nullptr);

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
void PushWidgetColors();

/**
 * @brief Restore previous colors
 */
void PopWidgetColors();

}  // namespace themed
}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_THEMED_WIDGETS_H
