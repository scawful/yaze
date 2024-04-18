
#include "app/gui/zeml.h"

#include <imgui/imgui.h>

#include <cctype>
#include <map>
#include <sstream>
#include <string>
#include <vector>

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
  };
  return typeMap[type];
}

Node ParseNode(const std::vector<Token>& tokens, size_t& index,
           const std::map<std::string, void*>& data_bindings) {
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
      node.attributes =
          ParseAttributes(tokens, index, node.type, data_bindings);
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
        else if (keyToken.value == "text")
          attributes.text = value;
        else if (keyToken.value == "data" &&
                 data_bindings.find(value) != data_bindings.end()) {
          attributes.data = data_bindings.at(value);
        } else if (keyToken.value == "count") {
          attributes.count = std::stoi(value);
        } else if (keyToken.value == "flags") {
          attributes.flags = nullptr;  // Placeholder for future use
        } else if (keyToken.value == "size") {
          attributes.size = ImVec2(0, 0);  // Placeholder for future use
        }
      }
    } else {
      // If it's not an identifier or we encounter an open brace, break out.
      break;
    }
  }
  return attributes;
}

void Render(Node& node) {
  switch (node.type) {
    case WidgetType::Window:
      if (ImGui::Begin(node.attributes.title.c_str())) {
        for (auto& child : node.children) {
          Render(child);
        }
        ImGui::End();
      }
      break;
    case WidgetType::Button:
      if (ImGui::Button(node.attributes.text.c_str())) {
        ExecuteActions(node.actions, ActionType::Click);
      }
      break;
    case WidgetType::CollapsingHeader:
      if (ImGui::CollapsingHeader(node.attributes.title.c_str())) {
        for (auto& child : node.children) {
          Render(child);
        }
      }
      break;
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
    case WidgetType::Table:
      ImGui::BeginTable(node.attributes.id.c_str(), node.attributes.count);
      for (auto& child : node.children) {
        Render(child);
      }
      ImGui::EndTable();
      break;
    case WidgetType::TableSetupColumn:
      ImGui::TableSetupColumn(node.attributes.title.c_str());
      break;
    case WidgetType::TableHeadersRow:
      ImGui::TableHeadersRow();
      break;
    case WidgetType::TableNextColumn:
      ImGui::TableNextColumn();
      for (auto& child : node.children) {
        Render(child);
      }
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
    default:
      break;
  }
}

Node Parse(const std::string& yazon_input,
               const std::map<std::string, void*>& data_bindings) {
  size_t index = 0;
  auto tokens = Tokenize(yazon_input);
  return ParseNode(tokens, index, data_bindings);
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
    node->actions.push_back({ActionType::Click, callback});
  }
}

void BindAction(Node* node, ActionType type, std::function<void()> callback) {
  if (node) {
    node->actions.push_back({type, callback});
  }
}

}  // namespace zeml
}  // namespace gui
}  // namespace app
}  // namespace yaze
