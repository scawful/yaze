#ifndef YAZE_APP_EDITOR_LAYOUT_DESIGNER_WIDGET_CODE_GENERATOR_H_
#define YAZE_APP_EDITOR_LAYOUT_DESIGNER_WIDGET_CODE_GENERATOR_H_

#include <string>
#include <vector>

#include "app/editor/layout_designer/widget_definition.h"

namespace yaze {
namespace editor {
namespace layout_designer {

/**
 * @class WidgetCodeGenerator
 * @brief Generates C++ ImGui code from widget definitions
 */
class WidgetCodeGenerator {
 public:
  /**
   * @brief Generate complete panel Draw() method code
   * @param design The panel design
   * @return C++ code string
   */
  static std::string GeneratePanelDrawMethod(const PanelDesign& design);
  
  /**
   * @brief Generate code for a single widget
   * @param widget The widget definition
   * @param indent_level Indentation level for formatting
   * @return C++ code string
   */
  static std::string GenerateWidgetCode(const WidgetDefinition& widget,
                                        int indent_level = 0);
  
  /**
   * @brief Generate member variable declarations for panel
   * @param design The panel design
   * @return C++ code for private members
   */
  static std::string GenerateMemberVariables(const PanelDesign& design);
  
  /**
   * @brief Generate initialization code for panel constructor
   * @param design The panel design
   * @return C++ initialization code
   */
  static std::string GenerateInitializationCode(const PanelDesign& design);

 private:
  static std::string GetIndent(int level);
  static std::string EscapeString(const std::string& str);
  static std::string GenerateButtonCode(const WidgetDefinition& widget, int indent);
  static std::string GenerateTextCode(const WidgetDefinition& widget, int indent);
  static std::string GenerateInputCode(const WidgetDefinition& widget, int indent);
  static std::string GenerateTableCode(const WidgetDefinition& widget, int indent);
  static std::string GenerateCanvasCode(const WidgetDefinition& widget, int indent);
  static std::string GenerateContainerCode(const WidgetDefinition& widget, int indent);
  
  // Get variable name for widget (e.g., button_clicked_, input_text_buffer_)
  static std::string GetVariableName(const WidgetDefinition& widget);
};

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_DESIGNER_WIDGET_CODE_GENERATOR_H_

