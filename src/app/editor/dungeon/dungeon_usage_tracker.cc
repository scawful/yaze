#include "dungeon_usage_tracker.h"

#include <algorithm>
#include <vector>

#include "app/editor/dungeon/dungeon_room_store.h"
#include "imgui/imgui.h"

namespace yaze::editor {

namespace {

// Returns a heat-map color from green (low) to yellow (mid) to red (high).
// |t| is clamped to [0, 1].
ImU32 HeatColor(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  float r, g, b;
  if (t < 0.5f) {
    // Green -> Yellow
    float s = t * 2.0f;
    r = s;
    g = 1.0f;
    b = 0.0f;
  } else {
    // Yellow -> Red
    float s = (t - 0.5f) * 2.0f;
    r = 1.0f;
    g = 1.0f - s;
    b = 0.0f;
  }
  return IM_COL32(static_cast<int>(r * 255), static_cast<int>(g * 255),
                  static_cast<int>(b * 255), 200);
}

// Find the maximum count value in a usage map.
int MaxCount(const absl::flat_hash_map<uint16_t, int>& usage_map) {
  int max_val = 0;
  for (const auto& [key, count] : usage_map) {
    if (count > max_val)
      max_val = count;
  }
  return max_val;
}

}  // namespace

void DungeonUsageTracker::CalculateUsageStats(const DungeonRoomStore& rooms) {
  blockset_usage_.clear();
  spriteset_usage_.clear();
  palette_usage_.clear();

  for (int room_id = 0; room_id < static_cast<int>(rooms.size()); ++room_id) {
    const auto& room = rooms[room_id];
    if (blockset_usage_.find(room.blockset()) == blockset_usage_.end()) {
      blockset_usage_[room.blockset()] = 1;
    } else {
      blockset_usage_[room.blockset()] += 1;
    }

    if (spriteset_usage_.find(room.spriteset()) == spriteset_usage_.end()) {
      spriteset_usage_[room.spriteset()] = 1;
    } else {
      spriteset_usage_[room.spriteset()] += 1;
    }

    if (palette_usage_.find(room.palette()) == palette_usage_.end()) {
      palette_usage_[room.palette()] = 1;
    } else {
      palette_usage_[room.palette()] += 1;
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
  if (blockset_usage_.empty() && spriteset_usage_.empty() &&
      palette_usage_.empty()) {
    ImGui::TextDisabled("No usage data. Load rooms and click Refresh.");
    return;
  }

  ImGui::Text("Blockset Usage Grid");
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Color intensity indicates room count.\n"
        "Green = few rooms, Red = many rooms.\n"
        "Click a cell to select that blockset.");
  }
  ImGui::Separator();

  // Collect blockset IDs and sort them for a stable layout.
  std::vector<uint16_t> blockset_ids;
  blockset_ids.reserve(blockset_usage_.size());
  for (const auto& [id, count] : blockset_usage_) {
    blockset_ids.push_back(id);
  }
  std::sort(blockset_ids.begin(), blockset_ids.end());

  int max_val = MaxCount(blockset_usage_);
  if (max_val == 0)
    max_val = 1;  // Avoid division by zero.

  // Render as a grid table. Use 8 columns to fit a reasonable width.
  constexpr int kGridCols = 8;
  if (ImGui::BeginTable(
          "BlocksetGrid", kGridCols,
          ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    for (size_t i = 0; i < blockset_ids.size(); ++i) {
      if (i % kGridCols == 0)
        ImGui::TableNextRow();
      ImGui::TableNextColumn();

      uint16_t bs_id = blockset_ids[i];
      int count = blockset_usage_[bs_id];
      float t = static_cast<float>(count) / static_cast<float>(max_val);

      ImU32 bg_color = HeatColor(t);
      bool is_selected = (selected_blockset_ == bs_id);

      // Draw colored cell background.
      ImVec2 cell_min = ImGui::GetCursorScreenPos();
      ImVec2 cell_size(48.0f, 36.0f);
      ImVec2 cell_max(cell_min.x + cell_size.x, cell_min.y + cell_size.y);
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      draw_list->AddRectFilled(cell_min, cell_max, bg_color);

      if (is_selected) {
        draw_list->AddRect(cell_min, cell_max, IM_COL32(255, 255, 255, 255),
                           0.0f, 0, 2.0f);
      }

      // Invisible button to handle selection.
      char btn_id[32];
      snprintf(btn_id, sizeof(btn_id), "##bs%d", bs_id);
      if (ImGui::InvisibleButton(btn_id, cell_size)) {
        selected_blockset_ = bs_id;
      }

      // Overlay text: blockset ID and count.
      char label[16];
      snprintf(label, sizeof(label), "%02X", bs_id);
      ImVec2 text_pos(cell_min.x + 2.0f, cell_min.y + 2.0f);
      draw_list->AddText(text_pos, IM_COL32(0, 0, 0, 255), label);

      char count_label[16];
      snprintf(count_label, sizeof(count_label), "%d", count);
      ImVec2 count_pos(cell_min.x + 2.0f, cell_min.y + 18.0f);
      draw_list->AddText(count_pos, IM_COL32(0, 0, 0, 200), count_label);

      // Tooltip on hover.
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Blockset 0x%02X", bs_id);
        ImGui::Text("Used by %d rooms", count);
        ImGui::EndTooltip();
      }
    }
    ImGui::EndTable();
  }

  // Also show spriteset and palette grids in collapsible sections.
  if (ImGui::CollapsingHeader("Spriteset Usage Grid")) {
    RenderSetUsage(spriteset_usage_, selected_spriteset_);
  }

  if (ImGui::CollapsingHeader("Palette Usage Grid")) {
    RenderSetUsage(palette_usage_, selected_palette_);
  }
}

void DungeonUsageTracker::RenderSetUsage(
    const absl::flat_hash_map<uint16_t, int>& usage_map, uint16_t& selected_set,
    int spriteset_offset) {
  if (usage_map.empty()) {
    ImGui::TextDisabled("No data available.");
    return;
  }

  // Collect and sort IDs for stable ordering.
  std::vector<uint16_t> ids;
  ids.reserve(usage_map.size());
  for (const auto& [id, count] : usage_map) {
    ids.push_back(id);
  }
  std::sort(ids.begin(), ids.end());

  int max_val = MaxCount(usage_map);
  if (max_val == 0)
    max_val = 1;

  // Summary bar showing distribution.
  ImGui::Text("%zu unique sets, max usage: %d rooms", ids.size(), max_val);
  ImGui::Separator();

  // Table with ID, usage count, and visual bar.
  // Scope table IDs by usage_map address so multiple calls in one frame
  // (spriteset + palette sections) do not collide in ImGui state.
  ImGui::PushID(static_cast<const void*>(&usage_map));
  if (ImGui::BeginTable("SetUsageTable", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                        ImVec2(0, 200.0f))) {
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Rooms", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(ids.size()));

    while (clipper.Step()) {
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        uint16_t set_id = ids[row];
        auto it = usage_map.find(set_id);
        int count = (it != usage_map.end()) ? it->second : 0;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        // Display ID with optional offset (for spritesets).
        uint16_t display_id = static_cast<uint16_t>(set_id + spriteset_offset);
        char id_label[32];
        snprintf(id_label, sizeof(id_label), "0x%02X##set%d", display_id,
                 set_id);
        if (ImGui::Selectable(id_label, selected_set == set_id,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          selected_set = set_id;
        }

        ImGui::TableNextColumn();
        ImGui::Text("%d", count);

        ImGui::TableNextColumn();
        // Visual usage bar.
        float fraction =
            static_cast<float>(count) / static_cast<float>(max_val);
        ImU32 bar_color = HeatColor(fraction);

        ImVec2 bar_pos = ImGui::GetCursorScreenPos();
        float bar_width = ImGui::GetContentRegionAvail().x;
        float bar_height = ImGui::GetTextLineHeight();
        ImVec2 bar_end(bar_pos.x + bar_width * fraction,
                       bar_pos.y + bar_height);
        ImVec2 bar_bg_end(bar_pos.x + bar_width, bar_pos.y + bar_height);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(bar_pos, bar_bg_end,
                                 IM_COL32(40, 40, 40, 100));
        draw_list->AddRectFilled(bar_pos, bar_end, bar_color);

        // Advance cursor past the bar.
        ImGui::Dummy(ImVec2(bar_width, bar_height));
      }
    }

    ImGui::EndTable();
  }
  ImGui::PopID();
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
