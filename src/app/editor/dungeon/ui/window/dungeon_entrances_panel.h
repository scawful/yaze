#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_entrance_camera.h"
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

  using CameraStateProvider =
      std::function<std::optional<DungeonEntranceCameraState>(int)>;
  using CameraRepairCallback = std::function<absl::Status(int)>;

  void SetCameraTools(CameraStateProvider state_provider,
                      CameraRepairCallback repair_callback,
                      std::function<void()> data_changed_callback = {}) {
    camera_state_provider_ = std::move(state_provider);
    camera_repair_callback_ = std::move(repair_callback);
    camera_data_changed_callback_ = std::move(data_changed_callback);
  }
  bool OwnsNavigationShortcutFocus() const {
    if (!navigation_shortcut_focus_ || ImGui::GetCurrentContext() == nullptr) {
      return false;
    }
    return navigation_shortcut_last_draw_frame_ >= ImGui::GetFrameCount() - 1;
  }

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
    navigation_shortcut_last_draw_frame_ = ImGui::GetFrameCount();
    navigation_shortcut_focus_ =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

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
  void DrawCameraRepairPreview() {
    if (!camera_repair_preview_.has_value()) {
      return;
    }
    const auto validation =
        ValidateDungeonEntranceCamera(*camera_repair_preview_);
    if (!ImGui::BeginPopupModal("Camera Repair Preview##DungeonEntrance",
                                nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      return;
    }

    ImGui::TextWrapped(
        "Review the derived camera values before changing this entrance. "
        "The repair is one undoable action.");
    ImGui::Separator();
    if (ImGui::BeginTable(
            "##CameraRepairValues", 3,
            ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("Field");
      ImGui::TableSetupColumn("Current");
      ImGui::TableSetupColumn("Derived");
      ImGui::TableHeadersRow();
      const auto draw_word_row = [](const char* label, uint16_t current,
                                    uint16_t derived) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::Text("0x%04X", current);
        ImGui::TableNextColumn();
        ImGui::Text("0x%04X", derived);
      };
      draw_word_row("Scroll X", camera_repair_preview_->camera_x,
                    validation.expected.camera_x);
      draw_word_row("Scroll Y", camera_repair_preview_->camera_y,
                    validation.expected.camera_y);
      draw_word_row("Trigger X", camera_repair_preview_->trigger_x,
                    validation.expected.trigger_x);
      draw_word_row("Trigger Y", camera_repair_preview_->trigger_y,
                    validation.expected.trigger_y);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted("Quadrant");
      ImGui::TableNextColumn();
      ImGui::Text("0x%02X", camera_repair_preview_->quadrant);
      ImGui::TableNextColumn();
      if (validation.quadrant_repair_safe) {
        ImGui::Text("0x%02X", validation.expected.quadrant);
      } else {
        ImGui::TextUnformatted("Preserved");
      }
      ImGui::EndTable();
    }
    ImGui::TextWrapped(
        "Boundary pages: [%02X %02X %02X %02X | %02X %02X %02X %02X] "
        "-> [%02X %02X %02X %02X | %02X %02X %02X %02X]",
        camera_repair_preview_->boundaries[0],
        camera_repair_preview_->boundaries[1],
        camera_repair_preview_->boundaries[2],
        camera_repair_preview_->boundaries[3],
        camera_repair_preview_->boundaries[4],
        camera_repair_preview_->boundaries[5],
        camera_repair_preview_->boundaries[6],
        camera_repair_preview_->boundaries[7],
        validation.expected.boundaries[0], validation.expected.boundaries[1],
        validation.expected.boundaries[2], validation.expected.boundaries[3],
        validation.expected.boundaries[4], validation.expected.boundaries[5],
        validation.expected.boundaries[6], validation.expected.boundaries[7]);
    if (!camera_repair_error_.empty()) {
      ImGui::TextWrapped("Repair failed: %s", camera_repair_error_.c_str());
    }

    ImGui::BeginDisabled(!validation.can_repair() || !camera_repair_callback_);
    if (ImGui::Button("Apply Safe Repair")) {
      const absl::Status status =
          camera_repair_callback_
              ? camera_repair_callback_(camera_repair_slot_)
              : absl::FailedPreconditionError(
                    "Camera repair callback is unavailable");
      if (status.ok()) {
        camera_repair_preview_.reset();
        camera_repair_slot_ = -1;
        camera_repair_error_.clear();
        ImGui::CloseCurrentPopup();
      } else {
        camera_repair_error_ = std::string(status.message());
      }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      camera_repair_preview_.reset();
      camera_repair_slot_ = -1;
      camera_repair_error_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  void DrawCameraSafety(int slot_index) {
    if (!camera_state_provider_) {
      return;
    }
    const auto state = camera_state_provider_(slot_index);
    if (!state.has_value()) {
      ImGui::TextDisabled("Camera validation unavailable.");
      return;
    }
    const auto validation = ValidateDungeonEntranceCamera(*state);
    ImGui::SeparatorText("Camera safety");
    if (!validation.geometry_valid()) {
      ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f),
                         "Unsafe camera geometry");
      for (const auto& error : validation.errors) {
        ImGui::BulletText("%s", error.c_str());
      }
      if (validation.can_repair()) {
        ImGui::TextWrapped(
            "The room and player position are valid, so a previewed repair "
            "can replace these derived fields.");
      }
    } else if (validation.matches_derived()) {
      ImGui::TextColored(ImVec4(0.35f, 0.80f, 0.45f, 1.0f),
                         "Camera values match the player position.");
    } else {
      ImGui::TextWrapped(
          "Camera geometry is safe, but %zu derived value group(s) differ.",
          validation.differences.size());
      for (const auto& difference : validation.differences) {
        ImGui::BulletText("%s", difference.c_str());
      }
    }

    ImGui::BeginDisabled(!validation.can_repair() || !camera_repair_callback_);
    if (ImGui::Button("Preview Camera Repair")) {
      camera_repair_preview_ = *state;
      camera_repair_slot_ = slot_index;
      camera_repair_error_.clear();
      ImGui::OpenPopup("Camera Repair Preview##DungeonEntrance");
    }
    ImGui::EndDisabled();
    DrawCameraRepairPreview();
  }

  void DrawSelectedProperties() {
    if (*current_entrance_id_ < zelda3::kNumDungeonSpawnPoints) {
      DrawSpawnPointProperties(*current_entrance_id_);
    } else {
      DrawRegularEntranceProperties(*current_entrance_id_);
    }
  }

  bool DrawCameraBoundaries(const char* table_id,
                            const std::array<uint8_t*, 8>& boundaries) {
    bool changed = false;
    if (!ImGui::BeginTable(table_id, 5,
                           ImGuiTableFlags_BordersInnerV |
                               ImGuiTableFlags_SizingStretchSame)) {
      return false;
    }
    ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("North");
    ImGui::TableSetupColumn("East");
    ImGui::TableSetupColumn("South");
    ImGui::TableSetupColumn("West");
    ImGui::TableHeadersRow();
    constexpr std::array<int, 4> kQuadrantOrder = {0, 6, 2, 4};
    constexpr std::array<int, 4> kFullRoomOrder = {1, 7, 3, 5};
    const auto draw_row = [&changed, &boundaries](
                              const char* label,
                              const std::array<int, 4>& order) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(label);
      for (const int index : order) {
        ImGui::TableNextColumn();
        ImGui::PushID(index);
        changed |= gui::InputHexByte(
            "##Boundary", boundaries[index],
            std::max(36.0f, ImGui::GetContentRegionAvail().x), true);
        ImGui::PopID();
      }
    };
    draw_row("Quadrant", kQuadrantOrder);
    draw_row("Full room", kFullRoomOrder);
    ImGui::EndTable();
    return changed;
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

    changed |= gui::InputHexWord("Overworld Door Tilemap",
                                 &spawn.overworld_door_tilemap, 70.f, true);
    ImGui::EndDisabled();

    DrawCameraSafety(slot_index);

    if (ImGui::CollapsingHeader("Advanced camera data")) {
      ImGui::BeginDisabled(!properties_editable);
      changed |= gui::InputHexWord("Camera Trigger X", &spawn.camera_trigger_x);
      ImGui::SameLine();
      changed |= gui::InputHexWord("Camera Trigger Y", &spawn.camera_trigger_y);

      changed |=
          gui::InputHexWord("Horizontal Scroll", &spawn.horizontal_scroll);
      ImGui::SameLine();
      changed |= gui::InputHexWord("Vertical Scroll", &spawn.vertical_scroll);

      changed |= gui::InputHexByte("Layer", &spawn.layer, 50.f, true);
      ImGui::SameLine();
      changed |=
          gui::InputHexByte("Spawn Quadrant", &spawn.quadrant, 50.f, true);
      ImGui::SameLine();
      changed |= gui::InputHexByte("Scroll Controller",
                                   &spawn.camera_scroll_controller, 50.f, true);

      ImGui::SeparatorText(tr("Camera Boundaries"));
      changed |= DrawCameraBoundaries("##SpawnCameraBoundaries",
                                      {&spawn.camera_scroll_boundaries[0],
                                       &spawn.camera_scroll_boundaries[1],
                                       &spawn.camera_scroll_boundaries[2],
                                       &spawn.camera_scroll_boundaries[3],
                                       &spawn.camera_scroll_boundaries[4],
                                       &spawn.camera_scroll_boundaries[5],
                                       &spawn.camera_scroll_boundaries[6],
                                       &spawn.camera_scroll_boundaries[7]});
      ImGui::EndDisabled();
    }

    MarkDungeonSpawnPointDirtyIfEditable(slot_index, spawn, changed);
    if (changed && camera_data_changed_callback_) {
      camera_data_changed_callback_();
    }
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

    changed |= gui::InputHexWord("Exit", &entrance.exit_, 50.f, true);
    ImGui::EndDisabled();

    DrawCameraSafety(slot_index);

    if (ImGui::CollapsingHeader("Advanced camera data")) {
      ImGui::BeginDisabled(!properties_editable);
      changed |=
          gui::InputHexWord("Camera Trigger X", &entrance.camera_trigger_x_);
      ImGui::SameLine();
      changed |=
          gui::InputHexWord("Camera Trigger Y", &entrance.camera_trigger_y_);

      changed |= gui::InputHexWord("Scroll X", &entrance.camera_x_);
      ImGui::SameLine();
      changed |= gui::InputHexWord("Scroll Y", &entrance.camera_y_);
      changed |= gui::InputHexByte("Scroll Quadrant",
                                   &entrance.scroll_quadrant_, 50.f, true);

      ImGui::SeparatorText(tr("Camera Boundaries"));
      changed |= DrawCameraBoundaries(
          "##RegularCameraBoundaries",
          {&entrance.camera_boundary_qn_, &entrance.camera_boundary_fn_,
           &entrance.camera_boundary_qs_, &entrance.camera_boundary_fs_,
           &entrance.camera_boundary_qw_, &entrance.camera_boundary_fw_,
           &entrance.camera_boundary_qe_, &entrance.camera_boundary_fe_});
      ImGui::EndDisabled();
    }

    MarkDungeonEntranceDirtyIfEditable(slot_index, entrance, changed);
    if (changed && camera_data_changed_callback_) {
      camera_data_changed_callback_();
    }
  }

  std::array<zelda3::RoomEntrance, zelda3::kNumDungeonEntranceSlots>*
      entrances_ = nullptr;
  std::array<zelda3::DungeonSpawnPoint, zelda3::kNumDungeonSpawnPoints>*
      spawn_points_ = nullptr;
  int* current_entrance_id_ = nullptr;
  std::function<void(int)> on_entrance_selected_;
  CameraStateProvider camera_state_provider_;
  CameraRepairCallback camera_repair_callback_;
  std::function<void()> camera_data_changed_callback_;
  std::optional<DungeonEntranceCameraState> camera_repair_preview_;
  int camera_repair_slot_ = -1;
  std::string camera_repair_error_;
  bool navigation_shortcut_focus_ = false;
  int navigation_shortcut_last_draw_frame_ = -1;
  ImGuiTextFilter entrance_filter_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_
