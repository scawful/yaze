// Related header
#include "room_tag_editor_panel.h"

// C++ standard library headers
#include <algorithm>
#include <cctype>
#include <string>

// Third-party library headers
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

// Project headers
#include "app/gui/core/icons.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/resource_labels.h"
#include "zelda3/zelda3_labels.h"

namespace yaze::editor {

void RoomTagEditorPanel::Draw(bool* p_open) {
  if (!rooms_) {
    ImGui::TextDisabled("No room data available.");
    return;
  }

  if (cache_dirty_) {
    RebuildRoomCountCache();
  }

  DrawTagTable();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  DrawQuickAssign();
}

void RoomTagEditorPanel::DrawTagTable() {
  const auto& vanilla_tags = zelda3::Zelda3Labels::GetRoomTagNames();
  const int num_tags = static_cast<int>(vanilla_tags.size());

  // Filter input
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputTextWithHint("##TagFilter", ICON_MD_SEARCH " Filter tags...",
                               filter_text_, sizeof(filter_text_))) {
    // Filter text changed - table will re-filter below
  }

  ImGui::Spacing();

  const float available_height = ImGui::GetContentRegionAvail().y - 120.0f;
  const ImGuiTableFlags table_flags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_SizingFixedFit;

  if (!ImGui::BeginTable("##RoomTagTable", 6, table_flags,
                         ImVec2(0, available_height))) {
    return;
  }

  ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 40.0f);
  ImGui::TableSetupColumn("Vanilla Name", ImGuiTableColumnFlags_WidthFixed,
                          140.0f);
  ImGui::TableSetupColumn("ASM Label", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Feature Flag", ImGuiTableColumnFlags_WidthFixed,
                          160.0f);
  ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
  ImGui::TableSetupColumn("Rooms", ImGuiTableColumnFlags_WidthFixed, 80.0f);
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableHeadersRow();

  // Determine if hack manifest is available
  const bool has_manifest =
      project_ && project_->hack_manifest.loaded();

  std::string filter_lower;
  if (filter_text_[0] != '\0') {
    filter_lower = filter_text_;
    std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(),
                   [](unsigned char c) {
                     return static_cast<char>(std::tolower(c));
                   });
  }

  for (int i = 0; i < num_tags; i++) {
    const std::string& vanilla_name = vanilla_tags[i];

    // Get manifest data if available
    std::optional<core::RoomTagEntry> manifest_tag;
    if (has_manifest) {
      manifest_tag =
          project_->hack_manifest.GetRoomTag(static_cast<uint8_t>(i));
    }

    // Apply filter
    if (!filter_lower.empty()) {
      std::string searchable = vanilla_name;
      if (manifest_tag.has_value()) {
        searchable += " " + manifest_tag->name;
        searchable += " " + manifest_tag->feature_flag;
      }
      searchable += " " + absl::StrFormat("$%02X", i);
      std::transform(searchable.begin(), searchable.end(), searchable.begin(),
                     [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                     });
      if (searchable.find(filter_lower) == std::string::npos) {
        continue;
      }
    }

    ImGui::TableNextRow();

    // Column: Slot
    ImGui::TableNextColumn();
    ImGui::Text("$%02X", i);

    // Column: Vanilla Name
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(vanilla_name.c_str());

    // Column: ASM Label
    ImGui::TableNextColumn();
    if (manifest_tag.has_value() && !manifest_tag->name.empty()) {
      ImGui::TextUnformatted(manifest_tag->name.c_str());
    } else {
      ImGui::TextDisabled("--");
    }

    // Column: Feature Flag
    ImGui::TableNextColumn();
    if (manifest_tag.has_value() && !manifest_tag->feature_flag.empty()) {
      if (manifest_tag->enabled) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f),
                           ICON_MD_CIRCLE);
      } else {
        ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f),
                           ICON_MD_CIRCLE);
      }
      ImGui::SameLine(0, 4);
      ImGui::TextUnformatted(manifest_tag->feature_flag.c_str());
    } else {
      ImGui::TextDisabled("--");
    }

    // Column: Status
    ImGui::TableNextColumn();
    if (manifest_tag.has_value() && manifest_tag->enabled) {
      ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Active");
    } else if (manifest_tag.has_value() && !manifest_tag->enabled) {
      ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "Disabled");
    } else if (vanilla_name == "Nothing") {
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Available");
    } else {
      ImGui::TextColored(ImVec4(0.4f, 0.6f, 0.9f, 1.0f), "Vanilla");
    }

    // Column: Rooms
    ImGui::TableNextColumn();
    auto it = tag_usage_count_.find(i);
    int count = (it != tag_usage_count_.end()) ? it->second : 0;
    if (count > 0) {
      ImGui::Text("%d", count);
    } else {
      ImGui::TextDisabled("0");
    }
  }

  ImGui::EndTable();
}

