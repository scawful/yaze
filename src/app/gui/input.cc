#include "input.h"

#include <functional>
#include <string>
#include <variant>

#include "absl/strings/string_view.h"
#include "app/gfx/snes_tile.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui_memory_editor.h"

template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace ImGui {

static inline ImGuiInputTextFlags InputScalar_DefaultCharsFilter(
    ImGuiDataType data_type, const char* format) {
  if (data_type == ImGuiDataType_Float || data_type == ImGuiDataType_Double)
    return ImGuiInputTextFlags_CharsScientific;
  const char format_last_char = format[0] ? format[strlen(format) - 1] : 0;
  return (format_last_char == 'x' || format_last_char == 'X')
             ? ImGuiInputTextFlags_CharsHexadecimal
             : ImGuiInputTextFlags_CharsDecimal;
}
bool InputScalarLeft(const char* label, ImGuiDataType data_type, void* p_data,
                     const void* p_step, const void* p_step_fast,
                     const char* format, float input_width,
                     ImGuiInputTextFlags flags, bool no_step = false) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems) return false;

  ImGuiContext& g = *GImGui;
  ImGuiStyle& style = g.Style;

  if (format == NULL) format = DataTypeGetInfo(data_type)->PrintFmt;

  char buf[64];
  DataTypeFormatString(buf, IM_ARRAYSIZE(buf), data_type, p_data, format);

  if (g.ActiveId == 0 && (flags & (ImGuiInputTextFlags_CharsDecimal |
                                   ImGuiInputTextFlags_CharsHexadecimal |
                                   ImGuiInputTextFlags_CharsScientific)) == 0)
    flags |= InputScalar_DefaultCharsFilter(data_type, format);
  flags |= ImGuiInputTextFlags_AutoSelectAll;

  bool value_changed = false;
  // if (p_step == NULL) {
  //   ImGui::SetNextItemWidth(input_width);
  //   if (InputText("", buf, IM_ARRAYSIZE(buf), flags))
  //     value_changed = DataTypeApplyFromText(buf, data_type, p_data, format);
  // } else {
  const float button_size = GetFrameHeight();
  AlignTextToFramePadding();
  Text("%s", label);
  SameLine();
  BeginGroup();  // The only purpose of the group here is to allow the caller
                 // to query item data e.g. IsItemActive()
  PushID(label);
  SetNextItemWidth(ImMax(
      1.0f, CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * 2));

  // Place the label on the left of the input field
  PushStyleVar(ImGuiStyleVar_ItemSpacing,
               ImVec2{style.ItemSpacing.x, style.ItemSpacing.y});
  PushStyleVar(ImGuiStyleVar_FramePadding,
               ImVec2{style.FramePadding.x, style.FramePadding.y});

  SetNextItemWidth(input_width);
  if (InputText("", buf, IM_ARRAYSIZE(buf),
                flags))  // PushId(label) + "" gives us the expected ID
                         // from outside point of view
    value_changed = DataTypeApplyFromText(buf, data_type, p_data, format);
  IMGUI_TEST_ENGINE_ITEM_INFO(
      g.LastItemData.ID, label,
      g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Inputable);

  // Mouse wheel support
  if (IsItemHovered() && g.IO.MouseWheel != 0.0f) {
    float scroll_amount = g.IO.MouseWheel;
    float scroll_speed = 0.25f;  // Adjust the scroll speed as needed

    if (g.IO.KeyCtrl && p_step_fast)
      scroll_amount *= *(const float*)p_step_fast;
    else
      scroll_amount *= *(const float*)p_step;

    if (scroll_amount > 0.0f) {
      scroll_amount *= scroll_speed;  // Adjust the scroll speed as needed
      DataTypeApplyOp(data_type, '+', p_data, p_data, &scroll_amount);
      value_changed = true;
    } else if (scroll_amount < 0.0f) {
      scroll_amount *= -scroll_speed;  // Adjust the scroll speed as needed
      DataTypeApplyOp(data_type, '-', p_data, p_data, &scroll_amount);
      value_changed = true;
    }
  }

  // Step buttons
  if (!no_step) {
    const ImVec2 backup_frame_padding = style.FramePadding;
    style.FramePadding.x = style.FramePadding.y;
    ImGuiButtonFlags button_flags = ImGuiButtonFlags_PressedOnClick;
    if (flags & ImGuiInputTextFlags_ReadOnly) BeginDisabled();
    SameLine(0, style.ItemInnerSpacing.x);
    if (ButtonEx("-", ImVec2(button_size, button_size), button_flags)) {
      DataTypeApplyOp(data_type, '-', p_data, p_data,
                      g.IO.KeyCtrl && p_step_fast ? p_step_fast : p_step);
      value_changed = true;
    }
    SameLine(0, style.ItemInnerSpacing.x);
    if (ButtonEx("+", ImVec2(button_size, button_size), button_flags)) {
      DataTypeApplyOp(data_type, '+', p_data, p_data,
                      g.IO.KeyCtrl && p_step_fast ? p_step_fast : p_step);
      value_changed = true;
    }

    if (flags & ImGuiInputTextFlags_ReadOnly) EndDisabled();

    style.FramePadding = backup_frame_padding;
  }
  PopID();
  EndGroup();
  ImGui::PopStyleVar(2);

  if (value_changed) MarkItemEdited(g.LastItemData.ID);

  return value_changed;
}
}  // namespace ImGui

