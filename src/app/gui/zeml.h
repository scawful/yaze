#ifndef YAZE_APP_GUI_ZEML_H
#define YAZE_APP_GUI_ZEML_H

#include <imgui/imgui.h>

#include <cctype>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace yaze {
namespace app {
namespace gui {

/**
 * @namespace yaze::app::gui::zeml
 * @brief Zelda Editor Markup Language Functions
 */
namespace zeml {

/**
 * @enum TokenType
 */
enum class TokenType {
  Identifier,
  String,
  OpenBrace,
  CloseBrace,
  Comma,
  EndOfStream
};

/**
 * @struct Token
 */
struct Token {
  TokenType type;
  std::string value;
} typedef Token;

/**
 * @enum WidgetType
 */
enum class WidgetType {
  Window,
  Button,
  Slider,
  Text,
  Table,
  TableSetupColumn,
  TableHeadersRow,
  TableNextColumn,
  CollapsingHeader,
  Columns,
  Selectable,
  Function,
  BeginChild,
  BeginMenuBar,
  BeginMenu,
  MenuItem,
  Separator,
  HexInputByte,
  HexInputWord,
};

/**
 * @struct WidgetAttributes
 * @brief Attributes for a widget
 * @details id, title, min, max, value, text, count, size, flags, data
 * @details id: unique identifier for the widget
 * @details title: title for the widget
 * @details text: text for the widget
 * @details min: minimum value for the widget
 * @details max: maximum value for the widget
 * @details value: value for the widget
 * @details count: number of columns
 * @details size: size of the widget
 * @details flags: flags for the widget
 * @details data: data to be binded using the data_binding map
 */
struct WidgetAttributes {
  std::string id;
  std::string title;  // For Window, Button
  double min;         // For Slider
  double max;         // For Slider
  double value;       // For Slidecar
  std::string text;   // For Text, Button
  int count = 0;      // For Columns
  ImVec2 size = ImVec2(0, 0);
  void* flags = nullptr;

  void* data = nullptr;
};

/**
 * @enum ActionType
 */
enum class ActionType { Click, Change, Run };

/**
 * @struct Action
 */
struct Action {
  ActionType type;
  std::function<void()> callback;  // Using std::function to hold lambda
                                   // expressions or function pointers
};

/**
 * @brief Tokenize a zeml string
 */
std::vector<Token> Tokenize(const std::string& input);

/**
 * @struct Node
 * @brief Node for a zeml tree
 */
struct Node {
  WidgetType type;
  WidgetAttributes attributes;
  std::vector<Action> actions;
  std::vector<Node> children;

  Node* parent = nullptr;

  Node* GetNode(const std::string& searchId) {
    if (attributes.id == searchId) {
      return this;
    }
    for (Node& child : children) {
      Node* found = child.GetNode(searchId);
      if (found != nullptr) {
        return found;
      }
    }
    return nullptr;
  }
};

/**
 * @brief Bind a callback to a node
 */
void Bind(Node* node, std::function<void()> callback);

/**
 * @brief Bind an action to a node
 */
void BindAction(Node* node, ActionType type, std::function<void()> callback);

/**
 * @brief Map a string to a widget type
 */
WidgetType MapType(const std::string& type);

/**
 * @brief ParseNode attributes for a widget
 */
WidgetAttributes ParseAttributes(
    const std::vector<Token>& tokens, size_t& index, const WidgetType& type,
    const std::map<std::string, void*>& data_bindings = {});

/**
 * @brief ParseNode a zeml node
 */
Node ParseNode(const std::vector<Token>& tokens, size_t& index,
               const std::map<std::string, void*>& data_bindings = {});

/**
 * @brief ParseNode a zeml string
 */
Node Parse(const std::string& yazon_input,
           const std::map<std::string, void*>& data_bindings = {});

/**
 * @brief Render a zeml tree
 */
void Render(Node& node);

/**
 * @brief Execute actions for a node
 */
void ExecuteActions(const std::vector<Action>& actions, ActionType type);

}  // namespace zeml
}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GUI_YAZON_H_
