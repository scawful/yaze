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

// Semantic color enum for type-safe theme color access.
enum class SemanticColor {
  Primary,
  Secondary,
  Disabled,
  Error,
  Warning,
  Success,
  Info,
  OnSurface,
  OnSurfaceVariant
};

// Resolve a SemanticColor to an ImVec4 from the current theme.
ImVec4 ResolveSemanticColor(SemanticColor color);

// Gets a color from the current theme
ImVec4 GetThemeColor(ImGuiCol idx);

// Semantic colors from current theme
ImVec4 GetSuccessColor();
ImVec4 GetWarningColor();
ImVec4 GetErrorColor();
ImVec4 GetInfoColor();
ImVec4 GetAccentColor();
ImVec4 GetDisabledColor();
ImVec4 GetSelectedColor();
ImVec4 GetModifiedColor();

// ============================================================================
// Colored Text Shortcuts
// ============================================================================
// Replace PushStyleColor(ImGuiCol_Text, c); Text(...); PopStyleColor();

// Render text with an explicit ImVec4 color.
void ColoredText(const char* text, const ImVec4& color);

// Render formatted text with an explicit ImVec4 color.
void ColoredTextF(const ImVec4& color, const char* fmt, ...) IM_FMTARGS(2);

// Render text with a SemanticColor from the current theme.
void ThemedText(const char* text, SemanticColor color);

// ============================================================================
// Button Color Sets
// ============================================================================

// Pre-computed 3-color set for styled buttons (button, hovered, active).
struct ButtonColorSet {
  ImVec4 button;
  ImVec4 hovered;
  ImVec4 active;
};

ButtonColorSet GetPrimaryButtonColors();
ButtonColorSet GetSuccessButtonColors();
ButtonColorSet GetWarningButtonColors();

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

// Icon-only button (AgentUI style)
bool IconButton(const char* icon, const char* tooltip = nullptr);

// Colored button for status actions
enum class ButtonType { Default, Success, Warning, Error, Info };
bool ColoredButton(const char* label, ButtonType type,
                   const ImVec2& size = ImVec2(0, 0));

// Styled button with custom color (AgentUI style)
bool StyledButton(const char* label, const ImVec4& color,
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

// Modal confirmation dialog
bool ConfirmationDialog(const char* id, const char* title, const char* message,
                        const char* confirm_text = "OK",
                        const char* cancel_text = "Cancel");

// ============================================================================
// Visual Indicators
// ============================================================================

// Provider badge (AgentUI style)
void RenderProviderBadge(const char* provider);

// ROM version badge
void RomVersionBadge(const char* version, bool is_vanilla);

// ============================================================================
// Spacing and Alignment
// ============================================================================

void VerticalSpacing(float pixels = 8.0f);
void HorizontalSpacing(float pixels = 8.0f);
void CenterText(const char* text);

// ============================================================================
// Input Helpers (complement existing gui::InputHex functions)
// ============================================================================

// Labeled hex input with automatic formatting
bool LabeledInputHex(const char* label, uint8_t* value);
bool LabeledInputHex(const char* label, uint16_t* value);

// Helper to create consistent card titles
std::string MakePanelTitle(const std::string& title);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_UI_HELPERS_H
