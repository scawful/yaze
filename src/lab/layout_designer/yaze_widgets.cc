#include "lab/layout_designer/yaze_widgets.h"

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {
namespace layout_designer {

const char* GetYazeWidgetTypeName(YazeWidgetType type) {
  switch (type) {
    case YazeWidgetType::ThemedButton:
      return "Themed Button";
    case YazeWidgetType::PrimaryButton:
      return "Primary Button";
    case YazeWidgetType::DangerButton:
      return "Danger Button";
    case YazeWidgetType::ThemedIconButton:
      return "Themed Icon Button";
    case YazeWidgetType::TransparentIconButton:
      return "Transparent Icon Button";
    case YazeWidgetType::BeginField:
      return "Begin Field";
    case YazeWidgetType::EndField:
      return "End Field";
    case YazeWidgetType::PropertyTable:
      return "Property Table";
    case YazeWidgetType::PropertyRow:
      return "Property Row";
    case YazeWidgetType::SectionHeader:
      return "Section Header";
    case YazeWidgetType::PaddedPanel:
      return "Padded Panel";
    case YazeWidgetType::TableWithTheming:
      return "Themed Table";
    case YazeWidgetType::CanvasPanel:
      return "Canvas Panel";
    case YazeWidgetType::AutoInputField:
      return "Auto Input Field";
    case YazeWidgetType::AutoButton:
      return "Auto Button";
    case YazeWidgetType::AutoCheckbox:
      return "Auto Checkbox";
    case YazeWidgetType::AutoInputText:
      return "Auto Input Text";
    case YazeWidgetType::PaletteColorButton:
      return "Palette Color Button";
    case YazeWidgetType::PanelHeader:
      return "Panel Header";
    default:
      return "Unknown Yaze Widget";
  }
}

const char* GetYazeWidgetTypeIcon(YazeWidgetType type) {
  switch (type) {
    case YazeWidgetType::ThemedButton:
    case YazeWidgetType::PrimaryButton:
    case YazeWidgetType::DangerButton:
    case YazeWidgetType::AutoButton:
      return ICON_MD_SMART_BUTTON;

    case YazeWidgetType::ThemedIconButton:
    case YazeWidgetType::TransparentIconButton:
      return ICON_MD_RADIO_BUTTON_UNCHECKED;

    case YazeWidgetType::PropertyTable:
    case YazeWidgetType::PropertyRow:
    case YazeWidgetType::TableWithTheming:
      return ICON_MD_TABLE_CHART;

    case YazeWidgetType::SectionHeader:
    case YazeWidgetType::PanelHeader:
      return ICON_MD_TITLE;

    case YazeWidgetType::CanvasPanel:
      return ICON_MD_DRAW;

    case YazeWidgetType::BeginField:
    case YazeWidgetType::EndField:
    case YazeWidgetType::AutoInputField:
    case YazeWidgetType::AutoInputText:
      return ICON_MD_INPUT;

    case YazeWidgetType::PaletteColorButton:
      return ICON_MD_PALETTE;

    default:
      return ICON_MD_WIDGETS;
  }
}

std::string GenerateYazeWidgetCode(YazeWidgetType yaze_type,
                                   const WidgetDefinition& widget,
                                   int indent_level) {
  std::string indent(indent_level * 2, ' ');
  std::string code;

  auto* label_prop = const_cast<WidgetDefinition&>(widget).GetProperty("label");
  std::string label = label_prop ? label_prop->string_value : "Widget";

  switch (yaze_type) {
    case YazeWidgetType::ThemedButton:
      code +=
          indent + absl::StrFormat("if (gui::ThemedButton(\"%s\")) {\n", label);
      if (!widget.callback_name.empty()) {
        code += indent + absl::StrFormat("  %s();\n", widget.callback_name);
      }
      code += indent + "}\n";
      break;

    case YazeWidgetType::PrimaryButton:
      code += indent +
              absl::StrFormat("if (gui::PrimaryButton(\"%s\")) {\n", label);
      if (!widget.callback_name.empty()) {
        code += indent + absl::StrFormat("  %s();\n", widget.callback_name);
      }
      code += indent + "}\n";
      break;

    case YazeWidgetType::DangerButton:
      code +=
          indent + absl::StrFormat("if (gui::DangerButton(\"%s\")) {\n", label);
      if (!widget.callback_name.empty()) {
        code += indent + absl::StrFormat("  %s();\n", widget.callback_name);
      }
      code += indent + "}\n";
      break;

    case YazeWidgetType::SectionHeader: {
      auto* icon_prop =
          const_cast<WidgetDefinition&>(widget).GetProperty("icon");
      std::string icon = icon_prop ? icon_prop->string_value : ICON_MD_LABEL;
      code += indent + absl::StrFormat("gui::SectionHeader(\"%s\", \"%s\");\n",
                                       icon, label);
      break;
    }

    case YazeWidgetType::PropertyTable:
      code += indent + "if (gui::BeginPropertyTable(\"props\")) {\n";
      code += indent + "  // Add property rows here\n";
      code += indent + "  gui::EndPropertyTable();\n";
      code += indent + "}\n";
      break;

    case YazeWidgetType::PropertyRow: {
      auto* value_prop =
          const_cast<WidgetDefinition&>(widget).GetProperty("value");
      std::string value = value_prop ? value_prop->string_value : "Value";
      code += indent + absl::StrFormat("gui::PropertyRow(\"%s\", \"%s\");\n",
                                       label, value);
      break;
    }

    case YazeWidgetType::TableWithTheming: {
      auto* columns_prop =
          const_cast<WidgetDefinition&>(widget).GetProperty("columns");
      int columns = columns_prop ? columns_prop->int_value : 2;
      code += indent +
              absl::StrFormat(
                  "if (gui::LayoutHelpers::BeginTableWithTheming(\"table\", "
                  "%d)) {\n",
                  columns);
      code += indent + "  // Table contents\n";
      code += indent + "  gui::LayoutHelpers::EndTableWithTheming();\n";
      code += indent + "}\n";
      break;
    }

    case YazeWidgetType::AutoButton:
      code +=
          indent + absl::StrFormat("if (gui::AutoButton(\"%s\")) {\n", label);
      if (!widget.callback_name.empty()) {
        code += indent + absl::StrFormat("  %s();\n", widget.callback_name);
      }
      code += indent + "}\n";
      break;

    case YazeWidgetType::CanvasPanel: {
      auto* size_prop =
          const_cast<WidgetDefinition&>(widget).GetProperty("size");
      ImVec2 size = size_prop ? size_prop->vec2_value : ImVec2(300, 200);
      code += indent + "ImVec2 canvas_size;\n";
      code +=
          indent +
          "gui::LayoutHelpers::BeginCanvasPanel(\"canvas\", &canvas_size);\n";
      code += indent + "// Custom drawing code here\n";
      code += indent + "gui::LayoutHelpers::EndCanvasPanel();\n";
      break;
    }

    case YazeWidgetType::PanelHeader: {
      auto* icon_prop =
          const_cast<WidgetDefinition&>(widget).GetProperty("icon");
      std::string icon = icon_prop ? icon_prop->string_value : ICON_MD_WINDOW;
      code += indent + absl::StrFormat("gui::PanelHeader(\"%s\", \"%s\");\n",
                                       label, icon);
      break;
    }

    default:
      code += indent + absl::StrFormat("// TODO: Yaze widget: %s\n",
                                       GetYazeWidgetTypeName(yaze_type));
      break;
  }

  return code;
}

std::vector<std::string> GetRequiredIncludes(YazeWidgetType type) {
  std::vector<std::string> includes;

  switch (type) {
    case YazeWidgetType::ThemedButton:
    case YazeWidgetType::PrimaryButton:
    case YazeWidgetType::DangerButton:
    case YazeWidgetType::ThemedIconButton:
    case YazeWidgetType::TransparentIconButton:
    case YazeWidgetType::PaletteColorButton:
    case YazeWidgetType::PanelHeader:
      includes.push_back("app/gui/widgets/themed_widgets.h");
      break;

    case YazeWidgetType::BeginField:
    case YazeWidgetType::EndField:
    case YazeWidgetType::PropertyTable:
    case YazeWidgetType::PropertyRow:
    case YazeWidgetType::SectionHeader:
      includes.push_back("app/gui/core/ui_helpers.h");
      break;

    case YazeWidgetType::PaddedPanel:
    case YazeWidgetType::TableWithTheming:
    case YazeWidgetType::CanvasPanel:
    case YazeWidgetType::AutoInputField:
      includes.push_back("app/gui/core/layout_helpers.h");
      break;

    case YazeWidgetType::AutoButton:
    case YazeWidgetType::AutoCheckbox:
    case YazeWidgetType::AutoInputText:
      includes.push_back("app/gui/automation/widget_auto_register.h");
      break;

    default:
      break;
  }

  return includes;
}

std::vector<WidgetProperty> GetYazeDefaultProperties(YazeWidgetType type) {
  std::vector<WidgetProperty> props;

  switch (type) {
    case YazeWidgetType::ThemedButton:
    case YazeWidgetType::PrimaryButton:
    case YazeWidgetType::DangerButton:
    case YazeWidgetType::AutoButton: {
      WidgetProperty label;
      label.name = "label";
      label.type = WidgetProperty::Type::String;
      label.string_value = "Button";
      props.push_back(label);
      break;
    }

    case YazeWidgetType::SectionHeader: {
      WidgetProperty label;
      label.name = "label";
      label.type = WidgetProperty::Type::String;
      label.string_value = "Section";
      props.push_back(label);

      WidgetProperty icon;
      icon.name = "icon";
      icon.type = WidgetProperty::Type::String;
      icon.string_value = ICON_MD_LABEL;
      props.push_back(icon);
      break;
    }

    case YazeWidgetType::PropertyTable: {
      WidgetProperty columns;
      columns.name = "columns";
      columns.type = WidgetProperty::Type::Int;
      columns.int_value = 2;
      props.push_back(columns);
      break;
    }

    case YazeWidgetType::PropertyRow: {
      WidgetProperty label;
      label.name = "label";
      label.type = WidgetProperty::Type::String;
      label.string_value = "Property";
      props.push_back(label);

      WidgetProperty value;
      value.name = "value";
      value.type = WidgetProperty::Type::String;
      value.string_value = "Value";
      props.push_back(value);
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
