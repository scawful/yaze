
#include "app/gui/zeml.h"

#include <imgui/imgui.h>

#include <cctype>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "app/gui/canvas.h"
#include "app/gui/input.h"

namespace yaze {
namespace app {
namespace gui {
namespace zeml {

std::vector<Token> Tokenize(const std::string& input) {
  std::vector<Token> tokens;
  std::istringstream stream(input);
  char ch;

  while (stream.get(ch)) {
    if (isspace(ch)) continue;

    if (ch == '{') {
      tokens.push_back({TokenType::OpenBrace, "{"});
    } else if (ch == '}') {
      tokens.push_back({TokenType::CloseBrace, "}"});
    } else if (ch == ',') {
      tokens.push_back({TokenType::Comma, ","});
    } else if (std::isalnum(ch) || ch == '_') {
      std::string ident(1, ch);
      while (stream.get(ch) && (std::isalnum(ch) || ch == '_')) {
        ident += ch;
      }
      stream.unget();
      tokens.push_back({TokenType::Identifier, ident});
    } else if (ch == '"' || ch == '\'') {
      std::string str;
      char quoteType = ch;
      while (stream.get(ch) && ch != quoteType) {
        str += ch;
      }
      tokens.push_back({TokenType::String, str});
    }
  }

  tokens.push_back({TokenType::EndOfStream, ""});
  return tokens;
}

WidgetType MapType(const std::string& type) {
  static std::map<std::string, WidgetType> typeMap = {
      {"Window", WidgetType::Window},
      {"Button", WidgetType::Button},
      {"Slider", WidgetType::Slider},
      {"Text", WidgetType::Text},
      {"CollapsingHeader", WidgetType::CollapsingHeader},
      {"Columns", WidgetType::Columns},
      {"Checkbox", WidgetType::Checkbox},
      {"HexInputByte", WidgetType::HexInputByte},
      {"HexInputWord", WidgetType::HexInputWord},
      {"Table", WidgetType::Table},
      {"Selectable", WidgetType::Selectable},
      {"TableSetupColumn", WidgetType::TableSetupColumn},
      {"TableHeadersRow", WidgetType::TableHeadersRow},
      {"TableNextColumn", WidgetType::TableNextColumn},
      {"Function", WidgetType::Function},
      {"BeginChild", WidgetType::BeginChild},
      {"BeginMenu", WidgetType::BeginMenu},
      {"MenuItem", WidgetType::MenuItem},
      {"BeginMenuBar", WidgetType::BeginMenuBar},
      {"Separator", WidgetType::Separator},
      {"BeginTabBar", WidgetType::BeginTabBar},
      {"BeginTabItem", WidgetType::BeginTabItem},
      {"Canvas", WidgetType::Canvas},
      {"ref", WidgetType::Definition},
  };
  return typeMap[type];
}

Node ParseNode(const std::vector<Token>& tokens, size_t& index,
               const std::map<std::string, void*>& data_bindings,
               const std::map<std::string, Node>& definitions) {
  Node node;
  if (index >= tokens.size() || tokens[index].type == TokenType::EndOfStream) {
    return node;
  }

  while (index < tokens.size() &&
         tokens[index].type != TokenType::EndOfStream) {
    Token token = tokens[index];
    if (token.type == TokenType::Identifier) {
      node.type = MapType(token.value);
      index++;  // Move to the next token for attributes
      if (node.type == WidgetType::Definition) {
        if (definitions.find(token.value) != definitions.end()) {
          node = definitions.at(token.value);
        }
      } else {
        node.attributes =
            ParseAttributes(tokens, index, node.type, data_bindings);
      }
    }

    // Handle the opening brace indicating the start of child nodes
    if (index < tokens.size() && tokens[index].type == TokenType::OpenBrace) {
      index++;  // Skip the opening brace

      while (index < tokens.size() &&
             tokens[index].type != TokenType::CloseBrace) {
        if (tokens[index].type == TokenType::Comma) {
          index++;  // Skip commas
        } else {
          node.children.push_back(ParseNode(tokens, index, data_bindings));
        }
      }

      if (index < tokens.size() &&
          tokens[index].type == TokenType::CloseBrace) {
        index++;  // Ensure closing brace is skipped before returning
      }
    }

    break;  // Exit after processing one complete node
  }
  return node;
}

void ParseFlags(const WidgetType& type, const std::string& flags,
                WidgetAttributes& attributes) {
  // Parse the flags for the `|` character
  std::vector<std::string> flag_tokens;
  std::string token;
  std::istringstream tokenStream(flags);
  while (std::getline(tokenStream, token, '|')) {
    flag_tokens.push_back(token);
  }

  switch (type) {
    case WidgetType::BeginChild: {
      static std::map<std::string, ImGuiWindowFlags> flagMap = {
          {"None", ImGuiWindowFlags_None},
          {"NoTitleBar", ImGuiWindowFlags_NoTitleBar},
          {"NoResize", ImGuiWindowFlags_NoResize},
          {"NoMove", ImGuiWindowFlags_NoMove},
          {"NoScrollbar", ImGuiWindowFlags_NoScrollbar},
          {"NoScrollWithMouse", ImGuiWindowFlags_NoScrollWithMouse},
          {"NoCollapse", ImGuiWindowFlags_NoCollapse},
          {"AlwaysAutoResize", ImGuiWindowFlags_AlwaysAutoResize},
          {"NoSavedSettings", ImGuiWindowFlags_NoSavedSettings},
          {"NoInputs", ImGuiWindowFlags_NoInputs},
          {"MenuBar", ImGuiWindowFlags_MenuBar},
          {"HorizontalScrollbar", ImGuiWindowFlags_HorizontalScrollbar},
          {"NoFocusOnAppearing", ImGuiWindowFlags_NoFocusOnAppearing},
          {"NoBringToFrontOnFocus", ImGuiWindowFlags_NoBringToFrontOnFocus},
          {"AlwaysVerticalScrollbar", ImGuiWindowFlags_AlwaysVerticalScrollbar},
          {"AlwaysHorizontalScrollbar",
           ImGuiWindowFlags_AlwaysHorizontalScrollbar},
          {"AlwaysUseWindowPadding", ImGuiWindowFlags_AlwaysUseWindowPadding},
          {"NoNavInputs", ImGuiWindowFlags_NoNavInputs},
          {"NoNavFocus", ImGuiWindowFlags_NoNavFocus},
          {"UnsavedDocument", ImGuiWindowFlags_UnsavedDocument},
          {"NoNav", ImGuiWindowFlags_NoNav},
          {"NoDecoration", ImGuiWindowFlags_NoDecoration},
          {"NoInputs", ImGuiWindowFlags_NoInputs},
          {"NoFocusOnAppearing", ImGuiWindowFlags_NoFocusOnAppearing},
          {"NoBringToFrontOnFocus", ImGuiWindowFlags_NoBringToFrontOnFocus},
          {"AlwaysAutoResize", ImGuiWindowFlags_AlwaysAutoResize},
          {"NoSavedSettings", ImGuiWindowFlags_NoSavedSettings},
          {"NoMouseInputs", ImGuiWindowFlags_NoMouseInputs},
          {"NoMouseInputs", ImGuiWindowFlags_NoMouseInputs},
          {"NoTitleBar", ImGuiWindowFlags_NoTitleBar},
          {"NoResize", ImGuiWindowFlags_NoResize},
          {"NoMove", ImGuiWindowFlags_NoMove},
          {"NoScrollbar", ImGuiWindowFlags_NoScrollbar},
          {"NoScrollWithMouse", ImGuiWindowFlags_NoScrollWithMouse},
          {"NoCollapse", ImGuiWindowFlags_NoCollapse},
          {"AlwaysVerticalScrollbar", ImGuiWindowFlags_AlwaysVerticalScrollbar},
          {"AlwaysHorizontalScrollbar",
           ImGuiWindowFlags_AlwaysHorizontalScrollbar}};
      ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
      for (const auto& flag : flag_tokens) {
        if (flagMap.find(flag) != flagMap.end()) {
          windowFlags |= flagMap[flag];
        }
      }
      attributes.flags = std::make_unique<ImGuiWindowFlags>(windowFlags);
      break;
    }
    case WidgetType::CollapsingHeader: {
      // Create a flag map using the tree node flags
      static std::map<std::string, ImGuiTreeNodeFlags> flagMap = {
          {"None", ImGuiTreeNodeFlags_None},
          {"Selected", ImGuiTreeNodeFlags_Selected},
          {"Framed", ImGuiTreeNodeFlags_Framed},
          {"AllowItemOverlap", ImGuiTreeNodeFlags_AllowItemOverlap},
          {"NoTreePushOnOpen", ImGuiTreeNodeFlags_NoTreePushOnOpen},
          {"NoAutoOpenOnLog", ImGuiTreeNodeFlags_NoAutoOpenOnLog},
          {"DefaultOpen", ImGuiTreeNodeFlags_DefaultOpen},
          {"OpenOnDoubleClick", ImGuiTreeNodeFlags_OpenOnDoubleClick},
          {"OpenOnArrow", ImGuiTreeNodeFlags_OpenOnArrow},
          {"Leaf", ImGuiTreeNodeFlags_Leaf},
          {"Bullet", ImGuiTreeNodeFlags_Bullet},
          {"FramePadding", ImGuiTreeNodeFlags_FramePadding},
          {"NavLeftJumpsBackHere", ImGuiTreeNodeFlags_NavLeftJumpsBackHere},
          {"CollapsingHeader", ImGuiTreeNodeFlags_CollapsingHeader}};
      ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_None;
      for (const auto& flag : flag_tokens) {
        if (flagMap.find(flag) != flagMap.end()) {
          treeFlags |= flagMap[flag];
        }
      }
      attributes.flags = std::make_unique<ImGuiTreeNodeFlags>(treeFlags);
      break;
    }
    case WidgetType::Table: {
      // Create a flag map
      static std::map<std::string, ImGuiTableFlags> flagMap = {
          {"None", ImGuiTableFlags_None},
          {"Resizable", ImGuiTableFlags_Resizable},
          {"Reorderable", ImGuiTableFlags_Reorderable},
          {"Hideable", ImGuiTableFlags_Hideable},
          {"Sortable", ImGuiTableFlags_Sortable},
          {"NoSavedSettings", ImGuiTableFlags_NoSavedSettings},
          {"ContextMenuInBody", ImGuiTableFlags_ContextMenuInBody},
          {"RowBg", ImGuiTableFlags_RowBg},
          {"BordersInnerH", ImGuiTableFlags_BordersInnerH},
          {"BordersOuterH", ImGuiTableFlags_BordersOuterH},
          {"BordersInnerV", ImGuiTableFlags_BordersInnerV},
          {"BordersOuterV", ImGuiTableFlags_BordersOuterV},
          {"BordersH", ImGuiTableFlags_BordersH},
          {"BordersV", ImGuiTableFlags_BordersV},
          {"Borders", ImGuiTableFlags_Borders},
          {"NoBordersInBody", ImGuiTableFlags_NoBordersInBody},
          {"NoBordersInBodyUntilResize",
           ImGuiTableFlags_NoBordersInBodyUntilResize},
          {"SizingFixedFit", ImGuiTableFlags_SizingFixedFit},
          {"SizingFixedSame", ImGuiTableFlags_SizingFixedSame},
          {"SizingStretchProp", ImGuiTableFlags_SizingStretchProp},
          {"SizingStretchSame", ImGuiTableFlags_SizingStretchSame},
          {"NoHostExtendX", ImGuiTableFlags_NoHostExtendX},
          {"NoHostExtendY", ImGuiTableFlags_NoHostExtendY},
          {"NoKeepColumnsVisible", ImGuiTableFlags_NoKeepColumnsVisible},
          {"PreciseWidths", ImGuiTableFlags_PreciseWidths},
          {"NoClip", ImGuiTableFlags_NoClip},
          {"PadOuterX", ImGuiTableFlags_PadOuterX},
          {"NoPadOuterX", ImGuiTableFlags_NoPadOuterX},
          {"NoPadInnerX", ImGuiTableFlags_NoPadInnerX},
          {"ScrollX", ImGuiTableFlags_ScrollX},
          {"ScrollY", ImGuiTableFlags_ScrollY},
          {"SortMulti", ImGuiTableFlags_SortMulti},
          {"SortTristate", ImGuiTableFlags_SortTristate}};
      ImGuiTableFlags tableFlags = ImGuiTableFlags_None;
      for (const auto& flag : flag_tokens) {
        if (flagMap.find(flag) != flagMap.end()) {
          tableFlags |= flagMap[flag];
        }
      }
      // Reserve data to the void* pointer and assign flags
      attributes.flags = std::make_unique<ImGuiTableFlags>(tableFlags);
    } break;
    case WidgetType::TableSetupColumn: {
      static std::map<std::string, ImGuiTableColumnFlags> flagMap = {
          {"None", ImGuiTableColumnFlags_None},
          {"DefaultHide", ImGuiTableColumnFlags_DefaultHide},
          {"DefaultSort", ImGuiTableColumnFlags_DefaultSort},
          {"WidthStretch", ImGuiTableColumnFlags_WidthStretch},
          {"WidthFixed", ImGuiTableColumnFlags_WidthFixed},
          {"NoResize", ImGuiTableColumnFlags_NoResize},
          {"NoReorder", ImGuiTableColumnFlags_NoReorder},
          {"NoHide", ImGuiTableColumnFlags_NoHide},
          {"NoClip", ImGuiTableColumnFlags_NoClip},
          {"NoSort", ImGuiTableColumnFlags_NoSort},
          {"NoSortAscending", ImGuiTableColumnFlags_NoSortAscending},
          {"NoSortDescending", ImGuiTableColumnFlags_NoSortDescending},
          {"NoHeaderWidth", ImGuiTableColumnFlags_NoHeaderWidth},
          {"PreferSortAscending", ImGuiTableColumnFlags_PreferSortAscending},
          {"PreferSortDescending", ImGuiTableColumnFlags_PreferSortDescending},
          {"IndentEnable", ImGuiTableColumnFlags_IndentEnable},
          {"IndentDisable", ImGuiTableColumnFlags_IndentDisable},
          {"IsEnabled", ImGuiTableColumnFlags_IsEnabled},
          {"IsVisible", ImGuiTableColumnFlags_IsVisible},
          {"IsSorted", ImGuiTableColumnFlags_IsSorted},
          {"IsHovered", ImGuiTableColumnFlags_IsHovered}};
      ImGuiTableColumnFlags columnFlags = ImGuiTableColumnFlags_None;
      for (const auto& flag : flag_tokens) {
        if (flagMap.find(flag) != flagMap.end()) {
          columnFlags |= flagMap[flag];
        }
      }
      // Reserve data to the void* pointer and assign flags
      attributes.flags = std::make_unique<ImGuiTableColumnFlags>(columnFlags);
    }
    default:
      break;
  }
}

WidgetAttributes ParseAttributes(
    const std::vector<Token>& tokens, size_t& index, const WidgetType& type,
    const std::map<std::string, void*>& data_bindings) {
  WidgetAttributes attributes;

  while (index < tokens.size() && tokens[index].type != TokenType::CloseBrace) {
    if (tokens[index].type == TokenType::Identifier) {
      Token keyToken = tokens[index];
      index++;  // Move to the value token.
      if (index < tokens.size() && tokens[index].type == TokenType::String) {
        std::string value = tokens[index].value;
        index++;  // Move past the value.

        if (keyToken.value == "id")
          attributes.id = value;
        else if (keyToken.value == "title")
          attributes.title = value;
        else if (keyToken.value == "min")
          attributes.min = std::stod(value);
        else if (keyToken.value == "max")
          attributes.max = std::stod(value);
        else if (keyToken.value == "value")
          attributes.value = std::stod(value);
        else if (keyToken.value == "width")
          if (value == "autox")
            attributes.width = ImGui::GetContentRegionAvail().x;
          else if (value == "autoy")
            attributes.width = ImGui::GetContentRegionAvail().y;
          else
            attributes.width = std::stod(value);
        else if (keyToken.value == "text")
          attributes.text = value;
        else if (keyToken.value == "data" &&
                 data_bindings.find(value) != data_bindings.end()) {
          attributes.data = data_bindings.at(value);
        } else if (keyToken.value == "count") {
          attributes.count = std::stoi(value);
        } else if (keyToken.value == "flags") {
          ParseFlags(type, value, attributes);
        } else if (keyToken.value == "size") {
          std::string sizeX, sizeY;
          std::istringstream sizeStream(value);
          std::getline(sizeStream, sizeX, ',');
          std::getline(sizeStream, sizeY, ',');
          attributes.size = ImVec2(std::stod(sizeX), std::stod(sizeY));
        }
      }
    } else {
      // If it's not an identifier or we encounter an open brace, break out.
      break;
    }
  }
  return attributes;
}

Node Parse(const std::string& yazon_input,
           const std::map<std::string, void*>& data_bindings) {
  size_t index = 0;
  auto tokens = Tokenize(yazon_input);

  std::map<std::string, Node> definitions;
  if (tokens[index].value == "Definitions") {
    index++;  // Skip the "Definitions" token
    while (index < tokens.size() &&
           tokens[index].value != "Layout") {  // Skip the definitions
      // Get the definition name and parse the node
      std::string definition_name = tokens[index].value;
      index++;  // Move to the definition node
      definitions[definition_name] = ParseNode(tokens, index, data_bindings);
      index++;
    }
  }

  return ParseNode(tokens, index, data_bindings);
}

void Render(Node& node) {
  switch (node.type) {
    case WidgetType::Window: {
      ImGuiWindowFlags flags = ImGuiWindowFlags_None;
      if (node.attributes.flags) {
        flags = *(ImGuiWindowFlags*)node.attributes.flags.get();
      }
      if (ImGui::Begin(node.attributes.title.c_str(), nullptr, flags)) {
        for (auto& child : node.children) {
          Render(child);
        }
        ImGui::End();
      }
    } break;
    case WidgetType::Button:
      if (node.attributes.data) {
        // Format the text with the data value
        char formattedText[256];
        snprintf(formattedText, sizeof(formattedText),
                 node.attributes.text.c_str(), *(int*)node.attributes.data);
        if (ImGui::Button(formattedText)) {
          ExecuteActions(node.actions, ActionType::Click);
        }
      } else {
        if (ImGui::Button(node.attributes.text.c_str())) {
          ExecuteActions(node.actions, ActionType::Click);
        }
      }
      break;
    case WidgetType::CollapsingHeader: {
      ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
      if (node.attributes.flags) {
        flags = *(ImGuiTreeNodeFlags*)node.attributes.flags.get();
      }
      if (ImGui::CollapsingHeader(node.attributes.title.c_str(), flags)) {
        for (auto& child : node.children) {
          Render(child);
        }
      }
    } break;
    case WidgetType::Columns:
      ImGui::Columns(node.attributes.count, node.attributes.title.c_str());
      ImGui::Separator();
      for (auto& child : node.children) {
        Render(child);
        ImGui::NextColumn();
      }
      ImGui::Columns(1);
      ImGui::Separator();
      break;
    case WidgetType::Checkbox:
      if (ImGui::Checkbox(node.attributes.title.c_str(),
                          (bool*)node.attributes.data)) {
        ExecuteActions(node.actions, ActionType::Change);
      }
      break;
    case WidgetType::Table: {
      ImGuiTableFlags flags = ImGuiTableFlags_None;
      if (node.attributes.flags) {
        flags = *(ImGuiTableFlags*)node.attributes.flags.get();
      }
      if (ImGui::BeginTable(node.attributes.id.c_str(), node.attributes.count,
                            flags)) {
        for (auto& child : node.children) {
          Render(child);
        }
      }
      ImGui::EndTable();
    } break;
    case WidgetType::TableSetupColumn: {
      ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;
      if (node.attributes.flags) {
        flags = *(ImGuiTableColumnFlags*)node.attributes.flags.get();
      }
      ImGui::TableSetupColumn(node.attributes.title.c_str(), flags);
    } break;
    case WidgetType::TableHeadersRow:
      ImGui::TableHeadersRow();
      break;
    case WidgetType::TableNextColumn:
      ImGui::TableNextColumn();
      break;
    case WidgetType::Text:
      if (node.attributes.data) {
        // Assuming all data-bound Text widgets use string formatting
        char formattedText[256];
        snprintf(formattedText, sizeof(formattedText),
                 node.attributes.text.c_str(), *(int*)node.attributes.data);
        ImGui::Text("%s", formattedText);
      } else {
        ImGui::Text("%s", node.attributes.text.c_str());
      }
      break;
    case WidgetType::Function: {
      node.actions[0].callback();
      break;
    }
    case WidgetType::BeginChild:
      if (ImGui::BeginChild(node.attributes.id.c_str(), node.attributes.size)) {
        for (auto& child : node.children) {
          Render(child);
        }
        ImGui::EndChild();
      }
      break;
    case WidgetType::BeginMenuBar:
      if (ImGui::BeginMenuBar()) {
        for (auto& child : node.children) {
          Render(child);
        }
        ImGui::EndMenuBar();
      }
      break;
    case WidgetType::BeginMenu: {
      if (ImGui::BeginMenu(node.attributes.title.c_str())) {
        for (auto& child : node.children) {
          Render(child);
        }
        ImGui::EndMenu();
      }
      break;
    }
    case WidgetType::MenuItem: {
      if (ImGui::MenuItem(node.attributes.title.c_str())) {
        ExecuteActions(node.actions, ActionType::Click);
      }
      break;
    }
    case WidgetType::Separator:
      ImGui::Separator();
      break;
    case WidgetType::Selectable:
      if (ImGui::Selectable(node.attributes.title.c_str(),
                            (bool*)node.attributes.selected)) {
        ExecuteActions(node.actions, ActionType::Click);
      }
      break;
    case WidgetType::BeginTabBar:
      if (ImGui::BeginTabBar(node.attributes.title.c_str())) {
        for (auto& child : node.children) {
          Render(child);
        }
        ImGui::EndTabBar();
      }
      break;
    case WidgetType::BeginTabItem:
      if (ImGui::BeginTabItem(node.attributes.title.c_str())) {
        for (auto& child : node.children) {
          Render(child);
        }
        ImGui::EndTabItem();
      }
      break;
    case WidgetType::HexInputByte:
      gui::InputHexByte(node.attributes.id.c_str(),
                        (uint8_t*)node.attributes.data);
      break;
    case WidgetType::HexInputWord:
      gui::InputHexWord(node.attributes.id.c_str(),
                        (uint16_t*)node.attributes.data);
      break;
    case WidgetType::Canvas: {
      gui::Canvas* canvas = (gui::Canvas*)node.attributes.data;
      if (canvas) {
        canvas->DrawBackground();
        canvas->DrawContextMenu();

        canvas->DrawGrid();
        canvas->DrawOverlay();
      }
      break;
    }
    default:
      break;
  }
}

void ExecuteActions(const std::vector<Action>& actions, ActionType type) {
  for (const auto& action : actions) {
    if (action.type == type) {
      action.callback();  // Execute the callback associated with the action
    }
  }
}

void Bind(Node* node, std::function<void()> callback) {
  if (node) {
    Action action = {ActionType::Click, callback};
    node->actions.push_back(action);
  }
}

void BindAction(Node* node, ActionType type, std::function<void()> callback) {
  if (node) {
    Action action = {type, callback};
    node->actions.push_back(action);
  }
}

void BindSelectable(Node* node, bool* selected,
                    std::function<void()> callback) {
  if (node) {
    Action action = {ActionType::Click, callback};
    node->actions.push_back(action);
    node->attributes.selected = selected;
  }
}

std::string LoadFile(const std::string& filename) {
  std::string fileContents;
  const std::string kPath = "assets/layouts/";
  std::ifstream file(kPath + filename);

  if (file.is_open()) {
    std::string line;
    while (std::getline(file, line)) {
      fileContents += line;
    }
    file.close();
  } else {
    fileContents = "File not found: " + filename;
    std::cout << fileContents << std::endl;
  }
  return fileContents;
}

}  // namespace zeml
}  // namespace gui
}  // namespace app
}  // namespace yaze
