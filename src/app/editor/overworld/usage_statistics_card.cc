#include "app/editor/overworld/usage_statistics_card.h"

#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
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
    if (ImGui::BeginTabBar("UsageTabs")) {
      if (ImGui::BeginTabItem("Grid View")) {
        DrawUsageGrid();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("States")) {
        DrawUsageStates();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }
  ImGui::End();
}

void UsageStatisticsCard::DrawUsageGrid() {
  // Logic moved from OverworldEditor::DrawUsageGrid
  // Simplified for card layout
  
  ImGui::Text("Map Usage Grid (8x8)");
  ImGui::Separator();

  if (ImGui::BeginTable("UsageGrid", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    for (int y = 0; y < 8; y++) {
      ImGui::TableNextRow();
      for (int x = 0; x < 8; x++) {
        ImGui::TableNextColumn();
        int map_id = y * 8 + x;
        
        // Determine color based on usage (placeholder logic, assuming we can query map status)
        // For now, just show ID
        ImGui::Text("%02X", map_id);
        
        // TODO: Add actual usage data visualization here
        // e.g., color code based on entity count or size
      }
    }
    ImGui::EndTable();
  }
}

void UsageStatisticsCard::DrawUsageStates() {
  ImGui::Text("Global Usage Statistics");
  ImGui::Separator();
  
  // Placeholder for usage states
  ImGui::Text("Total Maps: 64");
  ImGui::Text("Large Maps: %d", 0); // TODO: Query actual data
  ImGui::Text("Small Maps: %d", 0);
}

}  // namespace yaze::editor
