#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "zelda3/common.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonEntrancesPanel
 * @brief EditorPanel for displaying and editing dungeon entrances
 *
 * This panel provides a list of all dungeon entrances with their properties.
 * Users can select entrances to navigate to their associated rooms.
 *
 * @see EditorPanel - Base interface
 */
class DungeonEntrancesPanel : public EditorPanel {
 public:
  DungeonEntrancesPanel(
      std::array<zelda3::RoomEntrance, 0x8C>* entrances,
      int* current_entrance_id,
      std::function<void(int)> on_entrance_selected)
      : entrances_(entrances),
        current_entrance_id_(current_entrance_id),
        on_entrance_selected_(std::move(on_entrance_selected)) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.entrance_properties"; }
  std::string GetDisplayName() const override { return "Entrance Properties"; }
  std::string GetIcon() const override { return ICON_MD_TUNE; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 26; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!entrances_ || !current_entrance_id_) return;

    auto& current_entrance = (*entrances_)[*current_entrance_id_];

    // Entrance properties
    gui::InputHexWord("Entrance ID", &current_entrance.entrance_id_);
    gui::InputHexWord("Room ID",
                      reinterpret_cast<uint16_t*>(&current_entrance.room_));
    ImGui::SameLine();
    gui::InputHexByte("Dungeon ID", &current_entrance.dungeon_id_, 50.f, true);

    gui::InputHexByte("Blockset", &current_entrance.blockset_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("Music", &current_entrance.music_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("Floor", &current_entrance.floor_);

    ImGui::Separator();

    gui::InputHexWord("Player X   ", &current_entrance.x_position_);
    ImGui::SameLine();
    gui::InputHexWord("Player Y   ", &current_entrance.y_position_);

    gui::InputHexWord("Camera X", &current_entrance.camera_trigger_x_);
    ImGui::SameLine();
    gui::InputHexWord("Camera Y", &current_entrance.camera_trigger_y_);

    gui::InputHexWord("Scroll X    ", &current_entrance.camera_x_);
    ImGui::SameLine();
    gui::InputHexWord("Scroll Y    ", &current_entrance.camera_y_);

    gui::InputHexWord("Exit",
                      reinterpret_cast<uint16_t*>(&current_entrance.exit_),
                      50.f, true);

    ImGui::Separator();
    ImGui::Text("Camera Boundaries");
    ImGui::Separator();
    ImGui::Text("\t\t\t\t\tNorth         East         South         West");

    gui::InputHexByte("Quadrant", &current_entrance.camera_boundary_qn_, 50.f,
                      true);
    ImGui::SameLine();
    gui::InputHexByte("##QE", &current_entrance.camera_boundary_qe_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("##QS", &current_entrance.camera_boundary_qs_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("##QW", &current_entrance.camera_boundary_qw_, 50.f, true);

    gui::InputHexByte("Full room", &current_entrance.camera_boundary_fn_, 50.f,
                      true);
    ImGui::SameLine();
    gui::InputHexByte("##FE", &current_entrance.camera_boundary_fe_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("##FS", &current_entrance.camera_boundary_fs_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("##FW", &current_entrance.camera_boundary_fw_, 50.f, true);

    ImGui::Separator();

    // Entrance list
    // Array layout (from LoadRoomEntrances):
    //   indices 0-6 (0x00-0x06): Spawn points (7 entries)
    //   indices 7-139 (0x07-0x8B): Regular entrances (133 entries)
    constexpr int kNumSpawnPoints = 7;
    constexpr int kNumEntrances = 133;
    constexpr int kTotalEntries = 140;

    if (ImGui::BeginChild("##EntrancesList", ImVec2(0, 0), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      for (int i = 0; i < kTotalEntries; i++) {
        std::string entrance_name;
        if (i < kNumSpawnPoints) {
          // Spawn points at indices 0-6
          char buf[32];
          snprintf(buf, sizeof(buf), "Spawn Point %d", i);
          entrance_name = buf;
        } else {
          // Regular entrances at indices 7-139, mapped to kEntranceNames[0-132]
          int entrance_id = i - kNumSpawnPoints;
          if (entrance_id < kNumEntrances) {
            // Use unified ResourceLabelProvider for entrance names
            entrance_name = zelda3::GetEntranceLabel(entrance_id);
          } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "Unknown %d", i);
            entrance_name = buf;
          }
        }

        int room_id = (*entrances_)[i].room_;
        // Use unified ResourceLabelProvider for room names
        std::string room_name = zelda3::GetRoomLabel(room_id);

        char label[256];
        snprintf(label, sizeof(label), "[%02X] %s -> %s (%03X)", i,
                 entrance_name.c_str(), room_name.c_str(), room_id);

        bool is_selected = (*current_entrance_id_ == i);
        if (ImGui::Selectable(label, is_selected)) {
          *current_entrance_id_ = i;
          if (on_entrance_selected_) {
            on_entrance_selected_(i);
          }
        }
      }
    }
    ImGui::EndChild();
  }

 private:
  std::array<zelda3::RoomEntrance, 0x8C>* entrances_ = nullptr;
  int* current_entrance_id_ = nullptr;
  std::function<void(int)> on_entrance_selected_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_
