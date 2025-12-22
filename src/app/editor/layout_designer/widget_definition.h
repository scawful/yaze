#ifndef YAZE_APP_EDITOR_LAYOUT_DESIGNER_WIDGET_DEFINITION_H_
#define YAZE_APP_EDITOR_LAYOUT_DESIGNER_WIDGET_DEFINITION_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace editor {
namespace layout_designer {

/**
 * @enum WidgetType
 * @brief Types of ImGui widgets available in the designer
 */
enum class WidgetType {
  // Basic Widgets
  Text,
  TextWrapped,
  TextColored,
  Button,
  SmallButton,
  Checkbox,
  RadioButton,
  InputText,
  InputInt,
  InputFloat,
  SliderInt,
  SliderFloat,
  ColorEdit,
  ColorPicker,
  
  // Layout Widgets
  Separator,
  SameLine,
  Spacing,
  Dummy,  // Invisible spacing
  NewLine,
  Indent,
  Unindent,
  
  // Container Widgets
  BeginGroup,
  EndGroup,
  BeginChild,
  EndChild,
  CollapsingHeader,
  TreeNode,
  TabBar,
  TabItem,
  
  // Table Widgets
  BeginTable,
  EndTable,
  TableNextRow,
  TableNextColumn,
  TableSetupColumn,
  
  // Custom Widgets
  Canvas,  // Custom drawing area
  Image,
  ImageButton,
  ProgressBar,
  BulletText,
  
  // Menu Widgets
  BeginMenu,
  EndMenu,
  MenuItem,
  
  // Combo/Dropdown
  BeginCombo,
  EndCombo,
  Selectable,
  ListBox,
};

/**
 * @struct WidgetProperty
 * @brief Represents a configurable property of a widget
 */
struct WidgetProperty {
  std::string name;
  enum class Type {
    String,
    Int,
    Float,
    Bool,
    Color,
    Vec2,
    Flags
  } type;
  
  // Value storage (union-like)
  std::string string_value;
  int int_value = 0;
  float float_value = 0.0f;
  bool bool_value = false;
  ImVec4 color_value = ImVec4(1, 1, 1, 1);
  ImVec2 vec2_value = ImVec2(0, 0);
  int flags_value = 0;
};

/**
 * @struct WidgetDefinition
 * @brief Defines a widget instance in a panel layout
 */
struct WidgetDefinition {
  std::string id;              // Unique widget ID
  WidgetType type;             // Widget type
  std::string label;           // Display label
  ImVec2 position = ImVec2(0, 0);  // Position in panel (for absolute positioning)
  ImVec2 size = ImVec2(-1, 0);     // Size (-1 = auto-width)
  
  // Properties specific to widget type
  std::vector<WidgetProperty> properties;
  
  // Hierarchy
  std::vector<std::unique_ptr<WidgetDefinition>> children;
  
  // Code generation hints
  std::string callback_name;   // For buttons, menu items, etc.
  std::string tooltip;         // Hover tooltip
  bool same_line = false;      // Should this widget be on same line as previous?
  
  // Visual hints for designer
  bool selected = false;
  ImVec4 border_color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
  
  // Helper methods
  void AddProperty(const std::string& name, WidgetProperty::Type type);
  WidgetProperty* GetProperty(const std::string& name);
  void AddChild(std::unique_ptr<WidgetDefinition> child);
  
  // Validation
  bool IsContainer() const;
  bool CanHaveChildren() const;
  bool RequiresEnd() const;  // Needs End*() call
};

/**
 * @struct PanelDesign
 * @brief Complete design definition for a panel's internal layout
 */
struct PanelDesign {
  std::string panel_id;        // e.g., "dungeon.room_selector"
  std::string panel_name;      // Human-readable name
  ImVec2 design_size = ImVec2(400, 600);  // Design canvas size
  
  // Widget tree (root level widgets)
  std::vector<std::unique_ptr<WidgetDefinition>> widgets;
  
  // Metadata
  std::string author;
  std::string version = "1.0.0";
  int64_t created_timestamp = 0;
  int64_t modified_timestamp = 0;
  
  // Helper methods
  void AddWidget(std::unique_ptr<WidgetDefinition> widget);
  WidgetDefinition* FindWidget(const std::string& id);
  std::vector<WidgetDefinition*> GetAllWidgets();
  bool Validate(std::string* error_message = nullptr) const;
  void Touch();  // Update modified timestamp
};

/**
 * @brief Get human-readable name for widget type
 */
const char* GetWidgetTypeName(WidgetType type);

/**
 * @brief Get icon for widget type
 */
const char* GetWidgetTypeIcon(WidgetType type);

/**
 * @brief Check if widget type is a container
 */
bool IsContainerWidget(WidgetType type);

/**
 * @brief Check if widget type requires an End*() call
 */
bool RequiresEndCall(WidgetType type);

/**
 * @brief Get default properties for a widget type
 */
std::vector<WidgetProperty> GetDefaultProperties(WidgetType type);

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_DESIGNER_WIDGET_DEFINITION_H_

