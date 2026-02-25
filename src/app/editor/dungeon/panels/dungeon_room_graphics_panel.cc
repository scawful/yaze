#include "app/editor/dungeon/panels/dungeon_room_graphics_panel.h"

#include <algorithm>

#include "app/editor/core/content_registry.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void DungeonRoomGraphicsPanel::Draw(bool* p_open) {
  if (!ImGui::Begin(GetDisplayName().c_str(), p_open)) {
    ImGui::End();
    return;
  }

  // Get context from ContentRegistry if not set via legacy constructor.
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

  if (current_room_id_ == nullptr || rooms_ == nullptr) {
    ImGui::TextDisabled("No room data available");
    ImGui::End();
    return;
  }

  const int active_room_id = *current_room_id_;
  if (active_room_id < 0 ||
      active_room_id >= static_cast<int>(rooms_->size())) {
    ImGui::TextDisabled("Invalid room ID: %d", active_room_id);
    ImGui::End();
    return;
  }

  auto& room = (*rooms_)[active_room_id];
  // Refresh room sheet assignments from current room header values.
  room.LoadRoomGraphics(room.blockset());
  auto blocks = room.blocks();

  if (renderer_ != nullptr) {
    gfx::Arena::Get().ProcessTextureQueue(renderer_);
  }

  constexpr float kBlockWidth = 128.0f;
  constexpr float kBlockHeight = 32.0f;
  constexpr int kBlocksPerRow = 2;
  constexpr float kPadding = 4.0f;

  const int block_count = static_cast<int>(blocks.size());
  const int row_count =
      std::max(1, (block_count + kBlocksPerRow - 1) / kBlocksPerRow);
  const ImVec2 canvas_size(
      kPadding + (kBlockWidth + kPadding) * static_cast<float>(kBlocksPerRow),
      kPadding + (kBlockHeight + kPadding) * static_cast<float>(row_count));

  ImGui::Text("Room %03X Graphics Blocks", active_room_id);
  ImGui::TextDisabled("Blockset %02X | Spriteset %02X", room.blockset(),
                      room.spriteset());
  ImGui::Separator();

  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = canvas_size;
  frame_opts.draw_grid = false;
  frame_opts.draw_overlay = false;
  frame_opts.draw_context_menu = false;
  frame_opts.render_popups = false;

  auto runtime = gui::BeginCanvas(room_gfx_canvas_, frame_opts);
  (void)runtime;

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  for (int i = 0; i < block_count; ++i) {
    const uint8_t block_id = blocks[static_cast<size_t>(i)];
    const int row = i / kBlocksPerRow;
    const int col = i % kBlocksPerRow;
    const ImVec2 local_pos(kPadding + col * (kBlockWidth + kPadding),
                           kPadding + row * (kBlockHeight + kPadding));

    const auto& sheets = gfx::Arena::Get().gfx_sheets();
    if (block_id < sheets.size() && sheets[block_id].texture() != 0) {
      room_gfx_canvas_.AddImageAt(
          (ImTextureID)(intptr_t)sheets[block_id].texture(), local_pos,
          ImVec2(kBlockWidth, kBlockHeight));
    } else {
      const ImVec2 zero = room_gfx_canvas_.zero_point();
      const float scale = room_gfx_canvas_.global_scale();
      const ImVec2 screen_pos(zero.x + local_pos.x, zero.y + local_pos.y);
      const ImVec2 screen_end(screen_pos.x + kBlockWidth * scale,
                              screen_pos.y + kBlockHeight * scale);
      draw_list->AddRect(screen_pos, screen_end,
                         ImGui::GetColorU32(gui::GetOutlineVec4()));
      draw_list->AddText(ImVec2(screen_pos.x + 6.0f, screen_pos.y + 6.0f),
                         ImGui::GetColorU32(gui::GetTextSecondaryVec4()),
                         "Missing");
    }
  }

  gui::EndCanvas(room_gfx_canvas_, runtime, frame_opts);

  ImGui::End();
}

}  // namespace editor
}  // namespace yaze
