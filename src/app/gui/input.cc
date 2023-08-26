#include "input.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "absl/strings/string_view.h"

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

bool InputHexShort(const char* label, uint32_t* data) {
  return ImGui::InputScalar(label, ImGuiDataType_U32, data, &kStepOneHex,
                            &kStepFastHex, "%06X",
                            ImGuiInputTextFlags_CharsHexadecimal);
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

}  // namespace gui
}  // namespace app
}  // namespace yaze