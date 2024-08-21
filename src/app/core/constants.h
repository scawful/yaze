#ifndef YAZE_APP_CORE_CONSTANTS_H
#define YAZE_APP_CORE_CONSTANTS_H

#include <string_view>

#define TAB_BAR(w) if (ImGui::BeginTabBar(w)) {
#define END_TAB_BAR() \
  ImGui::EndTabBar(); \
  }

#define TAB_ITEM(w) if (ImGui::BeginTabItem(w)) {
#define END_TAB_ITEM() \
  ImGui::EndTabItem(); \
  }

#define MENU_ITEM(w) if (ImGui::MenuItem(w))
#define MENU_ITEM2(w, v) if (ImGui::MenuItem(w, v))

#define BUTTON_COLUMN(w)    \
  ImGui::TableNextColumn(); \
  ImGui::Button(w);

#define TEXT_COLUMN(w)      \
  ImGui::TableNextColumn(); \
  ImGui::Text(w);

#define BEGIN_TABLE(l, n, f) if (ImGui::BeginTable(l, n, f, ImVec2(0, 0))) {
#define SETUP_COLUMN(l) ImGui::TableSetupColumn(l);

#define TABLE_HEADERS()     \
  ImGui::TableHeadersRow(); \
  ImGui::TableNextRow();

#define NEXT_COLUMN() ImGui::TableNextColumn();

#define END_TABLE()  \
  ImGui::EndTable(); \
  }

#define HOVER_HINT(string) \
  if (ImGui::IsItemHovered()) ImGui::SetTooltip(string);

#define PRINT_IF_ERROR(expression)                \
  {                                               \
    auto error = expression;                      \
    if (!error.ok()) {                            \
      std::cout << error.ToString() << std::endl; \
    }                                             \
  }

#define EXIT_IF_ERROR(expression)                 \
  {                                               \
    auto error = expression;                      \
    if (!error.ok()) {                            \
      std::cout << error.ToString() << std::endl; \
      return EXIT_FAILURE;                        \
    }                                             \
  }

#define RETURN_VOID_IF_ERROR(expression)          \
  {                                               \
    auto error = expression;                      \
    if (!error.ok()) {                            \
      std::cout << error.ToString() << std::endl; \
      return;                                     \
    }                                             \
  }

#define RETURN_IF_ERROR(expression) \
  {                                 \
    auto error = expression;        \
    if (!error.ok()) {              \
      return error;                 \
    }                               \
  }

#define ASSIGN_OR_RETURN(type_variable_name, expression)         \
  ASSIGN_OR_RETURN_IMPL(APPEND_NUMBER(error_or_value, __LINE__), \
                        type_variable_name, expression)

#define ASSIGN_OR_RETURN_IMPL(error_or_value, type_variable_name, expression) \
  auto error_or_value = expression;                                           \
  if (!error_or_value.ok()) {                                                 \
    return error_or_value.status();                                           \
  }                                                                           \
  type_variable_name = std::move(*error_or_value);

#define ASSIGN_OR_LOG_ERROR(type_variable_name, expression)         \
  ASSIGN_OR_LOG_ERROR_IMPL(APPEND_NUMBER(error_or_value, __LINE__), \
                           type_variable_name, expression)

#define ASSIGN_OR_LOG_ERROR_IMPL(error_or_value, type_variable_name, \
                                 expression)                         \
  auto error_or_value = expression;                                  \
  if (!error_or_value.ok()) {                                        \
    std::cout << error_or_value.status().ToString() << std::endl;    \
  }                                                                  \
  type_variable_name = std::move(*error_or_value);

#define APPEND_NUMBER(expression, number) \
  APPEND_NUMBER_INNER(expression, number)

#define APPEND_NUMBER_INNER(expression, number) expression##number

#define TEXT_WITH_SEPARATOR(text) \
  ImGui::Text(text);              \
  ImGui::Separator();

#define TABLE_BORDERS_RESIZABLE \
  ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable

#define CLEAR_AND_RETURN_STATUS(status) \
  if (!status.ok()) {                   \
    auto temp = status;                 \
    status = absl::OkStatus();          \
    return temp;                        \
  }

using ushort = unsigned short;
using uint = unsigned int;
using uchar = unsigned char;

namespace yaze {
namespace app {
namespace core {

constexpr std::string_view kYazeVersion = "0.2.1";

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif