#include "app/emu/memory/memory.h"

#include <imgui/imgui.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "app/emu/debug/log.h"

namespace yaze {
namespace app {
namespace emu {

void DrawSnesMemoryMapping(const MemoryImpl& memory) {
  // Using those as a base value to create width/height that are factor of the
  // size of our font
  const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
  const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
  const char* column_names[] = {
      "Offset", "0x00", "0x01", "0x02", "0x03", "0x04", "0x05", "0x06", "0x07",
      "0x08",   "0x09", "0x0A", "0x0B", "0x0C", "0x0D", "0x0E", "0x0F", "0x10",
      "0x11",   "0x12", "0x13", "0x14", "0x15", "0x16", "0x17", "0x18", "0x19",
      "0x1A",   "0x1B", "0x1C", "0x1D", "0x1E", "0x1F"};
  const int columns_count = IM_ARRAYSIZE(column_names);
  const int rows_count = 16;

  static ImGuiTableFlags table_flags =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
      ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Hideable |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
      ImGuiTableFlags_HighlightHoveredColumn;
  static bool bools[columns_count * rows_count] = {};
  static int frozen_cols = 1;
  static int frozen_rows = 2;
  ImGui::CheckboxFlags("_ScrollX", &table_flags, ImGuiTableFlags_ScrollX);
  ImGui::CheckboxFlags("_ScrollY", &table_flags, ImGuiTableFlags_ScrollY);
  ImGui::CheckboxFlags("_NoBordersInBody", &table_flags,
                       ImGuiTableFlags_NoBordersInBody);
  ImGui::CheckboxFlags("_HighlightHoveredColumn", &table_flags,
                       ImGuiTableFlags_HighlightHoveredColumn);
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
  ImGui::SliderInt("Frozen columns", &frozen_cols, 0, 2);
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
  ImGui::SliderInt("Frozen rows", &frozen_rows, 0, 2);

  if (ImGui::BeginTable("table_angled_headers", columns_count, table_flags,
                        ImVec2(0.0f, TEXT_BASE_HEIGHT * 12))) {
    ImGui::TableSetupColumn(
        column_names[0],
        ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder);
    for (int n = 1; n < columns_count; n++)
      ImGui::TableSetupColumn(column_names[n],
                              ImGuiTableColumnFlags_AngledHeader |
                                  ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupScrollFreeze(frozen_cols, frozen_rows);

    ImGui::TableAngledHeadersRow();
    ImGui::TableHeadersRow();
    for (int row = 0; row < rows_count; row++) {
      ImGui::PushID(row);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Offset 0x%04X", row);
      for (int column = 1; column < columns_count; column++)
        if (ImGui::TableSetColumnIndex(column)) {
          ImGui::PushID(column);
          ImGui::Checkbox("", &bools[row * columns_count + column]);
          ImGui::PopID();
        }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
}

}  // namespace emu
}  // namespace app
}  // namespace yaze