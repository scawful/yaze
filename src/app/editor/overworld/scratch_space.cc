#include "editor/overworld/overworld_editor.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <future>
#include <set>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/core/asar_wrapper.h"
#include "app/core/features.h"
#include "app/core/performance_monitor.h"
#include "app/core/window.h"
#include "app/editor/overworld/entity.h"
#include "app/editor/overworld/map_properties.h"
#include "app/editor/overworld/tile16_editor.h"
#include "app/gfx/arena.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/performance_profiler.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/tilemap.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "app/zelda3/common.h"
#include "app/zelda3/overworld/overworld.h"
#include "app/zelda3/overworld/overworld_map.h"
#include "imgui/imgui.h"
#include "imgui_memory_editor.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze::editor {

using namespace ImGui;

// Scratch space canvas methods
absl::Status OverworldEditor::DrawScratchSpace() {
  // Slot selector
  Text("Scratch Space Slot:");
  for (int i = 0; i < 4; i++) {
    if (i > 0)
      SameLine();
    bool is_current = (current_scratch_slot_ == i);
    if (is_current)
      PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.7f, 0.4f, 1.0f));
    if (Button(std::to_string(i + 1).c_str(), ImVec2(25, 25))) {
      current_scratch_slot_ = i;
    }
    if (is_current)
      PopStyleColor();
  }

  SameLine();
  if (Button("Save Selection")) {
    RETURN_IF_ERROR(SaveCurrentSelectionToScratch(current_scratch_slot_));
  }
  SameLine();
  if (Button("Load")) {
    RETURN_IF_ERROR(LoadScratchToSelection(current_scratch_slot_));
  }
  SameLine();
  if (Button("Clear")) {
    RETURN_IF_ERROR(ClearScratchSpace(current_scratch_slot_));
  }

  // Selection transfer buttons
  Separator();
  Text("Selection Transfer:");
  if (Button(ICON_MD_DOWNLOAD " From Overworld")) {
    // Transfer current overworld selection to scratch space
    if (ow_map_canvas_.select_rect_active() &&
        !ow_map_canvas_.selected_tiles().empty()) {
      RETURN_IF_ERROR(SaveCurrentSelectionToScratch(current_scratch_slot_));
    }
  }
  HOVER_HINT("Copy current overworld selection to this scratch slot");

  SameLine();
  if (Button(ICON_MD_UPLOAD " To Clipboard")) {
    // Copy scratch selection to clipboard for pasting in overworld
    if (scratch_canvas_.select_rect_active() &&
        !scratch_canvas_.selected_tiles().empty()) {
      // Copy scratch selection to clipboard
      std::vector<int> scratch_tile_ids;
      for (const auto& tile_pos : scratch_canvas_.selected_tiles()) {
        int tile_x = static_cast<int>(tile_pos.x) / 32;
        int tile_y = static_cast<int>(tile_pos.y) / 32;
        if (tile_x >= 0 && tile_x < 32 && tile_y >= 0 && tile_y < 32) {
          scratch_tile_ids.push_back(
              scratch_spaces_[current_scratch_slot_].tile_data[tile_x][tile_y]);
        }
      }
      if (!scratch_tile_ids.empty() && context_) {
        const auto& points = scratch_canvas_.selected_points();
        int width =
            std::abs(static_cast<int>((points[1].x - points[0].x) / 32)) + 1;
        int height =
            std::abs(static_cast<int>((points[1].y - points[0].y) / 32)) + 1;
        context_->shared_clipboard.overworld_tile16_ids =
            std::move(scratch_tile_ids);
        context_->shared_clipboard.overworld_width = width;
        context_->shared_clipboard.overworld_height = height;
        context_->shared_clipboard.has_overworld_tile16 = true;
      }
    }
  }
  HOVER_HINT("Copy scratch selection to clipboard for pasting in overworld");

  if (context_ && context_->shared_clipboard.has_overworld_tile16) {
    Text(ICON_MD_CONTENT_PASTE
         " Pattern ready! Use Shift+Click to stamp, or paste in overworld");
  }

  Text("Slot %d: %s (%dx%d)", current_scratch_slot_ + 1,
       scratch_spaces_[current_scratch_slot_].name.c_str(),
       scratch_spaces_[current_scratch_slot_].width,
       scratch_spaces_[current_scratch_slot_].height);
  Text(
      "Select tiles from Tile16 tab or make selections in overworld, then draw "
      "here!");

  // Initialize scratch bitmap with proper size based on scratch space dimensions
  auto& current_slot = scratch_spaces_[current_scratch_slot_];
  if (!current_slot.scratch_bitmap.is_active()) {
    // Create bitmap based on scratch space dimensions (each tile is 16x16)
    int bitmap_width = current_slot.width * 16;
    int bitmap_height = current_slot.height * 16;
    std::vector<uint8_t> empty_data(bitmap_width * bitmap_height, 0);
    current_slot.scratch_bitmap.Create(bitmap_width, bitmap_height, 8,
                                       empty_data);
    if (all_gfx_loaded_) {
      palette_ = overworld_.current_area_palette();
      current_slot.scratch_bitmap.SetPalette(palette_);
      core::Renderer::Get().RenderBitmap(&current_slot.scratch_bitmap);
    }
  }

  // Draw the scratch space canvas with dynamic sizing
  gui::BeginPadding(3);
  ImGui::BeginGroup();

  // Set proper content size for scrolling based on scratch space dimensions
  ImVec2 scratch_content_size(current_slot.width * 16 + 4,
                              current_slot.height * 16 + 4);
  gui::BeginChildWithScrollbar("##ScratchSpaceScrollRegion",
                               scratch_content_size);
  scratch_canvas_.DrawBackground();
  gui::EndPadding();

  // Disable context menu for scratch space to allow right-click selection
  scratch_canvas_.SetContextMenuEnabled(false);

  // Draw the scratch bitmap with proper scaling
  if (current_slot.scratch_bitmap.is_active()) {
    scratch_canvas_.DrawBitmap(current_slot.scratch_bitmap, 2, 2, 1.0f);
  }

  // Simplified scratch space - just basic tile drawing like the original
  if (map_blockset_loaded_) {
    scratch_canvas_.DrawTileSelector(32.0f);
  }

  scratch_canvas_.DrawGrid();
  scratch_canvas_.DrawOverlay();

  EndChild();
  ImGui::EndGroup();

  return absl::OkStatus();
}

