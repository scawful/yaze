#include "app/editor/dungeon/workspace/room_graphics_content.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/panels/dungeon_panel_access.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/theme_manager.h"
#include "app/platform/sdl_compat.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void RoomGraphicsContent::RefreshSheetPreviews(const zelda3::Room& room) {
  const auto blocks = room.blocks();
  if (blocks.size() < sheet_previews_.size()) {
    preview_cache_valid_ = false;
    return;
  }

  bool needs_refresh =
      palette_dirty_ || !preview_cache_valid_ || preview_room_id_ != room.id();
  if (!needs_refresh) {
    for (size_t i = 0; i < preview_block_ids_.size(); ++i) {
      if (preview_block_ids_[i] != blocks[i]) {
        needs_refresh = true;
        break;
      }
    }
  }
  if (!needs_refresh) {
    return;
  }

  std::vector<SDL_Color> palette_colors;
  if (SDL_Surface* bg_surface = room.bg1_buffer().bitmap().surface()) {
    if (SDL_Palette* palette = platform::GetSurfacePalette(bg_surface)) {
      palette_colors.assign(palette->colors,
                            palette->colors + palette->ncolors);
    }
  }

  const auto& gfx_buffer = room.get_gfx_buffer();
  constexpr size_t kSheetBytes = 128 * 32;
  for (size_t i = 0; i < sheet_previews_.size(); ++i) {
    preview_block_ids_[i] = blocks[i];
    const size_t offset = i * kSheetBytes;
    auto& meta = sheet_preview_metadata_[i];
    meta = {};
    meta.block_id = blocks[i];
    meta.source_offset = offset;
    if (offset + kSheetBytes > gfx_buffer.size()) {
      sheet_previews_[i] = gfx::Bitmap();
      continue;
    }

    std::vector<uint8_t> sheet_pixels(
        gfx_buffer.begin() + offset, gfx_buffer.begin() + offset + kSheetBytes);
    meta.nonzero_pixels = static_cast<int>(
        std::count_if(sheet_pixels.begin(), sheet_pixels.end(),
                      [](uint8_t value) { return value != 0; }));
    sheet_previews_[i].Create(128, 32, 8,
                              static_cast<int>(gfx::BitmapFormat::kIndexed),
                              sheet_pixels);
    if (!palette_colors.empty()) {
      sheet_previews_[i].SetPalette(palette_colors);
    }
    sheet_previews_[i].CreateTexture();
    meta.width = sheet_previews_[i].width();
    meta.height = sheet_previews_[i].height();
    meta.bitmap_active = sheet_previews_[i].is_active();
    meta.has_surface = sheet_previews_[i].surface() != nullptr;
    meta.palette_colors = static_cast<int>(palette_colors.size());
    meta.has_texture = sheet_previews_[i].texture() != nullptr;
  }

  preview_room_id_ = room.id();
  preview_cache_valid_ = true;
  palette_dirty_ = false;
}

