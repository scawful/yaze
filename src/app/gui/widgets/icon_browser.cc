#include "app/gui/widgets/icon_browser.h"

#include <cctype>
#include <cstring>
#include <string>

#include "app/gui/core/common_icons.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace gui {
namespace icon_browser_internal {

namespace {

bool ContainsIgnoreCase(const char* haystack, const char* needle,
                        std::size_t needle_len) {
  if (needle_len == 0)
    return true;
  const std::size_t hay_len = std::strlen(haystack);
  if (hay_len < needle_len)
    return false;
  for (std::size_t i = 0; i + needle_len <= hay_len; ++i) {
    bool match = true;
    for (std::size_t j = 0; j < needle_len; ++j) {
      const unsigned char hc = static_cast<unsigned char>(haystack[i + j]);
      const unsigned char nc = static_cast<unsigned char>(needle[j]);
      if (std::tolower(hc) != std::tolower(nc)) {
        match = false;
        break;
      }
    }
    if (match)
      return true;
  }
  return false;
}

}  // namespace

bool MatchesQuery(const char* search_key, const char* query) {
  if (query == nullptr || query[0] == '\0')
    return true;
  if (search_key == nullptr)
    return false;

  // Split query on whitespace; every term must be found in search_key.
  const std::size_t qlen = std::strlen(query);
  std::size_t i = 0;
  while (i < qlen) {
    while (i < qlen && std::isspace(static_cast<unsigned char>(query[i])))
      ++i;
    const std::size_t start = i;
    while (i < qlen && !std::isspace(static_cast<unsigned char>(query[i])))
      ++i;
    const std::size_t term_len = i - start;
    if (term_len == 0)
      continue;
    if (!ContainsIgnoreCase(search_key, query + start, term_len))
      return false;
  }
  return true;
}

}  // namespace icon_browser_internal

const char* IconBrowser(const char* label) {
  if (label == nullptr)
    label = "##icon_browser";
  ImGui::PushID(label);

  static thread_local std::string s_query;

  // Search box — the ID is scoped by the enclosing PushID.
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputTextWithHint("##search", "Search icons...", &s_query);

  const char* selected_glyph = nullptr;

  const float avail_x = ImGui::GetContentRegionAvail().x;
  const float cell_size =
      ImGui::GetFontSize() * 2.0f + ImGui::GetStyle().FramePadding.y * 2.0f;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const int columns = std::max(
      1, static_cast<int>((avail_x + spacing) / (cell_size + spacing)));

  int column = 0;
  for (int i = 0; i < kCommonIconCount; ++i) {
    const CommonIcon& icon = kCommonIcons[i];
    if (!icon_browser_internal::MatchesQuery(icon.search_key, s_query.c_str()))
      continue;

    if (column > 0)
      ImGui::SameLine();
    ImGui::PushID(i);
    if (ImGui::Button(icon.glyph, ImVec2(cell_size, cell_size))) {
      selected_glyph = icon.glyph;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s\n%s", icon.macro_name, icon.category);
    }
    ImGui::PopID();

    column = (column + 1) % columns;
  }

  ImGui::PopID();
  return selected_glyph;
}

}  // namespace gui
}  // namespace yaze
