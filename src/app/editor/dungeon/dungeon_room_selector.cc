#include "dungeon_room_selector.h"

#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "util/hex.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze::editor {

using ImGui::BeginChild;
using ImGui::EndChild;
using ImGui::SameLine;

void DungeonRoomSelector::Draw() {
  if (ImGui::BeginTabBar("##DungeonRoomTabBar")) {
    if (ImGui::BeginTabItem("Rooms")) {
      DrawRoomSelector();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Entrances")) {
      DrawEntranceSelector();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void DungeonRoomSelector::DrawRoomSelector() {
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return;
  }

  gui::InputHexWord("Room ID", &current_room_id_, 50.f, true);

  if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)9);
      BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    int i = 0;
    for (const auto each_room_name : zelda3::kRoomNames) {
      rom_->resource_label()->SelectableLabelWithNameEdit(
          current_room_id_ == i, "Dungeon Room Names", util::HexByte(i),
          each_room_name.data());
      if (ImGui::IsItemClicked()) {
        current_room_id_ = i;
        // Notify the dungeon editor about room selection
        if (room_selected_callback_) {
          room_selected_callback_(i);
        }
      }
      i += 1;
    }
  }
  EndChild();
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
  gui::InputHexWord("Entrance ID", &current_entrance.entrance_id_);
  gui::InputHexWord("Room ID", &current_entrance.room_);
  SameLine();

  gui::InputHexByte("Dungeon ID", &current_entrance.dungeon_id_, 50.f, true);
  gui::InputHexByte("Blockset", &current_entrance.blockset_, 50.f, true);
  SameLine();

  gui::InputHexByte("Music", &current_entrance.music_, 50.f, true);
  SameLine();
  gui::InputHexByte("Floor", &current_entrance.floor_);
  ImGui::Separator();

  gui::InputHexWord("Player X   ", &current_entrance.x_position_);
  SameLine();
  gui::InputHexWord("Player Y   ", &current_entrance.y_position_);

  gui::InputHexWord("Camera X", &current_entrance.camera_trigger_x_);
  SameLine();
  gui::InputHexWord("Camera Y", &current_entrance.camera_trigger_y_);

  gui::InputHexWord("Scroll X    ", &current_entrance.camera_x_);
  SameLine();
  gui::InputHexWord("Scroll Y    ", &current_entrance.camera_y_);

  gui::InputHexWord("Exit", &current_entrance.exit_, 50.f, true);

  ImGui::Separator();
  ImGui::Text("Camera Boundaries");
  ImGui::Separator();
  ImGui::Text("\t\t\t\t\tNorth         East         South         West");
  gui::InputHexByte("Quadrant", &current_entrance.camera_boundary_qn_, 50.f,
                    true);
  SameLine();
  gui::InputHexByte("", &current_entrance.camera_boundary_qe_, 50.f, true);
  SameLine();
  gui::InputHexByte("", &current_entrance.camera_boundary_qs_, 50.f, true);
  SameLine();
  gui::InputHexByte("", &current_entrance.camera_boundary_qw_, 50.f, true);

  gui::InputHexByte("Full room", &current_entrance.camera_boundary_fn_, 50.f,
                    true);
  SameLine();
  gui::InputHexByte("", &current_entrance.camera_boundary_fe_, 50.f, true);
  SameLine();
  gui::InputHexByte("", &current_entrance.camera_boundary_fs_, 50.f, true);
  SameLine();
  gui::InputHexByte("", &current_entrance.camera_boundary_fw_, 50.f, true);

  if (BeginChild("EntranceSelector", ImVec2(0, 0), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    for (int i = 0; i < 0x8C; i++) {
      // The last seven are the spawn points
      auto entrance_name = absl::StrFormat("Spawn Point %d", i - 0x85);
      if (i < 0x85) {
        entrance_name = std::string(zelda3::kEntranceNames[i]);
      }
      rom_->resource_label()->SelectableLabelWithNameEdit(
          current_entrance_id_ == i, "Dungeon Entrance Names", util::HexByte(i),
          entrance_name);

      if (ImGui::IsItemClicked()) {
        current_entrance_id_ = i;
        if (i < entrances_->size()) {
          int room_id = (*entrances_)[i].room_;
          // Notify the dungeon editor about room selection
          if (room_selected_callback_) {
            room_selected_callback_(room_id);
          }
        }
      }
    }
  }
  EndChild();
}

}  // namespace yaze::editor
