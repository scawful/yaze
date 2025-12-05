#include "app/editor/layout_designer/widget_definition.h"

#include <chrono>

#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {
namespace layout_designer {

// ============================================================================
// WidgetProperty Implementation
// ============================================================================

void WidgetDefinition::AddProperty(const std::string& name,
                                    WidgetProperty::Type type) {
  WidgetProperty prop;
  prop.name = name;
  prop.type = type;
  properties.push_back(prop);
}

WidgetProperty* WidgetDefinition::GetProperty(const std::string& name) {
  for (auto& prop : properties) {
    if (prop.name == name) {
      return &prop;
    }
  }
  return nullptr;
}

void WidgetDefinition::AddChild(std::unique_ptr<WidgetDefinition> child) {
  if (CanHaveChildren()) {
    children.push_back(std::move(child));
  }
}

bool WidgetDefinition::IsContainer() const {
  return IsContainerWidget(type);
}

bool WidgetDefinition::CanHaveChildren() const {
  return IsContainerWidget(type);
}

bool WidgetDefinition::RequiresEnd() const {
  return RequiresEndCall(type);
}

// ============================================================================
// PanelDesign Implementation
// ============================================================================

void PanelDesign::AddWidget(std::unique_ptr<WidgetDefinition> widget) {
  widgets.push_back(std::move(widget));
  Touch();
}

WidgetDefinition* PanelDesign::FindWidget(const std::string& id) {
  // Recursive search through widget tree
  std::function<WidgetDefinition*(WidgetDefinition*)> search =
      [&](WidgetDefinition* widget) -> WidgetDefinition* {
    if (widget->id == id) {
      return widget;
    }
    for (auto& child : widget->children) {
      if (auto* found = search(child.get())) {
        return found;
      }
    }
    return nullptr;
  };
  
  for (auto& widget : widgets) {
    if (auto* found = search(widget.get())) {
      return found;
    }
  }
  return nullptr;
}

std::vector<WidgetDefinition*> PanelDesign::GetAllWidgets() {
  std::vector<WidgetDefinition*> result;
  
  std::function<void(WidgetDefinition*)> collect =
      [&](WidgetDefinition* widget) {
    result.push_back(widget);
    for (auto& child : widget->children) {
      collect(child.get());
    }
  };
  
  for (auto& widget : widgets) {
    collect(widget.get());
  }
  
  return result;
}

bool PanelDesign::Validate(std::string* error_message) const {
  if (panel_id.empty()) {
    if (error_message) *error_message = "Panel ID cannot be empty";
    return false;
  }
  
  // Validate widget IDs are unique
  std::set<std::string> ids;
  for (const auto& widget : widgets) {
    if (ids.count(widget->id)) {
      if (error_message) *error_message = "Duplicate widget ID: " + widget->id;
      return false;
    }
    ids.insert(widget->id);
  }
  
  return true;
}

void PanelDesign::Touch() {
  auto now = std::chrono::system_clock::now();
  modified_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
      now.time_since_epoch()).count();
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* GetWidgetTypeName(WidgetType type) {
  switch (type) {
    case WidgetType::Text: return "Text";
    case WidgetType::TextWrapped: return "Text (Wrapped)";
    case WidgetType::TextColored: return "Colored Text";
    case WidgetType::Button: return "Button";
    case WidgetType::SmallButton: return "Small Button";
    case WidgetType::Checkbox: return "Checkbox";
    case WidgetType::RadioButton: return "Radio Button";
    case WidgetType::InputText: return "Text Input";
    case WidgetType::InputInt: return "Integer Input";
    case WidgetType::InputFloat: return "Float Input";
    case WidgetType::SliderInt: return "Integer Slider";
    case WidgetType::SliderFloat: return "Float Slider";
    case WidgetType::ColorEdit: return "Color Editor";
    case WidgetType::ColorPicker: return "Color Picker";
    case WidgetType::Separator: return "Separator";
    case WidgetType::SameLine: return "Same Line";
    case WidgetType::Spacing: return "Spacing";
    case WidgetType::Dummy: return "Dummy Space";
    case WidgetType::NewLine: return "New Line";
    case WidgetType::Indent: return "Indent";
    case WidgetType::Unindent: return "Unindent";
    case WidgetType::BeginGroup: return "Group (Begin)";
    case WidgetType::EndGroup: return "Group (End)";
    case WidgetType::BeginChild: return "Child Window (Begin)";
    case WidgetType::EndChild: return "Child Window (End)";
    case WidgetType::CollapsingHeader: return "Collapsing Header";
    case WidgetType::TreeNode: return "Tree Node";
    case WidgetType::TabBar: return "Tab Bar";
    case WidgetType::TabItem: return "Tab Item";
    case WidgetType::BeginTable: return "Table (Begin)";
    case WidgetType::EndTable: return "Table (End)";
    case WidgetType::TableNextRow: return "Table Next Row";
    case WidgetType::TableNextColumn: return "Table Next Column";
    case WidgetType::TableSetupColumn: return "Table Setup Column";
    case WidgetType::Canvas: return "Custom Canvas";
    case WidgetType::Image: return "Image";
    case WidgetType::ImageButton: return "Image Button";
    case WidgetType::ProgressBar: return "Progress Bar";
    case WidgetType::BulletText: return "Bullet Text";
    case WidgetType::BeginMenu: return "Menu (Begin)";
    case WidgetType::EndMenu: return "Menu (End)";
    case WidgetType::MenuItem: return "Menu Item";
    case WidgetType::BeginCombo: return "Combo (Begin)";
    case WidgetType::EndCombo: return "Combo (End)";
    case WidgetType::Selectable: return "Selectable";
    case WidgetType::ListBox: return "List Box";
    default: return "Unknown";
  }
}

const char* GetWidgetTypeIcon(WidgetType type) {
  switch (type) {
    case WidgetType::Text:
    case WidgetType::TextWrapped:
    case WidgetType::TextColored:
    case WidgetType::BulletText:
      return ICON_MD_TEXT_FIELDS;
      
    case WidgetType::Button:
    case WidgetType::SmallButton:
    case WidgetType::ImageButton:
      return ICON_MD_SMART_BUTTON;
      
    case WidgetType::Checkbox:
      return ICON_MD_CHECK_BOX;
      
    case WidgetType::RadioButton:
      return ICON_MD_RADIO_BUTTON_CHECKED;
      
    case WidgetType::InputText:
    case WidgetType::InputInt:
    case WidgetType::InputFloat:
      return ICON_MD_INPUT;
      
    case WidgetType::SliderInt:
    case WidgetType::SliderFloat:
      return ICON_MD_TUNE;
      
    case WidgetType::ColorEdit:
    case WidgetType::ColorPicker:
      return ICON_MD_PALETTE;
      
    case WidgetType::Separator:
      return ICON_MD_HORIZONTAL_RULE;
      
    case WidgetType::BeginTable:
    case WidgetType::EndTable:
      return ICON_MD_TABLE_CHART;
      
    case WidgetType::CollapsingHeader:
    case WidgetType::TreeNode:
      return ICON_MD_ACCOUNT_TREE;
      
    case WidgetType::TabBar:
    case WidgetType::TabItem:
      return ICON_MD_TAB;
      
    case WidgetType::Canvas:
      return ICON_MD_DRAW;
      
    case WidgetType::Image:
      return ICON_MD_IMAGE;
      
    case WidgetType::ProgressBar:
      return ICON_MD_LINEAR_SCALE;
      
    case WidgetType::BeginMenu:
    case WidgetType::EndMenu:
    case WidgetType::MenuItem:
      return ICON_MD_MENU;
      
    case WidgetType::BeginCombo:
    case WidgetType::EndCombo:
    case WidgetType::ListBox:
      return ICON_MD_ARROW_DROP_DOWN;
      
    case WidgetType::Selectable:
      return ICON_MD_CHECK_CIRCLE;
      
    default:
      return ICON_MD_WIDGETS;
  }
}

bool IsContainerWidget(WidgetType type) {
  switch (type) {
    case WidgetType::BeginGroup:
    case WidgetType::BeginChild:
    case WidgetType::CollapsingHeader:
    case WidgetType::TreeNode:
    case WidgetType::TabBar:
    case WidgetType::TabItem:
    case WidgetType::BeginTable:
    case WidgetType::BeginMenu:
    case WidgetType::BeginCombo:
      return true;
    default:
      return false;
  }
}

bool RequiresEndCall(WidgetType type) {
  switch (type) {
    case WidgetType::BeginGroup:
    case WidgetType::BeginChild:
    case WidgetType::TreeNode:
    case WidgetType::TabBar:
    case WidgetType::BeginTable:
    case WidgetType::BeginMenu:
    case WidgetType::BeginCombo:
      return true;
    default:
      return false;
  }
}

std::vector<WidgetProperty> GetDefaultProperties(WidgetType type) {
  std::vector<WidgetProperty> props;
  
  switch (type) {
    case WidgetType::Text:
    case WidgetType::TextWrapped:
    case WidgetType::BulletText: {
      WidgetProperty prop;
      prop.name = "text";
      prop.type = WidgetProperty::Type::String;
      prop.string_value = "Sample Text";
      props.push_back(prop);
      break;
    }
    
    case WidgetType::Button:
    case WidgetType::SmallButton: {
      WidgetProperty label;
      label.name = "label";
      label.type = WidgetProperty::Type::String;
      label.string_value = "Button";
      props.push_back(label);
      
      WidgetProperty size;
      size.name = "size";
      size.type = WidgetProperty::Type::Vec2;
      size.vec2_value = ImVec2(0, 0);  // Auto size
      props.push_back(size);
      break;
    }
    
    case WidgetType::Checkbox: {
      WidgetProperty label;
      label.name = "label";
      label.type = WidgetProperty::Type::String;
      label.string_value = "Checkbox";
      props.push_back(label);
      
      WidgetProperty checked;
      checked.name = "checked";
      checked.type = WidgetProperty::Type::Bool;
      checked.bool_value = false;
      props.push_back(checked);
      break;
    }
    
    case WidgetType::InputText: {
      WidgetProperty label;
      label.name = "label";
      label.type = WidgetProperty::Type::String;
      label.string_value = "Input";
      props.push_back(label);
      
      WidgetProperty hint;
      hint.name = "hint";
      hint.type = WidgetProperty::Type::String;
      hint.string_value = "Enter text...";
      props.push_back(hint);
      
      WidgetProperty buffer_size;
      buffer_size.name = "buffer_size";
      buffer_size.type = WidgetProperty::Type::Int;
      buffer_size.int_value = 256;
      props.push_back(buffer_size);
      break;
    }
    
    case WidgetType::SliderInt: {
      WidgetProperty label;
      label.name = "label";
      label.type = WidgetProperty::Type::String;
      label.string_value = "Slider";
      props.push_back(label);
      
      WidgetProperty min_val;
      min_val.name = "min";
      min_val.type = WidgetProperty::Type::Int;
      min_val.int_value = 0;
      props.push_back(min_val);
      
      WidgetProperty max_val;
      max_val.name = "max";
      max_val.type = WidgetProperty::Type::Int;
      max_val.int_value = 100;
      props.push_back(max_val);
      break;
    }
    
    case WidgetType::BeginTable: {
      WidgetProperty id;
      id.name = "id";
      id.type = WidgetProperty::Type::String;
      id.string_value = "table";
      props.push_back(id);
      
      WidgetProperty columns;
      columns.name = "columns";
      columns.type = WidgetProperty::Type::Int;
      columns.int_value = 2;
      props.push_back(columns);
      
      WidgetProperty flags;
      flags.name = "flags";
      flags.type = WidgetProperty::Type::Flags;
      flags.flags_value = 0;  // ImGuiTableFlags
      props.push_back(flags);
      break;
    }
    
    case WidgetType::Canvas: {
      WidgetProperty size;
      size.name = "size";
      size.type = WidgetProperty::Type::Vec2;
      size.vec2_value = ImVec2(300, 200);
      props.push_back(size);
      
      WidgetProperty bg_color;
      bg_color.name = "background";
      bg_color.type = WidgetProperty::Type::Color;
      bg_color.color_value = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
      props.push_back(bg_color);
      break;
    }
    
    default:
      break;
  }
  
  return props;
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