void OverworldEditor::DrawScratchSpaceEdits() {
  // Handle painting like the main overworld - continuous drawing
  auto mouse_position = scratch_canvas_.drawn_tile_position();

  // Use the scratch canvas scale and grid settings
  float canvas_scale = scratch_canvas_.global_scale();
  int grid_size =
      32;  // 32x32 grid for scratch space (matches kOverworldCanvasSize)

  // Calculate tile position using proper canvas scaling
  int tile_x = static_cast<int>(mouse_position.x) / grid_size;
  int tile_y = static_cast<int>(mouse_position.y) / grid_size;

  // Get current scratch slot dimensions
  auto& current_slot = scratch_spaces_[current_scratch_slot_];
  int max_width = current_slot.width > 0 ? current_slot.width : 20;
  int max_height = current_slot.height > 0 ? current_slot.height : 30;

  // Bounds check for current scratch space dimensions
  if (tile_x >= 0 && tile_x < max_width && tile_y >= 0 && tile_y < max_height) {
    // Bounds check for our tile_data array (always 32x32 max)
    if (tile_x < 32 && tile_y < 32) {
      current_slot.tile_data[tile_x][tile_y] = current_tile16_;
    }

    // Update the bitmap immediately for visual feedback
    UpdateScratchBitmapTile(tile_x, tile_y, current_tile16_);

    // Mark this scratch space as in use
    if (!current_slot.in_use) {
      current_slot.in_use = true;
      current_slot.name =
          absl::StrFormat("Layout %d", current_scratch_slot_ + 1);
    }
  }
}

