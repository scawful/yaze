#include "dungeon_room_selector.h"

#include "absl/strings/str_format.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "util/hex.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

using ImGui::BeginChild;
using ImGui::EndChild;
using ImGui::SameLine;

void DungeonRoomSelector::Draw() {
  // Legacy combined view - prefer using DrawRoomSelector() and
  // DrawEntranceSelector() separately via their own EditorPanels
  DrawRoomSelector();
}

void DungeonRoomSelector::RebuildRoomFilterCache() {
  filtered_room_indices_.clear();
  filtered_room_indices_.reserve(zelda3::kNumberOfRooms);

  for (int i = 0; i < zelda3::kNumberOfRooms; ++i) {
    std::string display_name = zelda3::GetRoomLabel(i);
    if (room_filter_.PassFilter(display_name.c_str())) {
      filtered_room_indices_.push_back(i);
    }
  }
}

void DungeonRoomSelector::DrawRoomSelector() {
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return;
  }

  gui::InputHexWord("Room ID", &current_room_id_, 50.f, true);
  ImGui::Separator();

  room_filter_.Draw("Filter", ImGui::GetContentRegionAvail().x);

  // Rebuild cache if filter changed
  std::string current_filter(room_filter_.InputBuf);
  if (current_filter != last_room_filter_ || filtered_room_indices_.empty()) {
    last_room_filter_ = current_filter;
    RebuildRoomFilterCache();
  }

  if (ImGui::BeginTable("RoomList", 2,
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("Name");
    ImGui::TableHeadersRow();

    // Use ImGuiListClipper for virtualized rendering
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(filtered_room_indices_.size()));

    while (clipper.Step()) {
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        int room_id = filtered_room_indices_[row];
        std::string display_name = zelda3::GetRoomLabel(room_id);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        char label[32];
        snprintf(label, sizeof(label), "%03X", room_id);
        if (ImGui::Selectable(label, current_room_id_ == room_id,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          current_room_id_ = room_id;
          if (room_selected_callback_) {
            room_selected_callback_(room_id);
          }
        }

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(display_name.c_str());
      }
    }

    ImGui::EndTable();
  }
}

void DungeonRoomSelector::RebuildEntranceFilterCache() {
  constexpr int kNumSpawnPoints = 7;
  constexpr int kNumEntrances = 133;
  constexpr int kTotalEntries = 140;

  filtered_entrance_indices_.clear();
  filtered_entrance_indices_.reserve(kTotalEntries);

  for (int i = 0; i < kTotalEntries; i++) {
    std::string display_name;

    if (i < kNumSpawnPoints) {
      display_name = absl::StrFormat("Spawn Point %d", i);
    } else {
      int entrance_id = i - kNumSpawnPoints;
      if (entrance_id < kNumEntrances) {
        display_name = zelda3::GetEntranceLabel(entrance_id);
      } else {
        display_name = absl::StrFormat("Unknown Entrance %d", i);
      }
    }

    int room_id = (entrances_ && i < static_cast<int>(entrances_->size()))
                      ? (*entrances_)[i].room_
                      : 0;

    char filter_text[256];
    snprintf(filter_text, sizeof(filter_text), "%s %03X", display_name.c_str(),
             room_id);

    if (entrance_filter_.PassFilter(filter_text)) {
      filtered_entrance_indices_.push_back(i);
    }
  }
}

