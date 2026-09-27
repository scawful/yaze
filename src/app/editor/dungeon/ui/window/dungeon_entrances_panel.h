#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_

#include <array>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_entrance_edit_policy.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
#include "zelda3/common.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonEntrancesPanel
 * @brief WindowContent for displaying and editing dungeon entrances
 *
 * This panel provides a list of all dungeon entrances with their properties.
 * Users can select entrances to navigate to their associated rooms.
 *
 * @see WindowContent - Base interface
 */
class DungeonEntrancesPanel : public WindowContent {
 public:
  DungeonEntrancesPanel(
      std::array<zelda3::RoomEntrance, zelda3::kNumDungeonEntranceSlots>*
          entrances,
      std::array<zelda3::DungeonSpawnPoint, zelda3::kNumDungeonSpawnPoints>*
          spawn_points,
      int* current_entrance_id, std::function<void(int)> on_entrance_selected)
      : entrances_(entrances),
        spawn_points_(spawn_points),
        current_entrance_id_(current_entrance_id),
        on_entrance_selected_(std::move(on_entrance_selected)) {}

  // ==========================================================================
  // WindowContent Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.entrance_properties"; }
  std::string GetDisplayName() const override { return "Entrances"; }
  std::string GetIcon() const override { return ICON_MD_DOOR_FRONT; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 26; }
  std::string GetWorkflowGroup() const override { return "Core"; }

  // ==========================================================================
  // WindowContent Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    (void)p_open;
    if (!entrances_ || !spawn_points_ || !current_entrance_id_)
      return;
    if (*current_entrance_id_ < 0 ||
        *current_entrance_id_ >= static_cast<int>(entrances_->size())) {
      *current_entrance_id_ = 0;
    }

    const bool split_layout = ImGui::GetContentRegionAvail().x >= 620.0f;
    if (split_layout &&
        ImGui::BeginTable("##EntranceNavigator", 2,
                          ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Entrances", ImGuiTableColumnFlags_WidthFixed,
                              300.0f);
      ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      DrawEntranceList();
      ImGui::TableNextColumn();
      DrawSelectedProperties();
      ImGui::EndTable();
      return;
    }