void OverworldEditor::DrawScratchSpacePattern() {
  // Handle drawing patterns from overworld selections
  auto mouse_position = scratch_canvas_.drawn_tile_position();

  // Use 32x32 grid size (same as scratch canvas grid)
  int start_tile_x = static_cast<int>(mouse_position.x) / 32;
  int start_tile_y = static_cast<int>(mouse_position.y) / 32;

  // Get the selected tiles from overworld via clipboard
  if (!context_ || !context_->shared_clipboard.has_overworld_tile16) {
    return;
  }

  const auto& tile_ids = context_->shared_clipboard.overworld_tile16_ids;
  int pattern_width = context_->shared_clipboard.overworld_width;
  int pattern_height = context_->shared_clipboard.overworld_height;

  if (tile_ids.empty())
    return;

  auto& current_slot = scratch_spaces_[current_scratch_slot_];
  int max_width = current_slot.width > 0 ? current_slot.width : 20;
  int max_height = current_slot.height > 0 ? current_slot.height : 30;

  // Draw the pattern to scratch space
  int idx = 0;
  for (int py = 0; py < pattern_height && (start_tile_y + py) < max_height;
       ++py) {
    for (int px = 0; px < pattern_width && (start_tile_x + px) < max_width;
         ++px) {
      if (idx < static_cast<int>(tile_ids.size())) {
        int tile_id = tile_ids[idx];
        int scratch_x = start_tile_x + px;
        int scratch_y = start_tile_y + py;

        // Bounds check for tile_data array
        if (scratch_x >= 0 && scratch_x < 32 && scratch_y >= 0 &&
            scratch_y < 32) {
          current_slot.tile_data[scratch_x][scratch_y] = tile_id;
          UpdateScratchBitmapTile(scratch_x, scratch_y, tile_id);
        }
        idx++;
      }
    }
  }

  // Mark scratch space as modified
  current_slot.in_use = true;
  if (current_slot.name == "Empty") {
    current_slot.name =
        absl::StrFormat("Pattern %dx%d", pattern_width, pattern_height);
  }
}

void OverworldEditor::UpdateScratchBitmapTile(int tile_x, int tile_y,
                                              int tile_id, int slot) {
  gfx::ScopedTimer timer("overworld_update_scratch_tile");

  // Use current slot if not specified
  if (slot == -1)
    slot = current_scratch_slot_;

  // Get the tile data from the tile16 blockset
  auto tile_data = gfx::GetTilemapData(tile16_blockset_, tile_id);
  if (tile_data.empty())
    return;

  auto& scratch_slot = scratch_spaces_[slot];

  // Use canvas grid size (32x32) for consistent scaling
  const int grid_size = 32;
  int scratch_bitmap_width = scratch_slot.scratch_bitmap.width();
  int scratch_bitmap_height = scratch_slot.scratch_bitmap.height();

  // Calculate pixel position in scratch bitmap
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      int src_index = y * 16 + x;

      // Scale to grid size - each tile takes up grid_size x grid_size pixels
      int dst_x = tile_x * grid_size + x + x;  // Double scaling for 32x32 grid
      int dst_y = tile_y * grid_size + y + y;

      // Bounds check for scratch bitmap
      if (dst_x >= 0 && dst_x < scratch_bitmap_width && dst_y >= 0 &&
          dst_y < scratch_bitmap_height &&
          src_index < static_cast<int>(tile_data.size())) {

        // Write 2x2 pixel blocks to fill the 32x32 grid space
        for (int py = 0; py < 2 && (dst_y + py) < scratch_bitmap_height; ++py) {
          for (int px = 0; px < 2 && (dst_x + px) < scratch_bitmap_width;
               ++px) {
            int dst_index = (dst_y + py) * scratch_bitmap_width + (dst_x + px);
            scratch_slot.scratch_bitmap.WriteToPixel(dst_index,
                                                     tile_data[src_index]);
          }
        }
      }
    }
  }

  scratch_slot.scratch_bitmap.set_modified(true);
  // Use batch operations for texture updates
  scratch_slot.scratch_bitmap.QueueTextureUpdate(
      core::Renderer::Get().renderer());
  scratch_slot.in_use = true;
}

