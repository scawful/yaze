#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_MATRIX_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_MATRIX_PANEL_H_

#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonRoomMatrixPanel
 * @brief EditorPanel for displaying a visual 16x19 grid of all dungeon rooms
 *
 * This panel provides a compact overview of all 296 dungeon rooms in a matrix
 * layout. Users can click on cells to select and open rooms.
 *
 * Features:
 * - Responsive cell sizing based on panel width
 * - Palette-based coloring when room data is available
 * - Theme-aware selection highlighting
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
   * @param rooms Optional pointer to room array for palette-based coloring
   */
  DungeonRoomMatrixPanel(int* current_room_id, ImVector<int>* active_rooms,
                         std::function<void(int)> on_room_selected,
                         std::function<void(int, int)> on_room_swap = nullptr,
                         std::array<zelda3::Room, 0x128>* rooms = nullptr)
      : current_room_id_(current_room_id),
        active_rooms_(active_rooms),
        rooms_(rooms),
        on_room_selected_(std::move(on_room_selected)),
        on_room_swap_(std::move(on_room_swap)) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.room_matrix"; }
  std::string GetDisplayName() const override { return "Room Matrix"; }
  std::string GetIcon() const override { return ICON_MD_GRID_VIEW; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 30; }

  void SetRoomIntentCallback(
      std::function<void(int, RoomSelectionIntent)> callback) {
    on_room_intent_ = std::move(callback);
  }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!current_room_id_ || !active_rooms_) return;

    const auto& theme = AgentUI::GetTheme();

    // 16 wide x 19 tall = 304 cells (296 rooms + 8 empty)
    constexpr int kRoomsPerRow = 16;
    constexpr int kRoomsPerCol = 19;
    constexpr int kTotalRooms = 0x128;  // 296 rooms (0x00-0x127)
    constexpr float kCellSpacing = 1.0f;

    // Responsive cell size based on available panel width
    float panel_width = ImGui::GetContentRegionAvail().x;
    // Calculate cell size to fit 16 cells with spacing in available width
    float cell_size = std::max(12.0f, std::min(24.0f,
        (panel_width - kCellSpacing * (kRoomsPerRow - 1)) / kRoomsPerRow));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

    int room_index = 0;
    for (int row = 0; row < kRoomsPerCol; row++) {
      for (int col = 0; col < kRoomsPerRow; col++) {
        int room_id = room_index;
        bool is_valid_room = (room_id < kTotalRooms);

        ImVec2 cell_min =
            ImVec2(canvas_pos.x + col * (cell_size + kCellSpacing),
                   canvas_pos.y + row * (cell_size + kCellSpacing));
        ImVec2 cell_max =
            ImVec2(cell_min.x + cell_size, cell_min.y + cell_size);

        if (is_valid_room) {
          // Get color based on room palette if available, else use algorithmic
          ImU32 bg_color = GetRoomColor(room_id, theme);

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

          // Draw outline based on state using theme colors
          if (is_current) {
            // Add glow effect for current room (outer glow layers)
            ImVec4 glow_color = theme.dungeon_selection_primary;
            glow_color.w = 0.3f;  // 30% opacity outer glow
            ImVec2 glow_min(cell_min.x - 2, cell_min.y - 2);
            ImVec2 glow_max(cell_max.x + 2, cell_max.y + 2);
            draw_list->AddRect(glow_min, glow_max, 
                              ImGui::ColorConvertFloat4ToU32(glow_color), 
                              0.0f, 0, 3.0f);
            
            // Inner bright border
            ImU32 sel_color = ImGui::ColorConvertFloat4ToU32(
                theme.dungeon_selection_primary);
            draw_list->AddRect(cell_min, cell_max, sel_color, 0.0f, 0, 2.5f);
          } else if (is_open) {
            ImU32 open_color = ImGui::ColorConvertFloat4ToU32(
                theme.dungeon_grid_cell_selected);
            draw_list->AddRect(cell_min, cell_max, open_color, 0.0f, 0, 2.0f);
          } else {
            ImU32 border_color = ImGui::ColorConvertFloat4ToU32(
                theme.dungeon_grid_cell_border);
            draw_list->AddRect(cell_min, cell_max, border_color, 0.0f, 0, 1.0f);
          }

          // Draw room ID (only if cell is large enough)
          if (cell_size >= 18.0f) {
            char label[8];
            snprintf(label, sizeof(label), "%02X", room_id);
            ImVec2 text_size = ImGui::CalcTextSize(label);
            ImVec2 text_pos =
                ImVec2(cell_min.x + (cell_size - text_size.x) * 0.5f,
                       cell_min.y + (cell_size - text_size.y) * 0.5f);
            ImU32 text_color = ImGui::ColorConvertFloat4ToU32(
                theme.dungeon_grid_text);
            draw_list->AddText(text_pos, text_color, label);
          }

          // Handle clicks
          ImGui::SetCursorScreenPos(cell_min);
          char btn_id[32];
          snprintf(btn_id, sizeof(btn_id), "##room%d", room_id);
          ImGui::InvisibleButton(btn_id, ImVec2(cell_size, cell_size));

          if (ImGui::IsItemClicked()) {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
              // Double-click: open as standalone panel
              if (on_room_intent_) {
                on_room_intent_(room_id,
                                RoomSelectionIntent::kOpenStandalone);
              } else if (on_room_selected_) {
                on_room_selected_(room_id);
              }
            } else if (on_room_selected_) {
              on_room_selected_(room_id);
            }
          }

          if (ImGui::BeginPopupContextItem()) {
            const bool can_swap =
                on_room_swap_ && current_room_id_ &&
                *current_room_id_ >= 0 && *current_room_id_ < kTotalRooms &&
                *current_room_id_ != room_id;

            std::string open_label = is_open ? "Focus Room" : "Open in Workbench";
            if (ImGui::MenuItem(open_label.c_str())) {
              if (on_room_intent_) {
                on_room_intent_(room_id,
                                RoomSelectionIntent::kFocusInWorkbench);
              } else if (on_room_selected_) {
                on_room_selected_(room_id);
              }
            }

            if (ImGui::MenuItem("Open as Panel")) {
              if (on_room_intent_) {
                on_room_intent_(room_id,
                                RoomSelectionIntent::kOpenStandalone);
              } else if (on_room_selected_) {
                on_room_selected_(room_id);
              }
            }

            if (ImGui::MenuItem("Swap With Current Room", nullptr, false,
                                can_swap)) {
              on_room_swap_(*current_room_id_, room_id);
            }

            ImGui::Separator();

            char id_buf[16];
            snprintf(id_buf, sizeof(id_buf), "0x%02X", room_id);
            if (ImGui::MenuItem("Copy Room ID")) {
              ImGui::SetClipboardText(id_buf);
            }

            const std::string& room_label = zelda3::GetRoomLabel(room_id);
            if (ImGui::MenuItem("Copy Room Name")) {
              ImGui::SetClipboardText(room_label.c_str());
            }

            ImGui::EndPopup();
          }

          // Tooltip with room info and thumbnail preview
          if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            // Use unified ResourceLabelProvider for room names
            ImGui::Text("%s", zelda3::GetRoomLabel(room_id).c_str());
            
            if (rooms_ && (*rooms_)[room_id].IsLoaded()) {
              // Show palette info
              ImGui::TextDisabled("Palette: %d | Blockset: %d", 
                                  (*rooms_)[room_id].palette,
                                  (*rooms_)[room_id].blockset);
              
              // Show thumbnail preview of the room
              auto& bg1_bitmap = (*rooms_)[room_id].bg1_buffer().bitmap();
              if (bg1_bitmap.is_active() && bg1_bitmap.texture() != 0) {
                ImGui::Separator();
                // Render at thumbnail size (80x80 from 512x512)
                constexpr float kThumbnailSize = 80.0f;
                ImGui::Image((ImTextureID)(intptr_t)bg1_bitmap.texture(),
                             ImVec2(kThumbnailSize, kThumbnailSize));
              }
            }
            
            ImGui::TextDisabled("Click to %s", is_open ? "focus" : "open");
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
    ImGui::Dummy(ImVec2(kRoomsPerRow * (cell_size + kCellSpacing),
                        kRoomsPerCol * (cell_size + kCellSpacing)));
  }

  void SetRooms(std::array<zelda3::Room, 0x128>* rooms) { rooms_ = rooms; }

 private:
  /**
   * @brief Get color for a room based on palette or algorithmic fallback
   *
   * If room data is available and loaded, generates a color based on the
   * room's palette ID for semantic grouping. Otherwise falls back to
   * algorithmic coloring.
   */
  ImU32 GetRoomColor(int room_id, const AgentUITheme& theme) {
    // If rooms data is available and this room is loaded, use palette-based color
    if (rooms_ && (*rooms_)[room_id].IsLoaded()) {
      int palette = (*rooms_)[room_id].palette;
      // Map palette to distinct hues (there are ~24 dungeon palettes)
      // Group similar palettes together for visual coherence
      float hue = (palette * 15.0f);  // Spread across 360 degrees
      float saturation = 0.4f + (palette % 3) * 0.1f;  // 40-60%
      float value = 0.5f + (palette % 5) * 0.08f;       // 50-82%

      // HSV to RGB conversion
      float h = fmodf(hue, 360.0f) / 60.0f;
      int i = static_cast<int>(h);
      float f = h - i;
      float p = value * (1 - saturation);
      float q = value * (1 - saturation * f);
      float t = value * (1 - saturation * (1 - f));

      float r, g, b;
      switch (i % 6) {
        case 0: r = value; g = t; b = p; break;
        case 1: r = q; g = value; b = p; break;
        case 2: r = p; g = value; b = t; break;
        case 3: r = p; g = q; b = value; break;
        case 4: r = t; g = p; b = value; break;
        case 5: r = value; g = p; b = q; break;
        default: r = g = b = 0.3f; break;
      }
      return IM_COL32(static_cast<int>(r * 255),
                      static_cast<int>(g * 255),
                      static_cast<int>(b * 255), 255);
    }

    // Fallback: Algorithmic coloring based on room ID
    // Group rooms by their approximate dungeon (rooms are organized in blocks)
    int dungeon_group = room_id / 0x20;  // 32 rooms per rough dungeon block
    float hue = (dungeon_group * 45.0f) + (room_id % 8) * 5.0f;
    float saturation = 0.35f + (room_id % 3) * 0.1f;
    float value = 0.45f + (room_id % 5) * 0.08f;

    float h = fmodf(hue, 360.0f) / 60.0f;
    int i = static_cast<int>(h);
    float f = h - i;
    float p = value * (1 - saturation);
    float q = value * (1 - saturation * f);
    float t = value * (1 - saturation * (1 - f));

    float r, g, b;
    switch (i % 6) {
      case 0: r = value; g = t; b = p; break;
      case 1: r = q; g = value; b = p; break;
      case 2: r = p; g = value; b = t; break;
      case 3: r = p; g = q; b = value; break;
      case 4: r = t; g = p; b = value; break;
      case 5: r = value; g = p; b = q; break;
      default: r = g = b = 0.3f; break;
    }
    return IM_COL32(static_cast<int>(r * 255),
                    static_cast<int>(g * 255),
                    static_cast<int>(b * 255), 255);
  }

  int* current_room_id_ = nullptr;
  ImVector<int>* active_rooms_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  std::function<void(int)> on_room_selected_;
  std::function<void(int, int)> on_room_swap_;
  std::function<void(int, RoomSelectionIntent)> on_room_intent_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_MATRIX_PANEL_H_
