#ifndef YAZE_APP_GUI_UI_HELPERS_H
#define YAZE_APP_GUI_UI_HELPERS_H

#include "imgui/imgui.h"
#include <string>

namespace yaze {
namespace gui {

// A collection of helper functions and widgets to standardize UI development
// and reduce boilerplate ImGui code.

// --- Theming and Colors ---

// Gets a color from the current theme.
ImVec4 GetThemeColor(ImGuiCol idx);

// Gets a semantic color from the current theme.
ImVec4 GetSuccessColor();
ImVec4 GetWarningColor();
ImVec4 GetErrorColor();
ImVec4 GetInfoColor();
ImVec4 GetAccentColor();

// --- Layout Helpers ---

// Begins a standard row for a label and a widget.
void BeginField(const char* label);
// Ends a field row.
void EndField();

// --- Widget Wrappers ---

// A button with an icon from the Material Design icon font.
bool IconButton(const char* icon, const char* label, const ImVec2& size = ImVec2(0, 0));

// A help marker that shows a tooltip on hover.
void HelpMarker(const char* desc);

// A separator with centered text.
void SeparatorText(const char* label);

} // namespace gui
} // namespace yaze

#endif // YAZE_APP_GUI_UI_HELPERS_H
