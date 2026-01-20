#include "app/editor/dungeon/panels/dungeon_room_graphics_panel.h"

#include "app/editor/core/content_registry.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void DungeonRoomGraphicsPanel::Draw(bool* p_open) {
  if (!ImGui::Begin(GetDisplayName().c_str(), p_open)) {
    ImGui::End();
    return;
  }

  // Get context from ContentRegistry if not set via legacy constructor
  if (current_room_id_ == nullptr || rooms_ == nullptr) {
    auto* editor = ContentRegistry::Context::current_editor();
    if (editor != nullptr) {
      if (auto* dungeon_editor = dynamic_cast<DungeonEditorV2*>(editor)) {
        current_room_id_ = dungeon_editor->mutable_current_room_id();
        rooms_ = &dungeon_editor->rooms();
        renderer_ = dungeon_editor->renderer();
      }
    }
  }

  // Validate we have required data
  if (current_room_id_ == nullptr || rooms_ == nullptr) {
    ImGui::TextDisabled("No room data available");
    ImGui::End();
    return;
  }

  const int active_room_id = *current_room_id_;
  if (active_room_id < 0 || active_room_id >= static_cast<int>(rooms_->size())) {
    ImGui::TextDisabled("Invalid room ID: %d", active_room_id);
    ImGui::End();
    return;
  }

  auto& room = (*rooms_)[active_room_id];
  auto blocks = room.blocks();

  ImGui::Text("Room %d Graphics Blocks", active_room_id);
  ImGui::Separator();

  // Canvas frame for graphics display
  const int block_width = 128;
  const int block_height = 32;
  const int max_blocks_per_row = 2;

  gui::CanvasFrameOptions frame_opts;
  frame_opts.draw_grid = true;
  frame_opts.grid_step = 32.0f;
  frame_opts.render_popups = true;

  gui::CanvasFrame frame(room_gfx_canvas_, frame_opts);
  room_gfx_canvas_.DrawTileSelector(32);

  int current_block = 0;
  for (int block_id : blocks) {
    if (current_block >= 16) break;  // Only show first 16 blocks

    // Get the graphics sheet from Arena using the block ID
    if (block_id < static_cast<int>(gfx::Arena::Get().gfx_sheets().size())) {
      auto& gfx_sheet = gfx::Arena::Get().gfx_sheets()[block_id];

      int row = current_block / max_blocks_per_row;
      int col = current_block % max_blocks_per_row;

      ImVec2 local_pos(2 + (col * block_width), 2 + (row * block_height));

      // Ensure we don't exceed canvas bounds
      if (local_pos.x + block_width <= room_gfx_canvas_.width() &&
          local_pos.y + block_height <= room_gfx_canvas_.height()) {
        if (gfx_sheet.texture() != 0) {
          room_gfx_canvas_.AddImageAt(
              (ImTextureID)(intptr_t)gfx_sheet.texture(), local_pos,
              ImVec2(block_width, block_height));
        }
      }
    }
    ++current_block;
  }

  ImGui::End();
}

}  // namespace editor
}  // namespace yaze
