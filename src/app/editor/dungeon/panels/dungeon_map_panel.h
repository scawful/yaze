#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_MAP_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_MAP_PANEL_H_

#include <array>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonMapPanel
 * @brief EditorPanel for displaying multiple rooms in a spatial dungeon layout
 *
 * This panel provides an overview of multiple dungeon rooms arranged spatially,
 * allowing users to see room connections and navigate between rooms quickly.
 * Unlike the 16x19 Room Matrix which shows ALL rooms, this panel shows a
 * focused subset of rooms that belong to a specific dungeon or user selection.
 *
 * Features:
 * - Spatial arrangement of 4-16 rooms
 * - Thumbnail previews of each room
 * - Room connection visualization
 * - Click to select
 *
 * @see DungeonRoomMatrixPanel - For full 296-room grid view
 * @see EditorPanel - Base interface
 */
class DungeonMapPanel : public EditorPanel {
 public:
  /**
   * @brief Construct a dungeon map panel
   * @param current_room_id Pointer to the current room ID (for highlighting)
   * @param active_rooms Pointer to list of currently open rooms
   * @param on_room_selected Callback when a room is clicked
   * @param rooms Pointer to room data array
   */
  DungeonMapPanel(int* current_room_id, ImVector<int>* active_rooms,
                  std::function<void(int)> on_room_selected,
                  std::array<zelda3::Room, 0x128>* rooms = nullptr)
      : current_room_id_(current_room_id),
        active_rooms_(active_rooms),
        rooms_(rooms),
        on_room_selected_(std::move(on_room_selected)) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.dungeon_map"; }
  std::string GetDisplayName() const override { return "Dungeon Map"; }
  std::string GetIcon() const override { return ICON_MD_MAP; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 35; }

  // ==========================================================================
  // Configuration
  // ==========================================================================

  /**
   * @brief Set which rooms to display in this dungeon map
   * @param room_ids Vector of room IDs to include
   */
  void SetDungeonRooms(const std::vector<int>& room_ids) {
    dungeon_room_ids_ = room_ids;
    AutoLayoutRooms();
  }

  /**
   * @brief Add a single room to the dungeon map
   */
  void AddRoom(int room_id) {
    // Avoid duplicates
    for (int id : dungeon_room_ids_) {
      if (id == room_id) return;
    }
    dungeon_room_ids_.push_back(room_id);
    AutoLayoutRooms();
  }

  /**
   * @brief Clear all rooms from the dungeon map
   */
  void ClearRooms() {
    dungeon_room_ids_.clear();
    room_positions_.clear();
  }

  /**
   * @brief Manually set a room's position in the grid
   */
  void SetRoomPosition(int room_id, int grid_x, int grid_y) {
    room_positions_[room_id] = ImVec2(static_cast<float>(grid_x),
                                       static_cast<float>(grid_y));
  }

  void SetRooms(std::array<zelda3::Room, 0x128>* rooms) { rooms_ = rooms; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!current_room_id_ || !active_rooms_) return;

    const auto& theme = AgentUI::GetTheme();

    // Show dungeon selection/quick presets
    DrawDungeonSelector();
    
    ImGui::Separator();

    // Room size in the map
    constexpr float kRoomWidth = 64.0f;
    constexpr float kRoomHeight = 64.0f;
    constexpr float kRoomSpacing = 8.0f;

    // Calculate canvas size based on room positions
    float max_x = 0, max_y = 0;
    for (const auto& [room_id, pos] : room_positions_) {
      max_x = std::max(max_x, pos.x);
      max_y = std::max(max_y, pos.y);
    }
    float canvas_width = (max_x + 1) * (kRoomWidth + kRoomSpacing) + kRoomSpacing;
    float canvas_height = (max_y + 1) * (kRoomHeight + kRoomSpacing) + kRoomSpacing;

    // Minimum size
    canvas_width = std::max(canvas_width, 200.0f);
    canvas_height = std::max(canvas_height, 200.0f);

    ImVec2 available = ImGui::GetContentRegionAvail();
    ImVec2 canvas_size(std::min(available.x, canvas_width),
                       std::min(available.y - 40, canvas_height));

    // Begin canvas area
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Background
    ImU32 bg_color = ImGui::ColorConvertFloat4ToU32(theme.panel_bg_darker);
    draw_list->AddRectFilled(canvas_pos, 
                             ImVec2(canvas_pos.x + canvas_size.x,
                                    canvas_pos.y + canvas_size.y),
                             bg_color);

