#include "dungeon_usage_tracker.h"

#include "imgui/imgui.h"

namespace yaze::editor {

void DungeonUsageTracker::CalculateUsageStats(const std::array<zelda3::Room, 0x128>& rooms) {
  blockset_usage_.clear();
  spriteset_usage_.clear();
  palette_usage_.clear();
  
  for (const auto& room : rooms) {
    if (blockset_usage_.find(room.blockset) == blockset_usage_.end()) {
      blockset_usage_[room.blockset] = 1;
    } else {
      blockset_usage_[room.blockset] += 1;
    }

    if (spriteset_usage_.find(room.spriteset) == spriteset_usage_.end()) {
      spriteset_usage_[room.spriteset] = 1;
    } else {
      spriteset_usage_[room.spriteset] += 1;
    }

    if (palette_usage_.find(room.palette) == palette_usage_.end()) {
      palette_usage_[room.palette] = 1;
    } else {
      palette_usage_[room.palette] += 1;
    }
  }
}

void DungeonUsageTracker::DrawUsageStats() {
  if (ImGui::Button("Refresh")) {
    ClearUsageStats();
  }
  
  ImGui::Text("Usage Statistics");
  ImGui::Separator();
  
  ImGui::Text("Blocksets: %zu used", blockset_usage_.size());
  ImGui::Text("Spritesets: %zu used", spriteset_usage_.size());
  ImGui::Text("Palettes: %zu used", palette_usage_.size());
  
  ImGui::Separator();
  
  // Detailed usage breakdown
  if (ImGui::CollapsingHeader("Blockset Usage")) {
    for (const auto& [blockset, count] : blockset_usage_) {
      ImGui::Text("Blockset 0x%02X: %d rooms", blockset, count);
    }
  }
  
  if (ImGui::CollapsingHeader("Spriteset Usage")) {
    for (const auto& [spriteset, count] : spriteset_usage_) {
      ImGui::Text("Spriteset 0x%02X: %d rooms", spriteset, count);
    }
  }
  
  if (ImGui::CollapsingHeader("Palette Usage")) {
    for (const auto& [palette, count] : palette_usage_) {
      ImGui::Text("Palette 0x%02X: %d rooms", palette, count);
    }
  }
}

void DungeonUsageTracker::DrawUsageGrid() {
  // TODO: Implement usage grid visualization
  ImGui::Text("Usage grid visualization not yet implemented");
}

void DungeonUsageTracker::RenderSetUsage(const absl::flat_hash_map<uint16_t, int>& usage_map,
                                         uint16_t& selected_set, int spriteset_offset) {
  // TODO: Implement set usage rendering
  ImGui::Text("Set usage rendering not yet implemented");
}

void DungeonUsageTracker::ClearUsageStats() {
  selected_blockset_ = 0xFFFF;
  selected_spriteset_ = 0xFFFF;
  selected_palette_ = 0xFFFF;
  spriteset_usage_.clear();
  blockset_usage_.clear();
  palette_usage_.clear();
}

}  // namespace yaze::editor