void DungeonRoomSelector::DrawEntranceSelector() {
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return;
  }

  if (!entrances_) {
    ImGui::Text("Entrances not loaded");
    return;
  }

  auto current_entrance = (*entrances_)[current_entrance_id_];

  // Organized Properties Table
  if (ImGui::BeginTable("EntranceProps", 4, ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Core", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Camera", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Scroll", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    gui::InputHexWord("Entr ID", &current_entrance.entrance_id_);
    gui::InputHexWord("Room ID", &current_entrance.room_);
    gui::InputHexByte("Dungeon", &current_entrance.dungeon_id_);
    gui::InputHexByte("Music", &current_entrance.music_);

    ImGui::TableNextColumn();
    gui::InputHexWord("Player X", &current_entrance.x_position_);
    gui::InputHexWord("Player Y", &current_entrance.y_position_);
    gui::InputHexByte("Blockset", &current_entrance.blockset_);
    gui::InputHexByte("Floor", &current_entrance.floor_);

    ImGui::TableNextColumn();
    gui::InputHexWord("Cam Trg X", &current_entrance.camera_trigger_x_);
    gui::InputHexWord("Cam Trg Y", &current_entrance.camera_trigger_y_);
    gui::InputHexWord("Exit", &current_entrance.exit_);

    ImGui::TableNextColumn();
    gui::InputHexWord("Scroll X", &current_entrance.camera_x_);
    gui::InputHexWord("Scroll Y", &current_entrance.camera_y_);

    ImGui::EndTable();
  }

  ImGui::Separator();
  if (ImGui::CollapsingHeader("Camera Boundaries")) {
    ImGui::Text("                North   East    South   West");
    ImGui::Text("Quadrant      ");
    SameLine();
    gui::InputHexByte("##QN", &current_entrance.camera_boundary_qn_, 40.f);
    SameLine();
    gui::InputHexByte("##QE", &current_entrance.camera_boundary_qe_, 40.f);
    SameLine();
    gui::InputHexByte("##QS", &current_entrance.camera_boundary_qs_, 40.f);
    SameLine();
    gui::InputHexByte("##QW", &current_entrance.camera_boundary_qw_, 40.f);

    ImGui::Text("Full Room     ");
    SameLine();
    gui::InputHexByte("##FN", &current_entrance.camera_boundary_fn_, 40.f);
    SameLine();
    gui::InputHexByte("##FE", &current_entrance.camera_boundary_fe_, 40.f);
    SameLine();
    gui::InputHexByte("##FS", &current_entrance.camera_boundary_fs_, 40.f);
    SameLine();
    gui::InputHexByte("##FW", &current_entrance.camera_boundary_fw_, 40.f);
  }
  ImGui::Separator();

  entrance_filter_.Draw("Filter", ImGui::GetContentRegionAvail().x);

  // Rebuild cache if filter changed
  std::string current_filter(entrance_filter_.InputBuf);
  if (current_filter != last_entrance_filter_ ||
      filtered_entrance_indices_.empty()) {
    last_entrance_filter_ = current_filter;
    RebuildEntranceFilterCache();
  }

  constexpr int kNumSpawnPoints = 7;

  if (ImGui::BeginTable("EntranceList", 3,
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("Room", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Name");
    ImGui::TableHeadersRow();

    // Use ImGuiListClipper for virtualized rendering
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(filtered_entrance_indices_.size()));

    while (clipper.Step()) {
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        int i = filtered_entrance_indices_[row];
        std::string display_name;

        if (i < kNumSpawnPoints) {
          display_name = absl::StrFormat("Spawn Point %d", i);
        } else {
          int entrance_id = i - kNumSpawnPoints;
          display_name = zelda3::GetEntranceLabel(entrance_id);
        }

        int room_id = (i < static_cast<int>(entrances_->size()))
                          ? (*entrances_)[i].room_
                          : 0;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        char label[32];
        snprintf(label, sizeof(label), "%02X", i);
        if (ImGui::Selectable(label, current_entrance_id_ == i,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          current_entrance_id_ = i;
          if (i < static_cast<int>(entrances_->size())) {
            if (entrance_selected_callback_) {
              entrance_selected_callback_(i);
            } else if (room_selected_callback_) {
              room_selected_callback_(room_id);
            }
          }
        }

        ImGui::TableNextColumn();
        ImGui::Text("%03X", room_id);

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(display_name.c_str());
      }
    }

    ImGui::EndTable();
  }
}

}  // namespace yaze::editor