    // Draw connections between adjacent rooms (before drawing rooms so lines are behind)
    ImVec4 connection_color = theme.dungeon_room_border_dark;
    connection_color.w = 0.45f;
    for (size_t i = 0; i < dungeon_room_ids_.size(); i++) {
      for (size_t j = i + 1; j < dungeon_room_ids_.size(); j++) {
        int room_a = dungeon_room_ids_[i];
        int room_b = dungeon_room_ids_[j];

        // Binary adjacency check (Z3 matrix neighbors)
        bool adjacent = false;
        if (std::abs(room_a - room_b) == 16) {
          adjacent = true;
        } else if (std::abs(room_a - room_b) == 1) {
          // Check same row (col check)
          int col_a = room_a % 16;
          int col_b = room_b % 16;
          if (std::abs(col_a - col_b) == 1) {
            adjacent = true;
          }
        }

        if (adjacent) {
          ImVec2 pos_a = room_positions_[room_a];
          ImVec2 pos_b = room_positions_[room_b];

          ImVec2 p_a(canvas_pos.x + kRoomSpacing +
                         pos_a.x * (kRoomWidth + kRoomSpacing) + kRoomWidth * 0.5f,
                     canvas_pos.y + kRoomSpacing +
                         pos_a.y * (kRoomHeight + kRoomSpacing) + kRoomHeight * 0.5f);
          ImVec2 p_b(canvas_pos.x + kRoomSpacing +
                         pos_b.x * (kRoomWidth + kRoomSpacing) + kRoomWidth * 0.5f,
                     canvas_pos.y + kRoomSpacing +
                         pos_b.y * (kRoomHeight + kRoomSpacing) + kRoomHeight * 0.5f);

          draw_list->AddLine(p_a, p_b,
                             ImGui::ColorConvertFloat4ToU32(connection_color),
                             1.5f);
        }
      }
    }