    if (ImGui::BeginTabBar("##EntranceNavigatorTabs")) {
      if (ImGui::BeginTabItem(ICON_MD_DOOR_FRONT " Entrances")) {
        DrawEntranceList();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(ICON_MD_TUNE " Properties")) {
        DrawSelectedProperties();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }

 private:
  void DrawSelectedProperties() {
    if (*current_entrance_id_ < zelda3::kNumDungeonSpawnPoints) {
      DrawSpawnPointProperties(*current_entrance_id_);
    } else {
      DrawRegularEntranceProperties(*current_entrance_id_);
    }
  }

  void DrawEntranceList() {
    entrance_filter_.Draw(ICON_MD_SEARCH " Filter",
                          ImGui::GetContentRegionAvail().x);
    constexpr int kNumSpawnPoints = zelda3::kNumDungeonSpawnPoints;
    constexpr int kNumEntrances = zelda3::kNumRegularDungeonEntrances;
    constexpr int kTotalEntries = zelda3::kNumDungeonEntranceSlots;

    if (ImGui::BeginChild("##EntrancesList", ImVec2(0, 0), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      for (int i = 0; i < kTotalEntries; i++) {
        std::string entrance_name;
        if (i < kNumSpawnPoints) {
          entrance_name = absl::StrFormat("Spawn Point %d", i);
        } else {
          const int entrance_id = i - kNumSpawnPoints;
          entrance_name = entrance_id < kNumEntrances
                              ? zelda3::GetEntranceLabel(entrance_id)
                              : absl::StrFormat("Unknown %d", i);
        }

        const int room_id = i < kNumSpawnPoints ? (*spawn_points_)[i].room_id
                                                : (*entrances_)[i].room_;
        const std::string room_name = zelda3::GetRoomLabel(room_id);
        const std::string label = absl::StrFormat(
            "[%02X] %s -> %s (%03X)", i, entrance_name, room_name, room_id);
        if (!entrance_filter_.PassFilter(label.c_str())) {
          continue;
        }

        const bool is_selected = (*current_entrance_id_ == i);
        if (ImGui::Selectable(label.c_str(), is_selected)) {
          *current_entrance_id_ = i;
          if (on_entrance_selected_) {
            on_entrance_selected_(i);
          }
        }
      }
    }
    ImGui::EndChild();
  }

  void DrawSpawnPointProperties(int slot_index) {
    auto& spawn = (*spawn_points_)[slot_index];
    const bool properties_editable =
        CanEditDungeonSpawnPoint(slot_index, spawn);
    bool changed = false;

    ImGui::Text(tr("Spawn Point %d"), slot_index);
    if (!properties_editable) {
      ImGui::TextWrapped(tr(
          "Spawn point data is unavailable; reload the ROM before editing."));
    }
    ImGui::BeginDisabled(!properties_editable);

    changed |= gui::InputHexWord("Entrance ID", &spawn.entrance_id);
    changed |= gui::InputHexWord("Room ID", &spawn.room_id);
    ImGui::SameLine();
    changed |= gui::InputHexByte("Dungeon ID", &spawn.dungeon_id, 50.f, true);

    changed |= gui::InputHexByte("Main GFX", &spawn.main_gfx, 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte("Song", &spawn.song, 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte("Floor", &spawn.floor);

    ImGui::Separator();

    changed |= gui::InputHexWord("Player X", &spawn.x_coordinate);
    ImGui::SameLine();
    changed |= gui::InputHexWord("Player Y", &spawn.y_coordinate);

    changed |= gui::InputHexWord("Camera Trigger X", &spawn.camera_trigger_x);
    ImGui::SameLine();
    changed |= gui::InputHexWord("Camera Trigger Y", &spawn.camera_trigger_y);

    changed |= gui::InputHexWord("Horizontal Scroll", &spawn.horizontal_scroll);
    ImGui::SameLine();
    changed |= gui::InputHexWord("Vertical Scroll", &spawn.vertical_scroll);

    changed |= gui::InputHexWord("Overworld Door Tilemap",
                                 &spawn.overworld_door_tilemap, 70.f, true);

    ImGui::Separator();
    changed |= gui::InputHexByte("Layer", &spawn.layer, 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte("Spawn Quadrant", &spawn.quadrant, 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte("Scroll Controller",
                                 &spawn.camera_scroll_controller, 50.f, true);

    ImGui::Separator();
    ImGui::Text(tr("Camera Boundaries"));
    ImGui::Separator();
    ImGui::Text(tr("\t\t\t\t\tNorth         East         South         West"));

    changed |=
        gui::InputHexByte("Quadrant##SpawnBoundaryQN",
                          &spawn.camera_scroll_boundaries[0], 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte(
        "##SpawnQE", &spawn.camera_scroll_boundaries[6], 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte(
        "##SpawnQS", &spawn.camera_scroll_boundaries[2], 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte(
        "##SpawnQW", &spawn.camera_scroll_boundaries[4], 50.f, true);

    changed |= gui::InputHexByte(
        "Full room", &spawn.camera_scroll_boundaries[1], 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte(
        "##SpawnFE", &spawn.camera_scroll_boundaries[7], 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte(
        "##SpawnFS", &spawn.camera_scroll_boundaries[3], 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte(
        "##SpawnFW", &spawn.camera_scroll_boundaries[5], 50.f, true);
    ImGui::EndDisabled();

    MarkDungeonSpawnPointDirtyIfEditable(slot_index, spawn, changed);
  }

  void DrawRegularEntranceProperties(int slot_index) {
    auto& entrance = (*entrances_)[slot_index];
    const bool properties_editable =
        CanEditDungeonEntrance(slot_index, entrance);
    bool changed = false;

    ImGui::Text(tr("Entrance ID: %04X"), entrance.entrance_id_);
    ImGui::BeginDisabled(!properties_editable);
    changed |= gui::InputHexWord("Room ID", &entrance.room_);
    ImGui::SameLine();
    changed |=
        gui::InputHexByte("Dungeon ID", &entrance.dungeon_id_, 50.f, true);

    changed |= gui::InputHexByte("Blockset", &entrance.blockset_, 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte("Music", &entrance.music_, 50.f, true);
    ImGui::SameLine();
    changed |= gui::InputHexByte("Floor", &entrance.floor_);

    ImGui::Separator();

    changed |= gui::InputHexWord("Player X   ", &entrance.x_position_);
    ImGui::SameLine();
    changed |= gui::InputHexWord("Player Y   ", &entrance.y_position_);

    changed |= gui::InputHexWord("Camera X", &entrance.camera_trigger_x_);
    ImGui::SameLine();
    changed |= gui::InputHexWord("Camera Y", &entrance.camera_trigger_y_);

    changed |= gui::InputHexWord("Scroll X    ", &entrance.camera_x_);
    ImGui::SameLine();
    changed |= gui::InputHexWord("Scroll Y    ", &entrance.camera_y_);

    changed |= gui::InputHexWord("Exit", &entrance.exit_, 50.f, true);

    ImGui::Separator();
    ImGui::Text(tr("Camera Boundaries"));
    ImGui::Separator();
    ImGui::Text(tr("\t\t\t\t\tNorth         East         South         West"));

    changed |= gui::InputHexByte("Quadrant", &entrance.camera_boundary_qn_,
                                 50.f, true);
    ImGui::SameLine();
    changed |=
        gui::InputHexByte("##QE", &entrance.camera_boundary_qe_, 50.f, true);
    ImGui::SameLine();
    changed |=
        gui::InputHexByte("##QS", &entrance.camera_boundary_qs_, 50.f, true);
    ImGui::SameLine();
    changed |=
        gui::InputHexByte("##QW", &entrance.camera_boundary_qw_, 50.f, true);

    changed |= gui::InputHexByte("Full room", &entrance.camera_boundary_fn_,
                                 50.f, true);
    ImGui::SameLine();
    changed |=
        gui::InputHexByte("##FE", &entrance.camera_boundary_fe_, 50.f, true);
    ImGui::SameLine();
    changed |=
        gui::InputHexByte("##FS", &entrance.camera_boundary_fs_, 50.f, true);
    ImGui::SameLine();
    changed |=
        gui::InputHexByte("##FW", &entrance.camera_boundary_fw_, 50.f, true);
    ImGui::EndDisabled();

    MarkDungeonEntranceDirtyIfEditable(slot_index, entrance, changed);
  }

  std::array<zelda3::RoomEntrance, zelda3::kNumDungeonEntranceSlots>*
      entrances_ = nullptr;
  std::array<zelda3::DungeonSpawnPoint, zelda3::kNumDungeonSpawnPoints>*
      spawn_points_ = nullptr;
  int* current_entrance_id_ = nullptr;
  std::function<void(int)> on_entrance_selected_;
  ImGuiTextFilter entrance_filter_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_
