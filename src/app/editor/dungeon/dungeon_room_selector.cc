#include "dungeon_room_selector.h"

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

void DungeonRoomSelector::DrawRoomSelector() {
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return;
  }

  gui::InputHexWord("Room ID", &current_room_id_, 50.f, true);
  ImGui::Separator();

  room_filter_.Draw("Filter", ImGui::GetContentRegionAvail().x);

  if (ImGui::BeginTable("RoomList", 2,
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders |
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("Name");
    ImGui::TableHeadersRow();

    // Use kNumberOfRooms (296) as limit - rooms_ array is 0x128 elements
    for (int i = 0; i < zelda3::kNumberOfRooms; ++i) {
      // Use unified ResourceLabelProvider for room names
      std::string display_name = zelda3::GetRoomLabel(i);

      if (room_filter_.PassFilter(display_name.c_str())) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        char label[32];
        snprintf(label, sizeof(label), "%03X", i);
        if (ImGui::Selectable(label, current_room_id_ == i,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          current_room_id_ = i;
          if (room_selected_callback_) {
            room_selected_callback_(i);
          }
        }

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(display_name.c_str());
      }
    }
    ImGui::EndTable();
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

  // Entrance array layout (from LoadRoomEntrances):
  //   indices 0-6 (0x00-0x06): Spawn points (7 entries)
  //   indices 7-139 (0x07-0x8B): Regular entrances (133 entries, IDs 0x00-0x84)
  constexpr int kNumSpawnPoints = 7;      // 0x07
  constexpr int kNumEntrances = 133;      // 0x85
  constexpr int kTotalEntries = 140;      // 0x8C

  if (ImGui::BeginTable("EntranceList", 3,
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders |
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("Room", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Name");
    ImGui::TableHeadersRow();

    for (int i = 0; i < kTotalEntries; i++) {
      std::string display_name;
      
      if (i < kNumSpawnPoints) {
        // Spawn points are at indices 0-6
        display_name = absl::StrFormat("Spawn Point %d", i);
      } else {
        // Regular entrances are at indices 7-139, mapped to entrance IDs 0-132
        int entrance_id = i - kNumSpawnPoints;
        if (entrance_id < kNumEntrances) {
          // Use unified ResourceLabelProvider for entrance names
          display_name = zelda3::GetEntranceLabel(entrance_id);
        } else {
          display_name = absl::StrFormat("Unknown Entrance %d", i);
        }
      }

      // Get room ID for this entrance
      int room_id = (i < static_cast<int>(entrances_->size())) 
          ? (*entrances_)[i].room_ : 0;

      // Include room ID in filter matching
      char filter_text[256];
      snprintf(filter_text, sizeof(filter_text), "%s %03X", 
               display_name.c_str(), room_id);

      if (entrance_filter_.PassFilter(filter_text)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        char label[32];
        snprintf(label, sizeof(label), "%02X", i);
        if (ImGui::Selectable(label, current_entrance_id_ == i,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          current_entrance_id_ = i;
          if (i < static_cast<int>(entrances_->size())) {
            // Use entrance callback if set, otherwise fall back to room callback
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
