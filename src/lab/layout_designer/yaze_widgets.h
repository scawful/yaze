#ifndef YAZE_APP_EDITOR_LAYOUT_DESIGNER_YAZE_WIDGETS_H_
#define YAZE_APP_EDITOR_LAYOUT_DESIGNER_YAZE_WIDGETS_H_

#include "lab/layout_designer/widget_definition.h"

namespace yaze {
namespace editor {
namespace layout_designer {

/**
 * @enum YazeWidgetType
 * @brief Extended widget types using yaze GUI abstractions
 */
enum class YazeWidgetType {
  // Themed buttons (from themed_widgets.h)
  ThemedButton,
  PrimaryButton,
  DangerButton,
  ThemedIconButton,
  TransparentIconButton,
  
  // Layout helpers (from ui_helpers.h)
  BeginField,       // Label + widget field pattern
  EndField,
  PropertyTable,    // Table for properties
  PropertyRow,      // Property row in table
  SectionHeader,    // Section header with icon
  
  // Layout helpers (from layout_helpers.h)
  PaddedPanel,      // Panel with standard padding
  TableWithTheming, // Table with theme awareness
  CanvasPanel,      // Canvas with toolbar
  AutoInputField,   // Auto-sized input field
  
  // Widget automation (from widget_auto_register.h)
  AutoButton,       // Auto-registered button
  AutoCheckbox,     // Auto-registered checkbox
  AutoInputText,    // Auto-registered input
  
  // Custom yaze widgets
  PaletteColorButton,  // Color button for palettes
  PanelHeader,         // Panel header with icon
};

/**
 * @brief Convert YazeWidgetType to WidgetType (for base widget system)
 */
WidgetType ToWidgetType(YazeWidgetType type);

/**
 * @brief Get human-readable name for yaze widget type
 */
const char* GetYazeWidgetTypeName(YazeWidgetType type);

/**
 * @brief Get icon for yaze widget type
 */
const char* GetYazeWidgetTypeIcon(YazeWidgetType type);

/**
 * @brief Generate code for yaze widget (uses yaze abstractions)
 * @param yaze_type The yaze widget type
 * @param widget The widget definition (for properties)
 * @param indent_level Indentation level
 * @return Generated C++ code using yaze helpers
 */
std::string GenerateYazeWidgetCode(YazeWidgetType yaze_type,
                                   const WidgetDefinition& widget,
                                   int indent_level = 0);

/**
 * @brief Get default properties for yaze widget type
 */
std::vector<WidgetProperty> GetYazeDefaultProperties(YazeWidgetType type);

/**
 * @brief Check if yaze widget requires specific includes
 */
std::vector<std::string> GetRequiredIncludes(YazeWidgetType type);

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_DESIGNER_YAZE_WIDGETS_H_