namespace yaze {
namespace gui {

const int kStepOneHex = 0x01;
const int kStepFastHex = 0x0F;

bool InputHex(const char* label, uint64_t* data) {
  return ImGui::InputScalar(label, ImGuiDataType_U64, data, &kStepOneHex,
                            &kStepFastHex, "%06X",
                            ImGuiInputTextFlags_CharsHexadecimal);
}

bool InputHex(const char* label, int* data, int num_digits, float input_width) {
  const std::string format = "%0" + std::to_string(num_digits) + "X";
  return ImGui::InputScalarLeft(label, ImGuiDataType_S32, data, &kStepOneHex,
                                &kStepFastHex, format.c_str(), input_width,
                                ImGuiInputTextFlags_CharsHexadecimal);
}

bool InputHexShort(const char* label, uint32_t* data) {
  return ImGui::InputScalar(label, ImGuiDataType_U32, data, &kStepOneHex,
                            &kStepFastHex, "%06X",
                            ImGuiInputTextFlags_CharsHexadecimal);
}

bool InputHexWord(const char* label, uint16_t* data, float input_width,
                  bool no_step) {
  return ImGui::InputScalarLeft(label, ImGuiDataType_U16, data, &kStepOneHex,
                                &kStepFastHex, "%04X", input_width,
                                ImGuiInputTextFlags_CharsHexadecimal, no_step);
}

bool InputHexWord(const char* label, int16_t* data, float input_width,
                  bool no_step) {
  return ImGui::InputScalarLeft(label, ImGuiDataType_S16, data, &kStepOneHex,
                                &kStepFastHex, "%04X", input_width,
                                ImGuiInputTextFlags_CharsHexadecimal, no_step);
}

bool InputHexByte(const char* label, uint8_t* data, float input_width,
                  bool no_step) {
  return ImGui::InputScalarLeft(label, ImGuiDataType_U8, data, &kStepOneHex,
                                &kStepFastHex, "%02X", input_width,
                                ImGuiInputTextFlags_CharsHexadecimal, no_step);
}

bool InputHexByte(const char* label, uint8_t* data, uint8_t max_value,
                  float input_width, bool no_step) {
  if (ImGui::InputScalarLeft(label, ImGuiDataType_U8, data, &kStepOneHex,
                             &kStepFastHex, "%02X", input_width,
                             ImGuiInputTextFlags_CharsHexadecimal, no_step)) {
    if (*data > max_value) {
      *data = max_value;
    }
    return true;
  }
  return false;
}

void Paragraph(const std::string& text) {
  ImGui::TextWrapped("%s", text.c_str());
}

// TODO: Setup themes and text/clickable colors
bool ClickableText(const std::string& text) {
  ImGui::BeginGroup();
  ImGui::PushID(text.c_str());

  // Calculate text size
  ImVec2 text_size = ImGui::CalcTextSize(text.c_str());

  // Get cursor position for hover detection
  ImVec2 pos = ImGui::GetCursorScreenPos();
  ImRect bb(pos, ImVec2(pos.x + text_size.x, pos.y + text_size.y));

  // Add item
  const ImGuiID id = ImGui::GetID(text.c_str());
  bool result = false;
  if (ImGui::ItemAdd(bb, id)) {
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    // Render text with appropriate color
    ImVec4 color = hovered ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
                           : ImGui::GetStyleColorVec4(ImGuiCol_TextLink);
    ImGui::GetWindowDrawList()->AddText(
        pos, ImGui::ColorConvertFloat4ToU32(color), text.c_str());

    result = clicked;
  }

  ImGui::PopID();

  // Advance cursor past the text
  ImGui::Dummy(text_size);
  ImGui::EndGroup();

  return result;
}

void ItemLabel(absl::string_view title, ItemLabelFlags flags) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  const ImVec2 lineStart = ImGui::GetCursorScreenPos();
  const ImGuiStyle& style = ImGui::GetStyle();
  float fullWidth = ImGui::GetContentRegionAvail().x;
  float itemWidth = ImGui::CalcItemWidth() + style.ItemSpacing.x;
  ImVec2 textSize = ImGui::CalcTextSize(title.begin(), title.end());
  ImRect textRect;
  textRect.Min = ImGui::GetCursorScreenPos();
  if (flags & ItemLabelFlag::Right) textRect.Min.x = textRect.Min.x + itemWidth;
  textRect.Max = textRect.Min;
  textRect.Max.x += fullWidth - itemWidth;
  textRect.Max.y += textSize.y;

