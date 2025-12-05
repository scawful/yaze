#ifndef YAZE_APP_EDITOR_LAYOUT_DESIGNER_THEME_PROPERTIES_H_
#define YAZE_APP_EDITOR_LAYOUT_DESIGNER_THEME_PROPERTIES_H_

#include <string>
#include "imgui/imgui.h"

namespace yaze {
namespace editor {
namespace layout_designer {

/**
 * @struct ThemeProperties
 * @brief Encapsulates ImGui style properties for visual design
 * 
 * This allows layout designer to expose theming properties directly,
 * eliminating the need for complex Display Settings menus.
 */
struct ThemeProperties {
  // Padding
  ImVec2 window_padding = ImVec2(10, 10);
  ImVec2 frame_padding = ImVec2(10, 2);
  ImVec2 cell_padding = ImVec2(4, 5);
  ImVec2 item_spacing = ImVec2(10, 5);
  ImVec2 item_inner_spacing = ImVec2(5, 5);
  
  // Rounding
  float window_rounding = 0.0f;
  float child_rounding = 0.0f;
  float frame_rounding = 5.0f;
  float popup_rounding = 0.0f;
  float scrollbar_rounding = 5.0f;
  float grab_rounding = 0.0f;
  float tab_rounding = 0.0f;
  
  // Borders
  float window_border_size = 0.0f;
  float child_border_size = 1.0f;
  float popup_border_size = 1.0f;
  float frame_border_size = 0.0f;
  float tab_border_size = 0.0f;
  
  // Sizes
  float indent_spacing = 20.0f;
  float scrollbar_size = 14.0f;
  float grab_min_size = 15.0f;
  
  // Apply these properties to ImGui style
  void Apply() const;
  
  // Load from current ImGui style
  void LoadFromCurrent();
  
  // Reset to defaults
  void Reset();
  
  // Export as code
  std::string GenerateStyleCode() const;
};

/**
 * @class ThemePropertiesPanel
 * @brief UI panel for editing theme properties in the layout designer
 */
class ThemePropertiesPanel {
 public:
  ThemePropertiesPanel() = default;
  
  /**
   * @brief Draw the theme properties editor
   * @param properties The theme properties to edit
   * @return true if any property was modified
   */
  bool Draw(ThemeProperties& properties);
  
 private:
  void DrawPaddingSection(ThemeProperties& properties);
  void DrawRoundingSection(ThemeProperties& properties);
  void DrawBordersSection(ThemeProperties& properties);
  void DrawSizesSection(ThemeProperties& properties);
  
  bool show_padding_ = true;
  bool show_rounding_ = true;
  bool show_borders_ = true;
  bool show_sizes_ = true;
};

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_DESIGNER_THEME_PROPERTIES_H_

