#ifndef YAZE_APP_GUI_LAYOUT_HELPERS_H
#define YAZE_APP_GUI_LAYOUT_HELPERS_H

#include "imgui/imgui.h"
#include "app/gui/theme_manager.h"

namespace yaze {
namespace gui {

/**
 * @brief Theme-aware sizing helpers for consistent UI layout
 *
 * All sizing functions respect the current theme's compact_factor and
 * semantic multipliers, ensuring layouts are consistent but customizable.
 */
class LayoutHelpers {
 public:
  // Core sizing functions (respect theme compact_factor + multipliers)
  static float GetStandardWidgetHeight();
  static float GetStandardSpacing();
  static float GetToolbarHeight();
  static float GetPanelPadding();
  static float GetStandardInputWidth();
  static float GetButtonPadding();
  static float GetTableRowHeight();
  static float GetCanvasToolbarHeight();

  // Layout utilities
  static void BeginPaddedPanel(const char* label, float padding = -1.0f);
  static void EndPaddedPanel();

  static bool BeginTableWithTheming(const char* str_id, int columns,
                                     ImGuiTableFlags flags = 0,
                                     const ImVec2& outer_size = ImVec2(0, 0),
                                     float inner_width = 0.0f);
  static void EndTable() { ImGui::EndTable(); }

  static void BeginCanvasPanel(const char* label, ImVec2* canvas_size = nullptr);
  static void EndCanvasPanel();

  // Input field helpers
  static bool AutoSizedInputField(const char* label, char* buf, size_t buf_size,
                                   ImGuiInputTextFlags flags = 0);
  static bool AutoSizedInputInt(const char* label, int* v, int step = 1,
                                 int step_fast = 100, ImGuiInputTextFlags flags = 0);
  static bool AutoSizedInputFloat(const char* label, float* v, float step = 0.0f,
                                   float step_fast = 0.0f, const char* format = "%.3f",
                                   ImGuiInputTextFlags flags = 0);

  // Input preset functions for common patterns
  static bool InputHexRow(const char* label, uint8_t* data);
  static bool InputHexRow(const char* label, uint16_t* data);
  static void BeginPropertyGrid(const char* label);
  static void EndPropertyGrid();
  static bool InputToolbarField(const char* label, char* buf, size_t buf_size);

  // Toolbar helpers
  static void BeginToolbar(const char* label);
  static void EndToolbar();
  static void ToolbarSeparator();
  static bool ToolbarButton(const char* label, const ImVec2& size = ImVec2(0, 0));

  // Common layout patterns
  static void PropertyRow(const char* label, std::function<void()> widget_callback);
  static void SectionHeader(const char* label);
  static void HelpMarker(const char* desc);

  // Get current theme
  static const EnhancedTheme& GetTheme() {
    return ThemeManager::Get().GetCurrentTheme();
  }

 private:
  static float GetBaseFontSize() { return ImGui::GetFontSize(); }
  static float ApplyCompactFactor(float base_value) {
    return base_value * GetTheme().compact_factor;
  }
};

} // namespace gui
} // namespace yaze

#endif // YAZE_APP_GUI_LAYOUT_HELPERS_H