    // Draw each room
    for (int room_id : dungeon_room_ids_) {
      auto pos_it = room_positions_.find(room_id);
      if (pos_it == room_positions_.end()) continue;

      ImVec2 grid_pos = pos_it->second;
      ImVec2 room_min(
          canvas_pos.x + kRoomSpacing + grid_pos.x * (kRoomWidth + kRoomSpacing),
          canvas_pos.y + kRoomSpacing + grid_pos.y * (kRoomHeight + kRoomSpacing));
      ImVec2 room_max(room_min.x + kRoomWidth, room_min.y + kRoomHeight);

      // Check if room is valid
      if (room_id < 0 || room_id >= 0x128) continue;

      bool is_current = (*current_room_id_ == room_id);
      bool is_open = false;
      for (int i = 0; i < active_rooms_->Size; i++) {
        if ((*active_rooms_)[i] == room_id) {
          is_open = true;
          break;
        }
      }

      // Draw room thumbnail or placeholder
      if (rooms_ && (*rooms_)[room_id].IsLoaded()) {
        auto& bg1_bitmap = (*rooms_)[room_id].bg1_buffer().bitmap();
        if (bg1_bitmap.is_active() && bg1_bitmap.texture() != 0) {
          // Draw room thumbnail
          draw_list->AddImage(
              (ImTextureID)(intptr_t)bg1_bitmap.texture(),
              room_min, room_max);
        } else {
          // Placeholder for loaded but no texture
          draw_list->AddRectFilled(
              room_min, room_max,
              ImGui::ColorConvertFloat4ToU32(theme.panel_bg_color));
        }
      } else {
        // Not loaded - gray placeholder
        draw_list->AddRectFilled(
            room_min, room_max,
            ImGui::ColorConvertFloat4ToU32(theme.panel_bg_darker));
        
        // Show room ID
        char label[8];
        snprintf(label, sizeof(label), "%02X", room_id);
        ImVec2 text_size = ImGui::CalcTextSize(label);
        ImVec2 text_pos(room_min.x + (kRoomWidth - text_size.x) * 0.5f,
                        room_min.y + (kRoomHeight - text_size.y) * 0.5f);
        draw_list->AddText(
            text_pos,
            ImGui::ColorConvertFloat4ToU32(theme.text_secondary_gray), label);
      }

      // Draw border based on state
      if (is_current) {
        // Glow effect
        ImVec4 glow = theme.dungeon_selection_primary;
        glow.w = 0.4f;
        ImVec2 glow_min(room_min.x - 2, room_min.y - 2);
        ImVec2 glow_max(room_max.x + 2, room_max.y + 2);
        draw_list->AddRect(glow_min, glow_max,
                          ImGui::ColorConvertFloat4ToU32(glow), 0.0f, 0, 4.0f);
        // Inner border
        draw_list->AddRect(room_min, room_max,
                          ImGui::ColorConvertFloat4ToU32(
                              theme.dungeon_selection_primary),
                          0.0f, 0, 2.0f);
      } else if (is_open) {
        draw_list->AddRect(room_min, room_max,
                          ImGui::ColorConvertFloat4ToU32(
                              theme.dungeon_grid_cell_selected),
                          0.0f, 0, 2.0f);
      } else {
        draw_list->AddRect(room_min, room_max,
                          ImGui::ColorConvertFloat4ToU32(
                              theme.dungeon_grid_cell_border),
                          0.0f, 0, 1.0f);
      }

      // Handle clicks
      ImGui::SetCursorScreenPos(room_min);
      char btn_id[32];
      snprintf(btn_id, sizeof(btn_id), "##map_room%d", room_id);
      ImGui::InvisibleButton(btn_id, ImVec2(kRoomWidth, kRoomHeight));

      if (ImGui::IsItemClicked() && on_room_selected_) {
        on_room_selected_(room_id);
      }

      // Tooltip
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("[%03X] %s", room_id, 
                    zelda3::GetRoomLabel(room_id).c_str());
        if (rooms_ && (*rooms_)[room_id].IsLoaded()) {
          ImGui::TextDisabled("Palette: %d", (*rooms_)[room_id].palette);
        }
        ImGui::TextDisabled("Click to select");
        ImGui::EndTooltip();
      }
    }

    // Advance past canvas
    ImGui::Dummy(canvas_size);

    // Status bar
    ImGui::TextDisabled("%zu rooms in view", dungeon_room_ids_.size());
  }

 private:
  /**
   * @brief Auto-layout rooms in a grid based on their IDs
   */
  void AutoLayoutRooms() {
    room_positions_.clear();
    
    int cols = static_cast<int>(std::ceil(std::sqrt(
        static_cast<double>(dungeon_room_ids_.size()))));
    cols = std::max(1, cols);
    
    for (size_t i = 0; i < dungeon_room_ids_.size(); i++) {
      int room_id = dungeon_room_ids_[i];
      int grid_x = static_cast<int>(i % cols);
      int grid_y = static_cast<int>(i / cols);
      room_positions_[room_id] = ImVec2(static_cast<float>(grid_x),
                                         static_cast<float>(grid_y));
    }
  }

  /**
   * @brief Draw dungeon preset selector
   */
  void DrawDungeonSelector() {
    // Dungeon presets (approximate room ranges)
    struct DungeonPreset {
      const char* name;
      int start_room;
      int count;
    };
    
    static const DungeonPreset kPresets[] = {
        {"Eastern Palace", 0xC8, 8},
        {"Desert Palace", 0x33, 8},
        {"Tower of Hera", 0x07, 8},
        {"Palace of Darkness", 0x09, 12},
        {"Swamp Palace", 0x28, 10},
        {"Skull Woods", 0x29, 10},
        {"Thieves' Town", 0x44, 8},
        {"Ice Palace", 0x0E, 12},
        {"Misery Mire", 0x61, 10},
        {"Turtle Rock", 0x04, 12},
        {"Ganon's Tower", 0x0C, 16},
        {"Hyrule Castle", 0x01, 12},
    };

    if (ImGui::BeginCombo("##DungeonPreset", 
                          selected_preset_ >= 0 
                              ? kPresets[selected_preset_].name 
                              : "Select Dungeon...")) {
      for (int i = 0; i < IM_ARRAYSIZE(kPresets); i++) {
        if (ImGui::Selectable(kPresets[i].name, selected_preset_ == i)) {
          selected_preset_ = i;
          // Load rooms for this dungeon
          dungeon_room_ids_.clear();
          for (int j = 0; j < kPresets[i].count; j++) {
            int room_id = kPresets[i].start_room + j;
            if (room_id < 0x128) {
              dungeon_room_ids_.push_back(room_id);
            }
          }
          AutoLayoutRooms();
        }
      }
      ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_ADD " Add Current")) {
      if (current_room_id_ && *current_room_id_ >= 0) {
        AddRoom(*current_room_id_);
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Add currently selected room to the map");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CLEAR " Clear")) {
      ClearRooms();
      selected_preset_ = -1;
    }
  }

  int* current_room_id_ = nullptr;
  ImVector<int>* active_rooms_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  std::function<void(int)> on_room_selected_;

  std::vector<int> dungeon_room_ids_;
  std::map<int, ImVec2> room_positions_;
  int selected_preset_ = -1;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_MAP_PANEL_H_