  ImGui::SetCursorScreenPos(textRect.Min);

  ImGui::AlignTextToFramePadding();
  // Adjust text rect manually because we render it directly into a drawlist
  // instead of using public functions.
  textRect.Min.y += window->DC.CurrLineTextBaseOffset;
  textRect.Max.y += window->DC.CurrLineTextBaseOffset;

  ImGui::ItemSize(textRect);
  if (ImGui::ItemAdd(
          textRect, window->GetID(title.data(), title.data() + title.size()))) {
    ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), textRect.Min,
                              textRect.Max, textRect.Max.x, title.data(),
                              title.data() + title.size(), &textSize);

    if (textRect.GetWidth() < textSize.x && ImGui::IsItemHovered())
      ImGui::SetTooltip("%.*s", (int)title.size(), title.data());
  }
  if (flags & ItemLabelFlag::Left) {
    ImVec2 result;
    auto other = ImVec2{0, textSize.y + window->DC.CurrLineTextBaseOffset};
    result.x = textRect.Max.x - other.x;
    result.y = textRect.Max.y - other.y;
    ImGui::SetCursorScreenPos(result);
    ImGui::SameLine();
  } else if (flags & ItemLabelFlag::Right)
    ImGui::SetCursorScreenPos(lineStart);
}

bool ListBox(const char* label, int* current_item,
             const std::vector<std::string>& items, int height_in_items) {
  std::vector<const char*> items_ptr;
  items_ptr.reserve(items.size());
  for (const auto& item : items) {
    items_ptr.push_back(item.c_str());
  }
  int items_count = static_cast<int>(items.size());
  return ImGui::ListBox(label, current_item, items_ptr.data(), items_count,
                        height_in_items);
}

bool InputTileInfo(const char* label, gfx::TileInfo* tile_info) {
  ImGui::PushID(label);
  ImGui::BeginGroup();
  bool changed = false;
  changed |= InputHexWord(label, &tile_info->id_);
  changed |= InputHexByte("Palette", &tile_info->palette_);
  changed |= ImGui::Checkbox("Priority", &tile_info->over_);
  changed |= ImGui::Checkbox("Vertical Flip", &tile_info->vertical_mirror_);
  changed |= ImGui::Checkbox("Horizontal Flip", &tile_info->horizontal_mirror_);
  ImGui::EndGroup();
  ImGui::PopID();
  return changed;
}

ImGuiID GetID(const std::string& id) { return ImGui::GetID(id.c_str()); }

