#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_GRAPHICS_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_GRAPHICS_PANEL_H_

#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
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

  /**
   * @brief Set the current palette group for graphics rendering
   * @param group The palette group from the current room
   */
  void SetCurrentPaletteGroup(const gfx::PaletteGroup& group) {
    current_palette_group_ = group;
    palette_dirty_ = true;
  }

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

    gui::CanvasFrameOptions frame_opts;
    frame_opts.draw_grid = true;
    frame_opts.grid_step = 32.0f;
    frame_opts.render_popups = true;
    gui::CanvasFrame frame(room_gfx_canvas_, frame_opts);
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

        // Apply current room's palette to the sheet if dirty
        if (palette_dirty_ && gfx_sheet.is_active() && 
            current_palette_group_.size() > 0) {
          // Use palette index based on block type (simplified: use palette 0)
          int palette_index = 0;
          if (current_palette_group_.size() > 0) {
            gfx_sheet.SetPaletteWithTransparent(
                current_palette_group_[palette_index], palette_index);
            gfx_sheet.set_modified(true);
          }
        }

        // Create or update texture
        if (!gfx_sheet.texture() && gfx_sheet.is_active() &&
            gfx_sheet.width() > 0) {
          gfx::Arena::Get().QueueTextureCommand(
              gfx::Arena::TextureCommandType::CREATE, &gfx_sheet);
          gfx::Arena::Get().ProcessTextureQueue(renderer_);
        } else if (gfx_sheet.modified() && gfx_sheet.texture()) {
          gfx::Arena::Get().QueueTextureCommand(
              gfx::Arena::TextureCommandType::UPDATE, &gfx_sheet);
          gfx::Arena::Get().ProcessTextureQueue(renderer_);
          gfx_sheet.set_modified(false);
        }

        int row = current_block / max_blocks_per_row;
        int col = current_block % max_blocks_per_row;

        ImVec2 local_pos(2 + (col * block_width), 2 + (row * block_height));

        if (gfx_sheet.texture() != 0) {
          room_gfx_canvas_.AddImageAt(
              (ImTextureID)(intptr_t)gfx_sheet.texture(), local_pos,
              ImVec2(block_width, block_height));
        } else {
          room_gfx_canvas_.AddRectFilledAt(
              local_pos, ImVec2(block_width, block_height),
              IM_COL32(40, 40, 40, 255));
          room_gfx_canvas_.AddTextAt(ImVec2(local_pos.x + 10, local_pos.y + 10),
                                     "No Graphics",
                                     IM_COL32(255, 255, 255, 255));
        }
      }
      current_block++;
    }
    
    // Clear dirty flag after processing all blocks
    palette_dirty_ = false;
  }

 private:
  int* current_room_id_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  gfx::IRenderer* renderer_ = nullptr;
  gui::Canvas room_gfx_canvas_;
  
  // Palette tracking for proper sheet coloring
  gfx::PaletteGroup current_palette_group_;
  bool palette_dirty_ = true;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_GRAPHICS_PANEL_H_
