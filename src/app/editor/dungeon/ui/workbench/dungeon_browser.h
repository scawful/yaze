#ifndef YAZE_APP_EDITOR_DUNGEON_UI_WORKBENCH_DUNGEON_BROWSER_H_
#define YAZE_APP_EDITOR_DUNGEON_UI_WORKBENCH_DUNGEON_BROWSER_H_

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>

#include "app/gui/automation/widget_auto_register.h"
#include "imgui/imgui.h"

namespace yaze::editor::workbench {

enum class DungeonBrowserView { kList, kIcons };

// Presentation state only. Each browser keeps its domain's existing placement
// callback, categories and asset identity.
struct DungeonBrowserState {
  char search[64] = {};
  int category = 0;
  int sort = 0;
  DungeonBrowserView view = DungeonBrowserView::kList;
};

inline void DrawDungeonBrowserControls(
    DungeonBrowserState& state, std::span<const char* const> categories) {
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##Search", "Search name or ID", state.search,
                           sizeof(state.search));
  gui::AutoRegisterLastItem("input_text", "search");
  const float sort_width = ImGui::GetFontSize() * 5.0f;
  const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
  ImGui::SetNextItemWidth(
      std::max(1.0f, width - sort_width - ImGui::GetStyle().ItemSpacing.x));
  if (!categories.empty())
    ImGui::Combo("##Category", &state.category, categories.data(),
                 static_cast<int>(categories.size()));
  gui::AutoRegisterLastItem("combo", "category");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(sort_width);
  constexpr const char* sorts[] = {"ID", "Name"};
  ImGui::Combo("##Sort", &state.sort, sorts, IM_ARRAYSIZE(sorts));
  gui::AutoRegisterLastItem("combo", "sort");
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Sort by ID or name.");
}

inline void DrawDungeonBrowserViewControls(DungeonBrowserState& state) {
  if (ImGui::BeginTable(
          "##BrowserView", 2,
          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
    ImGui::TableNextColumn();
    if (ImGui::Selectable("List", state.view == DungeonBrowserView::kList, 0,
                          ImVec2(ImGui::GetContentRegionAvail().x, 0)))
      state.view = DungeonBrowserView::kList;
    gui::AutoRegisterLastItem("selectable", "view_list");
    ImGui::TableNextColumn();
    if (ImGui::Selectable("Icons", state.view == DungeonBrowserView::kIcons, 0,
                          ImVec2(ImGui::GetContentRegionAvail().x, 0)))
      state.view = DungeonBrowserView::kIcons;
    gui::AutoRegisterLastItem("selectable", "view_icons");
    ImGui::EndTable();
  }
}

inline std::string DungeonBrowserFold(std::string_view value) {
  std::string folded(value);
  std::transform(
      folded.begin(), folded.end(), folded.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return folded;
}

inline bool DungeonBrowserMatches(std::string_view query, int id,
                                  std::string_view name) {
  if (query.empty())
    return true;
  const std::string folded = DungeonBrowserFold(query);
  if (DungeonBrowserFold(name).find(folded) != std::string::npos)
    return true;
  char hex[16];
  std::snprintf(hex, sizeof(hex), "%03X", id);
  std::string_view number = folded;
  if (number.starts_with("0x"))
    number.remove_prefix(2);
  if (number.starts_with("$"))
    number.remove_prefix(1);
  return !number.empty() && DungeonBrowserFold(hex).ends_with(number);
}

template <typename DrawPreview>
bool DrawDungeonBrowserRow(int id, const std::string& name, bool selected,
                           DrawPreview preview, std::string_view badge = {}) {
  const float height = std::max(28.0f, ImGui::GetTextLineHeight() + 10.0f);
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const bool pressed =
      ImGui::Selectable("##row", selected, 0, ImVec2(0, height));
  const ImVec2 max = ImGui::GetItemRectMax();
  if (ImGui::IsItemVisible()) {
    const bool rendered =
        preview(ImVec2(pos.x + 3, pos.y + 2), ImVec2(24, height - 4));
    char hex[16];
    std::snprintf(hex, sizeof(hex), "%03X", id);
    const std::string id_text =
        badge.empty() ? std::string(hex) : std::string(badge);
    const float id_width = ImGui::CalcTextSize(id_text.c_str()).x;
    const float name_x = pos.x + (rendered ? 34.0f : 6.0f);
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(ImVec2(name_x, pos.y),
                       ImVec2(std::max(name_x, max.x - id_width - 12), max.y),
                       true);
    draw->AddText(
        ImVec2(name_x, pos.y + (height - ImGui::GetTextLineHeight()) * .5f),
        ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
    draw->PopClipRect();
    draw->AddText(ImVec2(max.x - id_width - 4,
                         pos.y + (height - ImGui::GetTextLineHeight()) * .5f),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), id_text.c_str());
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("%s ($%03X)", name.c_str(), id);
  ImGui::LogText("%03X  %s\n", id, name.c_str());
  return pressed;
}

template <typename DrawPreview>
bool DrawDungeonBrowserIcon(int id, const std::string& name, bool selected,
                            float width, DrawPreview preview,
                            std::string_view badge = {}) {
  width = std::max(width, 1.0f);
  const float line_height = ImGui::GetTextLineHeight();
  const float padding = std::min(3.0f, width * 0.08f);
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const ImVec2 size(width, width + line_height * 2.0f + padding);
  // Keep the entry ID stable when the browser changes presentation.
  const bool pressed = ImGui::Selectable("##row", selected, 0, size);
  if (ImGui::IsItemVisible()) {
    char hex[16];
    std::snprintf(hex, sizeof(hex), "%03X", id);
    const std::string id_text =
        badge.empty() ? std::string(hex) : std::string(badge);
    const ImVec2 preview_size(std::max(1.0f, width - padding * 2.0f),
                              std::max(1.0f, width - padding * 2.0f));
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);
    if (!preview(ImVec2(pos.x + padding, pos.y + padding), preview_size)) {
      const ImVec2 label_size = ImGui::CalcTextSize(id_text.c_str());
      draw->AddText(ImVec2(pos.x + (width - label_size.x) * 0.5f,
                           pos.y + (width - line_height) * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), id_text.c_str());
    }
    draw->AddText(ImVec2(pos.x + padding, pos.y + width),
                  ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
    draw->AddText(ImVec2(pos.x + padding, pos.y + width + line_height),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), id_text.c_str());
    draw->PopClipRect();
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("%s ($%03X)%s%s", name.c_str(), id,
                      badge.empty() ? "" : " - ",
                      badge.empty() ? "" : std::string(badge).c_str());
  ImGui::LogText("%03X  %s\n", id, name.c_str());
  return pressed;
}

}  // namespace yaze::editor::workbench

#endif  // YAZE_APP_EDITOR_DUNGEON_UI_WORKBENCH_DUNGEON_BROWSER_H_
