#include "dungeon_room_selector.h"

#include "absl/strings/str_format.h"
#include "app/gui/input.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/room_entrance.h"
#include "imgui/imgui.h"
#include "util/hex.h"

namespace yaze::editor {

using ImGui::BeginChild;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::EndChild;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::Separator;
using ImGui::SameLine;
using ImGui::Text;

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

  if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)9);
      BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    int i = 0;
    for (const auto each_room_name : zelda3::kRoomNames) {
      rom_->resource_label()->SelectableLabelWithNameEdit(
          current_room_id_ == i, "Dungeon Room Names", util::HexByte(i),
          each_room_name.data());
      if (ImGui::IsItemClicked()) {
        current_room_id_ = i;
        if (!active_rooms_.contains(i)) {
          active_rooms_.push_back(i);
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
  Separator();

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

  Separator();
  ImGui::Text("Camera Boundaries");
  Separator();
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
    for (int i = 0; i < 0x85 + 7; i++) {
      rom_->resource_label()->SelectableLabelWithNameEdit(
          current_entrance_id_ == i, "Dungeon Entrance Names",
          util::HexByte(i), zelda3::kEntranceNames[i].data());

      if (ImGui::IsItemClicked()) {
        current_entrance_id_ = i;
        if (!active_rooms_.contains(i)) {
          active_rooms_.push_back((*entrances_)[i].room_);
        }
      }
    }
  }
  EndChild();
}

}  // namespace yaze::editor
