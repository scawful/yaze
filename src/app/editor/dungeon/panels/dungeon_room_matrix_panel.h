#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_MATRIX_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_MATRIX_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonRoomMatrixPanel
 * @brief EditorPanel for displaying a visual 16x19 grid of all dungeon rooms
 *
 * This panel provides a compact overview of all 296 dungeon rooms in a matrix
 * layout. Users can click on cells to select and open rooms.
 *
 * @see EditorPanel - Base interface
 */
class DungeonRoomMatrixPanel : public EditorPanel {
 public:
  /**
   * @brief Construct a room matrix panel
   * @param current_room_id Pointer to the current room ID (for highlighting)
   * @param active_rooms Pointer to list of currently open rooms
   * @param on_room_selected Callback when a room is clicked
   */
  DungeonRoomMatrixPanel(int* current_room_id, ImVector<int>* active_rooms,
                         std::function<void(int)> on_room_selected)
      : current_room_id_(current_room_id),
        active_rooms_(active_rooms),
        on_room_selected_(std::move(on_room_selected)) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.room_matrix"; }
  std::string GetDisplayName() const override { return "Room Matrix"; }
  std::string GetIcon() const override { return ICON_MD_GRID_VIEW; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 30; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!current_room_id_ || !active_rooms_) return;

    // 16 wide x 19 tall = 304 cells (296 rooms + 8 empty)
    constexpr int kRoomsPerRow = 16;
    constexpr int kRoomsPerCol = 19;
    constexpr int kTotalRooms = 0x128;      // 296 rooms (0x00-0x127)
    constexpr float kRoomCellSize = 24.0f;  // Smaller cells like ZScream
    constexpr float kCellSpacing = 1.0f;    // Tighter spacing

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

    int room_index = 0;
    for (int row = 0; row < kRoomsPerCol; row++) {
      for (int col = 0; col < kRoomsPerRow; col++) {
        int room_id = room_index;
        bool is_valid_room = (room_id < kTotalRooms);

        ImVec2 cell_min =
            ImVec2(canvas_pos.x + col * (kRoomCellSize + kCellSpacing),
                   canvas_pos.y + row * (kRoomCellSize + kCellSpacing));
        ImVec2 cell_max =
            ImVec2(cell_min.x + kRoomCellSize, cell_min.y + kRoomCellSize);

        if (is_valid_room) {
          // Generate color from room ID
          int hue = (room_id * 37) % 360;
          int saturation = 40 + (room_id % 3) * 15;
          int brightness = 50 + (room_id % 5) * 10;

          // Convert HSV to RGB
          float h = hue / 60.0f;
          float s = saturation / 100.0f;
          float v = brightness / 100.0f;

          int i = static_cast<int>(h);
          float f = h - i;
          int p = static_cast<int>(v * (1 - s) * 255);
          int q = static_cast<int>(v * (1 - s * f) * 255);
          int t = static_cast<int>(v * (1 - s * (1 - f)) * 255);
          int val = static_cast<int>(v * 255);

          ImU32 bg_color;
          switch (i % 6) {
            case 0: bg_color = IM_COL32(val, t, p, 255); break;
            case 1: bg_color = IM_COL32(q, val, p, 255); break;
            case 2: bg_color = IM_COL32(p, val, t, 255); break;
            case 3: bg_color = IM_COL32(p, q, val, 255); break;
            case 4: bg_color = IM_COL32(t, p, val, 255); break;
            case 5: bg_color = IM_COL32(val, p, q, 255); break;
            default: bg_color = IM_COL32(80, 80, 80, 255); break;
          }

          bool is_current = (*current_room_id_ == room_id);
          bool is_open = false;
          for (int i = 0; i < active_rooms_->Size; i++) {
            if ((*active_rooms_)[i] == room_id) {
              is_open = true;
              break;
            }
          }

          // Draw cell background
          draw_list->AddRectFilled(cell_min, cell_max, bg_color);

          // Draw outline based on state
          if (is_current) {
            draw_list->AddRect(cell_min, cell_max, IM_COL32(144, 238, 144, 255),
                               0.0f, 0, 2.5f);
          } else if (is_open) {
            draw_list->AddRect(cell_min, cell_max, IM_COL32(0, 255, 0, 255),
                               0.0f, 0, 2.0f);
          } else {
            draw_list->AddRect(cell_min, cell_max, IM_COL32(60, 60, 60, 255),
                               0.0f, 0, 1.0f);
          }

          // Draw room ID
          char label[8];
          snprintf(label, sizeof(label), "%02X", room_id);
          ImVec2 text_size = ImGui::CalcTextSize(label);
          ImVec2 text_pos =
              ImVec2(cell_min.x + (kRoomCellSize - text_size.x) * 0.5f,
                     cell_min.y + (kRoomCellSize - text_size.y) * 0.5f);
          draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), label);

          // Handle clicks
          ImGui::SetCursorScreenPos(cell_min);
          char btn_id[32];
          snprintf(btn_id, sizeof(btn_id), "##room%d", room_id);
          ImGui::InvisibleButton(btn_id, ImVec2(kRoomCellSize, kRoomCellSize));

          if (ImGui::IsItemClicked() && on_room_selected_) {
            on_room_selected_(room_id);
          }

          // Tooltip
          if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            if (room_id < static_cast<int>(std::size(zelda3::kRoomNames))) {
              ImGui::Text("%s", zelda3::kRoomNames[room_id].data());
            } else {
              ImGui::Text("Room %03X", room_id);
            }
            ImGui::Text("Click to %s", is_open ? "focus" : "open");
            ImGui::EndTooltip();
          }
        } else {
          // Empty cell
          draw_list->AddRectFilled(cell_min, cell_max, IM_COL32(40, 40, 40, 255));
        }

        room_index++;
      }
    }

    // Advance cursor past the grid
    ImGui::Dummy(ImVec2(kRoomsPerRow * (kRoomCellSize + kCellSpacing),
                        kRoomsPerCol * (kRoomCellSize + kCellSpacing)));
  }

 private:
  int* current_room_id_ = nullptr;
  ImVector<int>* active_rooms_ = nullptr;
  std::function<void(int)> on_room_selected_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_MATRIX_PANEL_H_
