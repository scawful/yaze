#include "app/editor/layout/layout_designer/panel_palette.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include "app/editor/registry/content_registry.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/drag_drop.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace editor {
namespace layout_designer {
namespace panel_palette_internal {

namespace {

std::string ToLower(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    out.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

}  // namespace

bool MatchesQuery(const PanelPaletteEntry& entry, const std::string& query) {
  if (query.empty())
    return true;
  const std::string haystack =
      ToLower(entry.display_name + " " + entry.panel_id + " " + entry.category);
  std::istringstream iss(ToLower(query));
  std::string term;
  while (iss >> term) {
    if (haystack.find(term) == std::string::npos)
      return false;
  }
  return true;
}

}  // namespace panel_palette_internal

std::vector<PanelPaletteEntry> CollectPaletteEntries(
    const std::string& exclude_panel_id) {
  std::vector<PanelPaletteEntry> out;
  for (WindowContent* panel : ContentRegistry::Panels::GetAll()) {
    if (panel == nullptr)
      continue;
    PanelPaletteEntry entry;
    entry.panel_id = panel->GetId();
    if (!exclude_panel_id.empty() && entry.panel_id == exclude_panel_id) {
      continue;
    }
    entry.display_name = panel->GetDisplayName();
    entry.icon = panel->GetIcon();
    entry.category = panel->GetEditorCategory();
    out.push_back(std::move(entry));
  }
  std::sort(out.begin(), out.end(),
            [](const PanelPaletteEntry& a, const PanelPaletteEntry& b) {
              if (a.category != b.category)
                return a.category < b.category;
              return a.display_name < b.display_name;
            });
  return out;
}

std::string DrawPanelPalette(const std::vector<PanelPaletteEntry>& entries,
                             std::string* query) {
  std::string clicked_id;
  if (query != nullptr) {
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##layout_designer_palette_query", "Search panels",
                             query);
  }
  const std::string q = query != nullptr ? *query : std::string();

  // Bucket filtered entries by category so collapsing headers group cleanly.
  std::string current_category;
  bool category_open = false;
  int rendered_in_category = 0;

  auto close_current_category = [&]() {
    if (category_open && rendered_in_category == 0) {
      ImGui::TextDisabled("  (no matches)");
    }
    category_open = false;
    rendered_in_category = 0;
  };

  for (const PanelPaletteEntry& entry : entries) {
    if (!panel_palette_internal::MatchesQuery(entry, q))
      continue;

    if (entry.category != current_category) {
      close_current_category();
      current_category = entry.category;
      const std::string header =
          entry.category.empty() ? "(uncategorized)" : entry.category;
      category_open = ImGui::CollapsingHeader(header.c_str(),
                                              ImGuiTreeNodeFlags_DefaultOpen);
    }
    if (!category_open)
      continue;

    ImGui::PushID(entry.panel_id.c_str());
    const std::string label = entry.icon.empty()
                                  ? entry.display_name
                                  : entry.icon + "  " + entry.display_name;
    if (ImGui::Selectable(label.c_str(), /*selected=*/false,
                          ImGuiSelectableFlags_AllowDoubleClick)) {
      clicked_id = entry.panel_id;
    }
    gui::BeginPanelDragSource(entry.panel_id.c_str(), label.c_str());
    if (ImGui::IsItemHovered() && !entry.panel_id.empty()) {
      ImGui::SetTooltip("%s", entry.panel_id.c_str());
    }
    ImGui::PopID();
    ++rendered_in_category;
  }
  close_current_category();

  return clicked_id;
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