ImGuiKey MapKeyToImGuiKey(char key) {
  switch (key) {
    case 'A':
      return ImGuiKey_A;
    case 'B':
      return ImGuiKey_B;
    case 'C':
      return ImGuiKey_C;
    case 'D':
      return ImGuiKey_D;
    case 'E':
      return ImGuiKey_E;
    case 'F':
      return ImGuiKey_F;
    case 'G':
      return ImGuiKey_G;
    case 'H':
      return ImGuiKey_H;
    case 'I':
      return ImGuiKey_I;
    case 'J':
      return ImGuiKey_J;
    case 'K':
      return ImGuiKey_K;
    case 'L':
      return ImGuiKey_L;
    case 'M':
      return ImGuiKey_M;
    case 'N':
      return ImGuiKey_N;
    case 'O':
      return ImGuiKey_O;
    case 'P':
      return ImGuiKey_P;
    case 'Q':
      return ImGuiKey_Q;
    case 'R':
      return ImGuiKey_R;
    case 'S':
      return ImGuiKey_S;
    case 'T':
      return ImGuiKey_T;
    case 'U':
      return ImGuiKey_U;
    case 'V':
      return ImGuiKey_V;
    case 'W':
      return ImGuiKey_W;
    case 'X':
      return ImGuiKey_X;
    case 'Y':
      return ImGuiKey_Y;
    case 'Z':
      return ImGuiKey_Z;
    case '/':
      return ImGuiKey_Slash;
    case '-':
      return ImGuiKey_Minus;
    default:
      return ImGuiKey_COUNT;
  }
}

void AddTableColumn(Table& table, const std::string& label,
                    GuiElement element) {
  table.column_labels.push_back(label);
  table.column_contents.push_back(element);
}

void DrawTable(Table& params) {
  if (ImGui::BeginTable(params.id, params.num_columns, params.flags,
                        params.size)) {
    for (int i = 0; i < params.num_columns; ++i)
      ImGui::TableSetupColumn(params.column_labels[i].c_str());

    for (int i = 0; i < params.num_columns; ++i) {
      ImGui::TableNextColumn();
      switch (params.column_contents[i].index()) {
        case 0:
          std::get<0>(params.column_contents[i])();
          break;
        case 1:
          ImGui::Text("%s", std::get<1>(params.column_contents[i]).c_str());
          break;
      }
    }
    ImGui::EndTable();
  }
}

void DrawMenu(Menu& menu) {
  for (const auto& each_menu : menu) {
    if (ImGui::BeginMenu(each_menu.name.c_str())) {
      for (const auto& each_item : each_menu.subitems) {
        if (!each_item.subitems.empty()) {
          if (ImGui::BeginMenu(each_item.name.c_str())) {
            for (const auto& each_subitem : each_item.subitems) {
              if (ImGui::MenuItem(each_subitem.name.c_str(),
                                  each_subitem.shortcut.c_str())) {
                if (each_subitem.callback) each_subitem.callback();
              }
            }
            ImGui::EndMenu();
          }
        } else if (each_item.name == kSeparator) {
          ImGui::Separator();
        } else {
          if (ImGui::MenuItem(each_item.name.c_str(),
                              each_item.shortcut.c_str(),
                              each_item.enabled_condition())) {
            if (each_item.callback) each_item.callback();
          }
        }
      }
      ImGui::EndMenu();
    }
  }
}

bool OpenUrl(const std::string& url) {
  // Open the url in the default browser
  return system(("open " + url).c_str()) == 0;
}

void RenderLayout(const Layout& layout) {
  for (const auto& element : layout.elements) {
    std::visit(overloaded{[](const Text& text) {
                            ImGui::Text("%s", text.content.c_str());
                          },
                          [](const Button& button) {
                            if (ImGui::Button(button.label.c_str())) {
                              button.callback();
                            }
                          }},
               element);
  }
}

void MemoryEditorPopup(const std::string& label, std::span<uint8_t> memory) {
  static bool open = false;
  static MemoryEditor editor;
  if (ImGui::Button("View Data")) {
    open = true;
  }
  if (open) {
    ImGui::Begin(label.c_str(), &open);
    editor.DrawContents(memory.data(), memory.size());
    ImGui::End();
  }
}

}  // namespace gui
}  // namespace yaze
