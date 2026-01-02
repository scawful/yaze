#include <algorithm>
#include <cmath>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

using namespace ImGui;

// =============================================================================
// Scratch Space - Unified Single Workspace
// =============================================================================

absl::Status OverworldEditor::DrawScratchSpace() {
  // Header with clear button
  Text(ICON_MD_BRUSH " Scratch Workspace");
  SameLine();
  if (Button("Clear")) {
    RETURN_IF_ERROR(ClearScratchSpace());
  }
  HOVER_HINT("Clear scratch workspace");

  // Status info
  Text("%s (%dx%d)", scratch_space_.name.c_str(), scratch_space_.width,
       scratch_space_.height);

  // Interaction hints
  if (ow_map_canvas_.select_rect_active() &&
      !ow_map_canvas_.selected_tiles().empty()) {
    TextColored(
        ImVec4(0.4f, 1.0f, 0.4f, 1.0f), ICON_MD_CONTENT_PASTE
        " Overworld selection active! Click in scratch space to stamp.");
  } else {
    Text("Left-click to paint with current tile.");
    Text("Right-click to select tiles.");
  }

  // Initialize scratch bitmap if needed
  if (!scratch_space_.scratch_bitmap.is_active()) {
    int bitmap_width = scratch_space_.width * 16;
    int bitmap_height = scratch_space_.height * 16;
    std::vector<uint8_t> empty_data(bitmap_width * bitmap_height, 0);
    scratch_space_.scratch_bitmap.Create(bitmap_width, bitmap_height, 8,
                                         empty_data);
    if (all_gfx_loaded_) {
      palette_ = overworld_.current_area_palette();
      scratch_space_.scratch_bitmap.SetPalette(palette_);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE,
          &scratch_space_.scratch_bitmap);
    }
  }

  // Draw the scratch space canvas using modern BeginCanvas/EndCanvas pattern
  gui::BeginPadding(3);
  ImGui::BeginGroup();

  ImVec2 scratch_content_size(scratch_space_.width * 16 + 4,
                              scratch_space_.height * 16 + 4);

  // Configure canvas frame options
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = scratch_content_size;
  frame_opts.draw_grid = true;
  frame_opts.grid_step = 32.0f;          // Tile16 grid (32px = 2x tile scale)
  frame_opts.draw_context_menu = false;  // No context menu for scratch
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = false;
  frame_opts.use_child_window = false;

  gui::BeginChildWithScrollbar("##ScratchSpaceScrollRegion",
                               scratch_content_size);

  auto canvas_rt = gui::BeginCanvas(scratch_canvas_, frame_opts);
  gui::EndPadding();

  if (scratch_space_.scratch_bitmap.is_active()) {
    scratch_canvas_.DrawBitmap(scratch_space_.scratch_bitmap, 2, 2, 1.0f);
  }

  if (map_blockset_loaded_) {
    scratch_canvas_.DrawTileSelector(32.0f);
  }

  // Handle Interactions using runtime hover state
  if (canvas_rt.hovered) {
    if (ow_map_canvas_.select_rect_active() &&
        !ow_map_canvas_.selected_tiles().empty()) {
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        RETURN_IF_ERROR(SaveCurrentSelectionToScratch());
      }
    } else if (current_mode == EditingMode::DRAW_TILE ||
               ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      DrawScratchSpaceEdits();
    }
  }

  gui::EndCanvas(scratch_canvas_, canvas_rt, frame_opts);
  EndChild();
  ImGui::EndGroup();

  return absl::OkStatus();
}

void OverworldEditor::DrawScratchSpaceEdits() {
  auto mouse_position = scratch_canvas_.drawn_tile_position();
  int grid_size = 32;

  int tile_x = static_cast<int>(mouse_position.x) / grid_size;
  int tile_y = static_cast<int>(mouse_position.y) / grid_size;

  int max_width = scratch_space_.width > 0 ? scratch_space_.width : 20;
  int max_height = scratch_space_.height > 0 ? scratch_space_.height : 30;

  if (tile_x >= 0 && tile_x < max_width && tile_y >= 0 && tile_y < max_height) {
    if (tile_x < 32 && tile_y < 32) {
      scratch_space_.tile_data[tile_x][tile_y] = current_tile16_;
    }

    UpdateScratchBitmapTile(tile_x, tile_y, current_tile16_);

    if (!scratch_space_.in_use) {
      scratch_space_.in_use = true;
      scratch_space_.name = "Modified Layout";
    }
  }
}

void OverworldEditor::DrawScratchSpacePattern() {
  auto mouse_position = scratch_canvas_.drawn_tile_position();

  int start_tile_x = static_cast<int>(mouse_position.x) / 32;
  int start_tile_y = static_cast<int>(mouse_position.y) / 32;

  if (!dependencies_.shared_clipboard ||
      !dependencies_.shared_clipboard->has_overworld_tile16) {
    return;
  }

  const auto& tile_ids = dependencies_.shared_clipboard->overworld_tile16_ids;
  int pattern_width = dependencies_.shared_clipboard->overworld_width;
  int pattern_height = dependencies_.shared_clipboard->overworld_height;

  if (tile_ids.empty())
    return;

  int max_width = scratch_space_.width > 0 ? scratch_space_.width : 20;
  int max_height = scratch_space_.height > 0 ? scratch_space_.height : 30;

  int idx = 0;
  for (int py = 0; py < pattern_height && (start_tile_y + py) < max_height;
       ++py) {
    for (int px = 0; px < pattern_width && (start_tile_x + px) < max_width;
         ++px) {
      if (idx < static_cast<int>(tile_ids.size())) {
        int tile_id = tile_ids[idx];
        int scratch_x = start_tile_x + px;
        int scratch_y = start_tile_y + py;

        if (scratch_x >= 0 && scratch_x < 32 && scratch_y >= 0 &&
            scratch_y < 32) {
          scratch_space_.tile_data[scratch_x][scratch_y] = tile_id;
          UpdateScratchBitmapTile(scratch_x, scratch_y, tile_id);
        }
        idx++;
      }
    }
  }

  scratch_space_.in_use = true;
  if (scratch_space_.name == "Scratch Space") {
    scratch_space_.name =
        absl::StrFormat("Pattern %dx%d", pattern_width, pattern_height);
  }
}

