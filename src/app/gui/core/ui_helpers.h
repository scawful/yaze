#ifndef YAZE_APP_GUI_UI_HELPERS_H
#define YAZE_APP_GUI_UI_HELPERS_H

#include <functional>
#include <string>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// A collection of helper functions and widgets to standardize UI development
// and reduce boilerplate ImGui code across all editors.

// ============================================================================
// Theme and Semantic Colors
// ============================================================================

// Gets a color from the current theme
ImVec4 GetThemeColor(ImGuiCol idx);

// Semantic colors from current theme
ImVec4 GetSuccessColor();
ImVec4 GetWarningColor();
ImVec4 GetErrorColor();
ImVec4 GetInfoColor();
ImVec4 GetAccentColor();

// Entity/Map marker colors (for overworld, dungeon)
ImVec4 GetEntranceColor();
ImVec4 GetExitColor();
ImVec4 GetItemColor();
ImVec4 GetSpriteColor();
ImVec4 GetSelectedColor();
ImVec4 GetLockedColor();

// Status colors
ImVec4 GetVanillaRomColor();
ImVec4 GetCustomRomColor();
ImVec4 GetModifiedColor();

// ============================================================================
// Layout Helpers
// ============================================================================

// Label + widget field pattern
void BeginField(const char* label, float label_width = 0.0f);
void EndField();

// Property table pattern (common in editors)
bool BeginPropertyTable(const char* id, int columns = 2,
                        ImGuiTableFlags extra_flags = 0);
void EndPropertyTable();

// Property row helpers
void PropertyRow(const char* label, const char* value);
void PropertyRow(const char* label, int value);
void PropertyRowHex(const char* label, uint8_t value);
void PropertyRowHex(const char* label, uint16_t value);

// Section headers with icons
void SectionHeader(const char* icon, const char* label,
                   const ImVec4& color = ImVec4(1, 1, 1, 1));

// ============================================================================
// Common Widget Patterns
// ============================================================================

// Button with icon
bool IconButton(const char* icon, const char* label,
                const ImVec2& size = ImVec2(0, 0));

// Colored button for status actions
enum class ButtonType { Default, Success, Warning, Error, Info };
bool ColoredButton(const char* label, ButtonType type,
                   const ImVec2& size = ImVec2(0, 0));

// Toggle button with visual state
bool ToggleIconButton(const char* icon_on, const char* icon_off, bool* state,
                      const char* tooltip = nullptr);

// Toggle button that looks like a regular button but stays pressed
bool ToggleButton(const char* label, bool active,
                  const ImVec2& size = ImVec2(0, 0));

// Help marker with tooltip
void HelpMarker(const char* desc);

// Separator with text
void SeparatorText(const char* label);

// Status badge (pill-shaped colored label)
void StatusBadge(const char* text, ButtonType type = ButtonType::Default);

// ============================================================================
// Editor-Specific Patterns
// ============================================================================

// Toolset table (horizontal button bar)
void BeginToolset(const char* id);
void EndToolset();
void ToolsetButton(const char* icon, bool selected, const char* tooltip,
                   std::function<void()> on_click);

// Canvas container patterns
void BeginCanvasContainer(const char* id, bool scrollable = true);
void EndCanvasContainer();

// Tab pattern for editor modes
bool EditorTabItem(const char* icon, const char* label, bool* p_open = nullptr);

// Modal confirmation dialog
bool ConfirmationDialog(const char* id, const char* title, const char* message,
                        const char* confirm_text = "OK",
                        const char* cancel_text = "Cancel");

// ============================================================================
// Visual Indicators
// ============================================================================

// Status indicator dot + label
void StatusIndicator(const char* label, bool active,
                     const char* tooltip = nullptr);

// ROM version badge
void RomVersionBadge(const char* version, bool is_vanilla);

// Locked/Unlocked indicator
void LockIndicator(bool locked, const char* label);

// ============================================================================
// Spacing and Alignment
// ============================================================================

void VerticalSpacing(float pixels = 8.0f);
void HorizontalSpacing(float pixels = 8.0f);
void CenterText(const char* text);
void RightAlign(float width);

// ============================================================================
// Animation Helpers
// ============================================================================

// Pulsing alpha animation (for loading indicators, etc.)
float GetPulseAlpha(float speed = 1.0f);

// Fade-in animation (for panel transitions)
float GetFadeIn(float duration = 0.3f);

// Apply pulsing effect to next widget
void PushPulseEffect(float speed = 1.0f);
void PopPulseEffect();

// Loading spinner (animated circle)
void LoadingSpinner(const char* label = nullptr, float radius = 10.0f);

// ============================================================================
// Responsive Layout Helpers
// ============================================================================

// Get responsive width based on available space
float GetResponsiveWidth(float min_width, float max_width, float ratio = 0.5f);

// Auto-fit table columns
void SetupResponsiveColumns(int count, float min_col_width = 100.0f);

// Responsive two-column layout
void BeginTwoColumns(const char* id, float split_ratio = 0.6f);
void SwitchColumn();
void EndTwoColumns();

// ============================================================================
// Input Helpers (complement existing gui::InputHex functions)
// ============================================================================

// Labeled hex input with automatic formatting
bool LabeledInputHex(const char* label, uint8_t* value);
bool LabeledInputHex(const char* label, uint16_t* value);

// Combo with icon
bool IconCombo(const char* icon, const char* label, int* current,
               const char* const items[], int count);

// Helper to create consistent card titles
std::string MakePanelTitle(const std::string& title);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_UI_HELPERS_H
