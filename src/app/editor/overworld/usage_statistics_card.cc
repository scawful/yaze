#include "app/editor/overworld/usage_statistics_card.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

UsageStatisticsCard::UsageStatisticsCard(zelda3::Overworld* overworld)
    : overworld_(overworld) {}

void UsageStatisticsCard::Draw(bool* p_open) {
  if (!overworld_ || !overworld_->is_loaded()) {
    ImGui::TextDisabled("Overworld not loaded");
    return;
  }

  if (ImGui::Begin("Usage Statistics", p_open)) {
    if (gui::BeginThemedTabBar("UsageTabs")) {
      if (ImGui::BeginTabItem("Grid View")) {
        DrawUsageGrid();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("States")) {
        DrawUsageStates();
        ImGui::EndTabItem();
      }
      gui::EndThemedTabBar();
    }
  }
  ImGui::End();
}

void UsageStatisticsCard::DrawUsageGrid() {
  const int world = std::clamp(overworld_->current_world(), 0, 2);
  const int world_start = (world == 0) ? 0 : ((world == 1) ? 0x40 : 0x80);
  const int world_count = (world == 2) ? 32 : 64;

  ImGui::Text("Map Usage Grid (World %d)", world);
  ImGui::Separator();

  if (ImGui::BeginTable(
          "UsageGrid", 8,
          ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    for (int y = 0; y < 8; y++) {
      ImGui::TableNextRow();
      for (int x = 0; x < 8; x++) {
        ImGui::TableNextColumn();
        const int local_id = y * 8 + x;
        if (local_id >= world_count) {
          ImGui::TextDisabled("--");
          continue;
        }

        const int map_id = world_start + local_id;
        const auto* map = overworld_->overworld_map(map_id);
        if (!map) {
          ImGui::TextDisabled("??");
          continue;
        }

        ImVec4 area_color = ImVec4(0.35f, 0.55f, 0.35f, 1.0f);  // Small
        const char* area_label = "Small";
        switch (map->area_size()) {
          case zelda3::AreaSizeEnum::LargeArea:
            area_color = ImVec4(0.65f, 0.52f, 0.28f, 1.0f);
            area_label = "Large";
            break;
          case zelda3::AreaSizeEnum::WideArea:
            area_color = ImVec4(0.30f, 0.50f, 0.72f, 1.0f);
            area_label = "Wide";
            break;
          case zelda3::AreaSizeEnum::TallArea:
            area_color = ImVec4(0.55f, 0.40f, 0.72f, 1.0f);
            area_label = "Tall";
            break;
          case zelda3::AreaSizeEnum::SmallArea:
          default:
            break;
        }

        ImGui::PushStyleColor(ImGuiCol_Button, area_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(area_color.x + 0.1f, area_color.y + 0.1f,
                                     area_color.z + 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, area_color);
        const std::string button_id =
            absl::StrFormat("%02X##usage_map_%d", map_id, map_id);
        ImGui::Button(button_id.c_str(), ImVec2(34.0f, 0.0f));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::Text("Map %02X (%s)", map_id, area_label);
          ImGui::Text("Parent: %02X", map->parent());
          ImGui::Text("Quadrant: %d", map->large_index());
          ImGui::EndTooltip();
        }
      }
    }
    ImGui::EndTable();
  }
}

void UsageStatisticsCard::DrawUsageStates() {
  const int world = std::clamp(overworld_->current_world(), 0, 2);
  const int world_start = (world == 0) ? 0 : ((world == 1) ? 0x40 : 0x80);
  const int world_count = (world == 2) ? 32 : 64;

  int small_count = 0;
  int large_count = 0;
  int wide_count = 0;
  int tall_count = 0;
  int parent_count = 0;

  for (int local_id = 0; local_id < world_count; ++local_id) {
    const int map_id = world_start + local_id;
    const auto* map = overworld_->overworld_map(map_id);
    if (!map) {
      continue;
    }

    if (map->parent() == map_id) {
      parent_count++;
    }

    switch (map->area_size()) {
      case zelda3::AreaSizeEnum::LargeArea:
        large_count++;
        break;
      case zelda3::AreaSizeEnum::WideArea:
        wide_count++;
        break;
      case zelda3::AreaSizeEnum::TallArea:
        tall_count++;
        break;
      case zelda3::AreaSizeEnum::SmallArea:
      default:
        small_count++;
        break;
    }
  }

  ImGui::Text("Global Usage Statistics");
  ImGui::Separator();
  ImGui::Text("Current World: %d", world);
  ImGui::Text("Total Maps: %d", world_count);
  ImGui::Text("Small Maps: %d", small_count);
  ImGui::Text("Large Maps: %d", large_count);
  ImGui::Text("Wide Maps: %d", wide_count);
  ImGui::Text("Tall Maps: %d", tall_count);
  ImGui::Text("Parent Areas: %d", parent_count);
}

}  // namespace yaze::editor