absl::Status OverworldEditor::SaveCurrentSelectionToScratch(int slot) {
  gfx::ScopedTimer timer("overworld_save_selection_to_scratch");

  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch slot");
  }

  if (ow_map_canvas_.select_rect_active() &&
      !ow_map_canvas_.selected_tiles().empty()) {
    // Calculate actual selection dimensions from overworld rectangle
    const auto& selected_points = ow_map_canvas_.selected_points();
    if (selected_points.size() >= 2) {
      const auto start = selected_points[0];
      const auto end = selected_points[1];

      // Calculate width and height in tiles
      int selection_width =
          std::abs(static_cast<int>((end.x - start.x) / 16)) + 1;
      int selection_height =
          std::abs(static_cast<int>((end.y - start.y) / 16)) + 1;

      // Update scratch space dimensions to match selection
      scratch_spaces_[slot].width = std::max(1, std::min(selection_width, 32));
      scratch_spaces_[slot].height =
          std::max(1, std::min(selection_height, 32));
      scratch_spaces_[slot].in_use = true;
      scratch_spaces_[slot].name =
          absl::StrFormat("Selection %dx%d", scratch_spaces_[slot].width,
                          scratch_spaces_[slot].height);

      // Recreate bitmap with new dimensions
      int bitmap_width = scratch_spaces_[slot].width * 16;
      int bitmap_height = scratch_spaces_[slot].height * 16;
      std::vector<uint8_t> empty_data(bitmap_width * bitmap_height, 0);
      scratch_spaces_[slot].scratch_bitmap.Create(bitmap_width, bitmap_height,
                                                  8, empty_data);
      if (all_gfx_loaded_) {
        palette_ = overworld_.current_area_palette();
        scratch_spaces_[slot].scratch_bitmap.SetPalette(palette_);
        core::Renderer::Get().RenderBitmap(
            &scratch_spaces_[slot].scratch_bitmap);
      }

      // Save selected tiles to scratch data with proper layout
      overworld_.set_current_world(current_world_);
      overworld_.set_current_map(current_map_);

      int idx = 0;
      for (int y = 0;
           y < scratch_spaces_[slot].height &&
           idx < static_cast<int>(ow_map_canvas_.selected_tiles().size());
           ++y) {
        for (int x = 0;
             x < scratch_spaces_[slot].width &&
             idx < static_cast<int>(ow_map_canvas_.selected_tiles().size());
             ++x) {
          if (idx < static_cast<int>(ow_map_canvas_.selected_tiles().size())) {
            int tile_id = overworld_.GetTileFromPosition(
                ow_map_canvas_.selected_tiles()[idx]);
            if (x < 32 && y < 32) {
              scratch_spaces_[slot].tile_data[x][y] = tile_id;
            }
            // Update the bitmap immediately
            UpdateScratchBitmapTile(x, y, tile_id, slot);
            idx++;
          }
        }
      }
    }
  } else {
    // Default single-tile scratch space
    scratch_spaces_[slot].width = 16;  // Default size
    scratch_spaces_[slot].height = 16;
    scratch_spaces_[slot].name = absl::StrFormat("Map %d Area", current_map_);
    scratch_spaces_[slot].in_use = true;
  }

  // Process all queued texture updates at once
  gfx::Arena::Get().ProcessBatchTextureUpdates();

  return absl::OkStatus();
}

absl::Status OverworldEditor::LoadScratchToSelection(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch slot");
  }

  if (!scratch_spaces_[slot].in_use) {
    return absl::FailedPreconditionError("Scratch slot is empty");
  }

  // Placeholder - could restore tiles to current map position
  util::logf("Loading scratch slot %d: %s", slot,
             scratch_spaces_[slot].name.c_str());

  return absl::OkStatus();
}

absl::Status OverworldEditor::ClearScratchSpace(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch slot");
  }

  scratch_spaces_[slot].in_use = false;
  scratch_spaces_[slot].name = "Empty";

  // Clear the bitmap
  if (scratch_spaces_[slot].scratch_bitmap.is_active()) {
    auto& data = scratch_spaces_[slot].scratch_bitmap.mutable_data();
    std::fill(data.begin(), data.end(), 0);
    scratch_spaces_[slot].scratch_bitmap.set_modified(true);
    core::Renderer::Get().UpdateBitmap(&scratch_spaces_[slot].scratch_bitmap);
  }

  return absl::OkStatus();
}

}