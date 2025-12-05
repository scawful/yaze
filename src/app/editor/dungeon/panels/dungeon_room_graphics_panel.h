#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_GRAPHICS_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_GRAPHICS_PANEL_H_

#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonRoomGraphicsPanel
 * @brief EditorPanel for displaying room graphics blocks
 *
 * This panel shows the graphics blocks used by the current room,
 * displaying a 2x8 grid of 128x32 graphics blocks.
 *
 * @see EditorPanel - Base interface
 */
class DungeonRoomGraphicsPanel : public EditorPanel {
 public:
  DungeonRoomGraphicsPanel(int* current_room_id,
                           std::array<zelda3::Room, 0x128>* rooms,
                           gfx::IRenderer* renderer = nullptr)
      : current_room_id_(current_room_id),
        rooms_(rooms),
        renderer_(renderer),
        room_gfx_canvas_("##RoomGfxCanvasPanel", ImVec2(256 + 1, 256 + 1)) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.room_graphics"; }
  std::string GetDisplayName() const override { return "Room Graphics"; }
  std::string GetIcon() const override { return ICON_MD_IMAGE; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 50; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!current_room_id_ || !rooms_) {
      ImGui::TextDisabled("No room data available");
      return;
    }

    if (*current_room_id_ < 0 ||
        *current_room_id_ >= static_cast<int>(rooms_->size())) {
      ImGui::TextDisabled("No room selected");
      return;
    }

    auto& room = (*rooms_)[*current_room_id_];

    ImGui::Text("Room %03X Graphics", *current_room_id_);
    ImGui::Text("Blockset: %02X", room.blockset);
    ImGui::Separator();

    room_gfx_canvas_.DrawBackground();
    room_gfx_canvas_.DrawContextMenu();
    room_gfx_canvas_.DrawTileSelector(32);

    auto blocks = room.blocks();

    // Load graphics if not already loaded
    if (blocks.empty()) {
      room.LoadRoomGraphics(room.blockset);
      blocks = room.blocks();
    }

    int current_block = 0;
    constexpr int max_blocks_per_row = 2;
    constexpr int block_width = 128;
    constexpr int block_height = 32;

    for (int block : blocks) {
      if (current_block >= 16) break;

      if (block < static_cast<int>(gfx::Arena::Get().gfx_sheets().size())) {
        auto& gfx_sheet = gfx::Arena::Get().gfx_sheets()[block];

        // Create texture if needed
        if (!gfx_sheet.texture() && gfx_sheet.is_active() &&
            gfx_sheet.width() > 0) {
          gfx::Arena::Get().QueueTextureCommand(
              gfx::Arena::TextureCommandType::CREATE, &gfx_sheet);
          gfx::Arena::Get().ProcessTextureQueue(renderer_);
        }

        int row = current_block / max_blocks_per_row;
        int col = current_block % max_blocks_per_row;

        int x = room_gfx_canvas_.zero_point().x + 2 + (col * block_width);
        int y = room_gfx_canvas_.zero_point().y + 2 + (row * block_height);

        if (gfx_sheet.texture() != 0) {
          room_gfx_canvas_.draw_list()->AddImage(
              (ImTextureID)(intptr_t)gfx_sheet.texture(), ImVec2(x, y),
              ImVec2(x + block_width, y + block_height));
        } else {
          room_gfx_canvas_.draw_list()->AddRectFilled(
              ImVec2(x, y), ImVec2(x + block_width, y + block_height),
              IM_COL32(40, 40, 40, 255));
          room_gfx_canvas_.draw_list()->AddText(
              ImVec2(x + 10, y + 10), IM_COL32(255, 255, 255, 255),
              "No Graphics");
        }
      }
      current_block++;
    }

    room_gfx_canvas_.DrawGrid(32.0f);
    room_gfx_canvas_.DrawOverlay();
  }

 private:
  int* current_room_id_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  gfx::IRenderer* renderer_ = nullptr;
  gui::Canvas room_gfx_canvas_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_GRAPHICS_PANEL_H_