void OverworldEditor::UpdateScratchBitmapTile(int tile_x, int tile_y,
                                              int tile_id) {
  gfx::ScopedTimer timer("overworld_update_scratch_tile");

  auto tile_data = gfx::GetTilemapData(tile16_blockset_, tile_id);
  if (tile_data.empty())
    return;

  const int grid_size = 32;
  int scratch_bitmap_width = scratch_space_.scratch_bitmap.width();
  int scratch_bitmap_height = scratch_space_.scratch_bitmap.height();

  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      int src_index = y * 16 + x;

      int dst_x = tile_x * grid_size + x + x;
      int dst_y = tile_y * grid_size + y + y;

      if (dst_x >= 0 && dst_x < scratch_bitmap_width && dst_y >= 0 &&
          dst_y < scratch_bitmap_height &&
          src_index < static_cast<int>(tile_data.size())) {
        for (int py = 0; py < 2 && (dst_y + py) < scratch_bitmap_height; ++py) {
          for (int px = 0; px < 2 && (dst_x + px) < scratch_bitmap_width;
               ++px) {
            int dst_index = (dst_y + py) * scratch_bitmap_width + (dst_x + px);
            scratch_space_.scratch_bitmap.WriteToPixel(dst_index,
                                                       tile_data[src_index]);
          }
        }
      }
    }
  }

  scratch_space_.scratch_bitmap.set_modified(true);
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &scratch_space_.scratch_bitmap);
  scratch_space_.in_use = true;
}

absl::Status OverworldEditor::SaveCurrentSelectionToScratch() {
  gfx::ScopedTimer timer("overworld_save_selection_to_scratch");

  if (ow_map_canvas_.select_rect_active() &&
      !ow_map_canvas_.selected_tiles().empty()) {
    const auto& selected_points = ow_map_canvas_.selected_points();
    if (selected_points.size() >= 2) {
      // selected_points are now stored in world coordinates
      const auto start = selected_points[0];
      const auto end = selected_points[1];

      int selection_width =
          std::abs(static_cast<int>((end.x - start.x) / 16)) + 1;
      int selection_height =
          std::abs(static_cast<int>((end.y - start.y) / 16)) + 1;

      scratch_space_.width = std::max(1, std::min(selection_width, 32));
      scratch_space_.height = std::max(1, std::min(selection_height, 32));
      scratch_space_.in_use = true;
      scratch_space_.name = absl::StrFormat(
          "Selection %dx%d", scratch_space_.width, scratch_space_.height);

      int bitmap_width = scratch_space_.width * 16;
      int bitmap_height = scratch_space_.height * 16;
      std::vector<uint8_t> empty_data(bitmap_width * bitmap_height, 0);
      scratch_space_.scratch_bitmap.Create(bitmap_width, bitmap_height, 8,
                                           empty_data);
      if (all_gfx_loaded_) {
        palette_ = overworld_.current_area_palette();
        scratch_space_.scratch_bitmap.SetPalette(palette_);
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE,
            &scratch_space_.scratch_bitmap);
      }

      overworld_.set_current_world(current_world_);
      overworld_.set_current_map(current_map_);

      int idx = 0;
      for (int y = 0;
           y < scratch_space_.height &&
           idx < static_cast<int>(ow_map_canvas_.selected_tiles().size());
           ++y) {
        for (int x = 0;
             x < scratch_space_.width &&
             idx < static_cast<int>(ow_map_canvas_.selected_tiles().size());
             ++x) {
          if (idx < static_cast<int>(ow_map_canvas_.selected_tiles().size())) {
            int tile_id = overworld_.GetTileFromPosition(
                ow_map_canvas_.selected_tiles()[idx]);
            if (x < 32 && y < 32) {
              scratch_space_.tile_data[x][y] = tile_id;
            }
            UpdateScratchBitmapTile(x, y, tile_id);
            idx++;
          }
        }
      }
    }
  } else {
    scratch_space_.width = 16;
    scratch_space_.height = 16;
    scratch_space_.name = absl::StrFormat("Map %d Area", current_map_);
    scratch_space_.in_use = true;
  }

  return absl::OkStatus();
}

absl::Status OverworldEditor::LoadScratchToSelection() {
  if (!scratch_space_.in_use) {
    return absl::FailedPreconditionError("Scratch space is empty");
  }

  util::logf("Loading scratch space: %s", scratch_space_.name.c_str());

  return absl::OkStatus();
}

absl::Status OverworldEditor::ClearScratchSpace() {
  scratch_space_.in_use = false;
  scratch_space_.name = "Scratch Space";

  // Clear tile data
  for (auto& row : scratch_space_.tile_data) {
    row.fill(0);
  }

  // Clear the bitmap
  if (scratch_space_.scratch_bitmap.is_active()) {
    auto& data = scratch_space_.scratch_bitmap.mutable_data();
    std::fill(data.begin(), data.end(), 0);
    scratch_space_.scratch_bitmap.set_modified(true);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &scratch_space_.scratch_bitmap);
  }

  return absl::OkStatus();
}

}  // namespace yaze::editor