void RoomTagEditorPanel::DrawQuickAssign() {
  if (!rooms_ || current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size())) {
    ImGui::TextDisabled("No room selected.");
    return;
  }

  auto& room = (*rooms_)[current_room_id_];

  ImGui::Text(ICON_MD_GRID_ON " Current Room: 0x%03X", current_room_id_);
  ImGui::Spacing();

  const auto& vanilla_tags = zelda3::Zelda3Labels::GetRoomTagNames();
  const int num_tags = static_cast<int>(vanilla_tags.size());

  // Helper to build tag display name with manifest info
  auto get_tag_display_name = [&](int tag_idx) -> std::string {
    std::string label = zelda3::GetRoomTagLabel(tag_idx);
    if (project_ && project_->hack_manifest.loaded()) {
      auto tag =
          project_->hack_manifest.GetRoomTag(static_cast<uint8_t>(tag_idx));
      if (tag.has_value() && !tag->enabled) {
        return absl::StrCat(
            label, " (disabled",
            tag->feature_flag.empty()
                ? ")"
                : absl::StrCat(" by ", tag->feature_flag, ")"));
      }
    }
    return label;
  };

  // Tag1 combo
  int tag1_val = static_cast<int>(room.tag1());
  int tag1_idx = std::clamp(tag1_val, 0, num_tags - 1);
  auto tag1_display = get_tag_display_name(tag1_idx);

  ImGui::TextDisabled(ICON_MD_LABEL);
  ImGui::SameLine(0, 4);
  ImGui::Text("Tag 1:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo("##QuickTag1", tag1_display.c_str())) {
    for (int i = 0; i < num_tags; i++) {
      auto item_label = get_tag_display_name(i);
      if (ImGui::Selectable(item_label.c_str(), tag1_idx == i)) {
        room.SetTag1(static_cast<zelda3::TagKey>(i));
        if (room.rom() && room.rom()->is_loaded()) {
          room.RenderRoomGraphics();
        }
        cache_dirty_ = true;
      }
    }
    ImGui::EndCombo();
  }

  // Tag2 combo
  int tag2_val = static_cast<int>(room.tag2());
  int tag2_idx = std::clamp(tag2_val, 0, num_tags - 1);
  auto tag2_display = get_tag_display_name(tag2_idx);

  ImGui::TextDisabled(ICON_MD_LABEL_OUTLINE);
  ImGui::SameLine(0, 4);
  ImGui::Text("Tag 2:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo("##QuickTag2", tag2_display.c_str())) {
    for (int i = 0; i < num_tags; i++) {
      auto item_label = get_tag_display_name(i);
      if (ImGui::Selectable(item_label.c_str(), tag2_idx == i)) {
        room.SetTag2(static_cast<zelda3::TagKey>(i));
        if (room.rom() && room.rom()->is_loaded()) {
          room.RenderRoomGraphics();
        }
        cache_dirty_ = true;
      }
    }
    ImGui::EndCombo();
  }
}

void RoomTagEditorPanel::RebuildRoomCountCache() {
  tag_usage_count_.clear();

  if (!rooms_) {
    cache_dirty_ = false;
    return;
  }

  for (size_t i = 0; i < rooms_->size(); i++) {
    const auto& room = (*rooms_)[i];
    if (!room.IsLoaded()) {
      continue;
    }
    int t1 = static_cast<int>(room.tag1());
    int t2 = static_cast<int>(room.tag2());
    tag_usage_count_[t1]++;
    tag_usage_count_[t2]++;
  }

  cache_dirty_ = false;
}

}  // namespace yaze::editor
