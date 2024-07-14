#include "input.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <string>

#include "absl/strings/string_view.h"

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
  flags |= ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_NoMarkEdited;

  bool value_changed = false;
  // if (p_step == NULL) {
  //   ImGui::SetNextItemWidth(input_width);
  //   if (InputText("", buf, IM_ARRAYSIZE(buf), flags))
  //     value_changed = DataTypeApplyFromText(buf, data_type, p_data, format);
  // } else {
  const float button_size = GetFrameHeight();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("%s", label);
  ImGui::SameLine();
  BeginGroup();  // The only purpose of the group here is to allow the caller
                 // to query item data e.g. IsItemActive()
  PushID(label);
  SetNextItemWidth(ImMax(
      1.0f, CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * 2));

  // Place the label on the left of the input field
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2{style.ItemSpacing.x, style.ItemSpacing.y});
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2{style.FramePadding.x, style.FramePadding.y});

  ImGui::SetNextItemWidth(input_width);
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
    ImGuiButtonFlags button_flags =
        ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;
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
namespace app {
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
    ImGui::RenderTextEllipsis(
        ImGui::GetWindowDrawList(), textRect.Min, textRect.Max, textRect.Max.x,
        textRect.Max.x, title.data(), title.data() + title.size(), &textSize);

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

ImGuiID GetID(const std::string& id) { return ImGui::GetID(id.c_str()); }

}  // namespace gui
}  // namespace app
}  // namespace yaze