void RoomGraphicsContent::Draw(bool* p_open) {
  (void)p_open;
  // Resolve the active dungeon editor lazily when this panel is hosted through
  // the workspace window system instead of the legacy direct constructor.
  if (current_room_id_ == nullptr || rooms_ == nullptr) {
    const auto ctx = CurrentDungeonWindowContext();
    if (ctx) {
      current_room_id_ = ctx.editor->mutable_current_room_id();
      rooms_ = &ctx.editor->rooms();
      renderer_ = ctx.editor->renderer();
    }
  }

  if (current_room_id_ == nullptr || rooms_ == nullptr) {
    ImGui::TextDisabled("No room data available");
    return;
  }

  const int active_room_id = *current_room_id_;
  if (active_room_id < 0 ||
      active_room_id >= static_cast<int>(rooms_->size())) {
    ImGui::TextDisabled("Invalid room ID: %d", active_room_id);
    return;
  }

  auto& room = (*rooms_)[active_room_id];
  bool needs_render = false;
  if (room.blocks().empty()) {
    room.LoadRoomGraphics(room.blockset());
    needs_render = true;
  }
  if (!room.AreObjectsLoaded()) {
    room.LoadObjects();
    needs_render = true;
  }
  auto& bg1_bitmap = room.bg1_buffer().bitmap();
  if (needs_render || !bg1_bitmap.is_active() || bg1_bitmap.width() == 0) {
    room.RenderRoomGraphics();
  }
  // Keep room-sheet assignments in sync with the active room header.
  room.LoadRoomGraphics(room.blockset());
  RefreshSheetPreviews(room);

  if (renderer_ != nullptr) {
    gfx::Arena::Get().ProcessTextureQueue(renderer_);
  }
  auto blocks = room.blocks();

  constexpr float kBlockWidth = 128.0f;
  constexpr float kBlockHeight = 32.0f;
  constexpr int kBlocksPerRow = 2;
  constexpr float kPadding = 4.0f;

  const int block_count = static_cast<int>(blocks.size());
  const int row_count =
      std::max(1, (block_count + kBlocksPerRow - 1) / kBlocksPerRow);
  const ImVec2 canvas_size(
      std::max(ImGui::GetContentRegionAvail().x,
               kPadding + (kBlockWidth + kPadding) *
                              static_cast<float>(kBlocksPerRow)),
      kPadding + (kBlockHeight + kPadding) * static_cast<float>(row_count));

  ImGui::Text("Room %03X Graphics Blocks", active_room_id);
  ImGui::TextDisabled("Blockset %02X | Spriteset %02X", room.blockset(),
                      room.spriteset());
  ImGui::SameLine();
  ImGui::Checkbox("Source Trace", &show_source_trace_);
  ImGui::Separator();

  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = canvas_size;
  frame_opts.draw_grid = false;
  frame_opts.draw_overlay = false;
  frame_opts.draw_context_menu = false;
  frame_opts.render_popups = false;

  auto rt = gui::BeginCanvas(room_gfx_canvas_, frame_opts);

  const float grid_width =
      kPadding + (kBlockWidth + kPadding) * static_cast<float>(kBlocksPerRow);
  const float x_offset = std::max(0.0f, (canvas_size.x - grid_width) * 0.5f);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  for (int i = 0; i < block_count; ++i) {
    const uint8_t block_id = blocks[static_cast<size_t>(i)];
    const int row = i / kBlocksPerRow;
    const int col = i % kBlocksPerRow;
    const ImVec2 local_pos(x_offset + kPadding + col * (kBlockWidth + kPadding),
                           kPadding + row * (kBlockHeight + kPadding));

    if (i < static_cast<int>(sheet_previews_.size()) &&
        sheet_previews_[static_cast<size_t>(i)].texture() != 0) {
      gui::BitmapPreviewOptions preview_opts;
      preview_opts.dest_pos = local_pos;
      preview_opts.dest_size = ImVec2(kBlockWidth, kBlockHeight);
      preview_opts.draw_context_menu = false;
      preview_opts.draw_grid = false;
      preview_opts.draw_overlay = false;
      preview_opts.render_popups = false;
      preview_opts.ensure_texture = true;
      gui::DrawBitmapPreview(rt, sheet_previews_[static_cast<size_t>(i)],
                             preview_opts);
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
                         block_id == 0 ? "Empty" : "Missing");
    }
  }

  gui::EndCanvas(room_gfx_canvas_, rt, frame_opts);

  if (show_source_trace_ &&
      ImGui::CollapsingHeader(ICON_MD_BUG_REPORT " Source Trace",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled(
        "Source: Room::get_gfx_buffer() -> preview bitmap -> queued texture");
    ImGui::TextDisabled("Room buffer bytes: %zu", room.get_gfx_buffer().size());
    ImGui::Separator();
    for (int i = 0;
         i < std::min(block_count,
                      static_cast<int>(sheet_preview_metadata_.size()));
         ++i) {
      const auto& meta = sheet_preview_metadata_[static_cast<size_t>(i)];
      const uintptr_t texture_value = reinterpret_cast<uintptr_t>(
          sheet_previews_[static_cast<size_t>(i)].texture());
      ImGui::PushID(i);
      ImGui::Text(
          "Slot %02d | Block %02X | Buf +0x%04zX | %dx%d | nz=%d | pal=%d | "
          "active=%d surf=%d tex=%d (0x%zx)",
          i, meta.block_id, meta.source_offset, meta.width, meta.height,
          meta.nonzero_pixels, meta.palette_colors, meta.bitmap_active,
          meta.has_surface, meta.has_texture, texture_value);
      ImGui::PopID();
    }
  }
}

}  // namespace editor
}  // namespace yaze
