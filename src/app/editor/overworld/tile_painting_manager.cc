// Related header
#include "app/editor/overworld/tile_painting_manager.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "app/editor/overworld/tile16_editor.h"
#include "app/editor/overworld/ui_constants.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/canvas/canvas_usage_tracker.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

TilePaintingManager::TilePaintingManager(
    const TilePaintingDependencies& deps,
    const TilePaintingCallbacks& callbacks)
    : deps_(deps), callbacks_(callbacks) {}

// ---------------------------------------------------------------------------
// DrawOverworldEdits - single-tile painting on left-click / drag
// ---------------------------------------------------------------------------
void TilePaintingManager::DrawOverworldEdits() {
  // Determine which overworld map the user is currently editing.
  // drawn_tile_position() returns scaled coordinates, need to unscale
  auto scaled_position = deps_.ow_map_canvas->drawn_tile_position();
  float scale = deps_.ow_map_canvas->global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;

  // Convert scaled position to world coordinates
  ImVec2 mouse_position =
      ImVec2(scaled_position.x / scale, scaled_position.y / scale);

  int map_x = static_cast<int>(mouse_position.x) / kOverworldMapSize;
  int map_y = static_cast<int>(mouse_position.y) / kOverworldMapSize;
  *deps_.current_map = map_x + map_y * 8;
  if (*deps_.current_world == 1) {
    *deps_.current_map += 0x40;
  } else if (*deps_.current_world == 2) {
    *deps_.current_map += 0x80;
  }

  // Bounds checking to prevent crashes
  if (*deps_.current_map < 0 ||
      *deps_.current_map >= static_cast<int>(deps_.maps_bmp->size())) {
    return;  // Invalid map index, skip drawing
  }

  // Validate tile16_blockset_ before calling GetTilemapData
  if (!deps_.tile16_blockset->atlas.is_active() ||
      deps_.tile16_blockset->atlas.vector().empty()) {
    LOG_ERROR(
        "TilePaintingManager",
        "Error: tile16_blockset is not properly initialized (active: %s, "
        "size: %zu)",
        deps_.tile16_blockset->atlas.is_active() ? "true" : "false",
        deps_.tile16_blockset->atlas.vector().size());
    return;  // Skip drawing if blockset is invalid
  }

  // Render the updated map bitmap.
  auto tile_data =
      gfx::GetTilemapData(*deps_.tile16_blockset, *deps_.current_tile16);
  RenderUpdatedMapBitmap(mouse_position, tile_data);

  // Calculate the correct superX and superY values
  const int world_offset = *deps_.current_world * 0x40;
  const int local_map = *deps_.current_map - world_offset;
  const int superY = local_map / 8;
  const int superX = local_map % 8;
  int mouse_x_i = static_cast<int>(mouse_position.x);
  int mouse_y_i = static_cast<int>(mouse_position.y);
  // Calculate the correct tile16_x and tile16_y positions
  int tile16_x = (mouse_x_i % kOverworldMapSize) / (kOverworldMapSize / 32);
  int tile16_y = (mouse_y_i % kOverworldMapSize) / (kOverworldMapSize / 32);

  // Update the overworld map_tiles based on tile16 ID and current world
  auto& selected_world =
      (*deps_.current_world == 0)
          ? deps_.overworld->mutable_map_tiles()->light_world
      : (*deps_.current_world == 1)
          ? deps_.overworld->mutable_map_tiles()->dark_world
          : deps_.overworld->mutable_map_tiles()->special_world;

  int index_x = superX * 32 + tile16_x;
  int index_y = superY * 32 + tile16_y;

  // Get old tile value for undo tracking
  int old_tile_id = selected_world[index_x][index_y];

  // Only record undo if tile is actually changing
  if (old_tile_id != *deps_.current_tile16) {
    callbacks_.create_undo_point(*deps_.current_map, *deps_.current_world,
                                 index_x, index_y, old_tile_id);
    deps_.rom->set_dirty(true);
  }

  selected_world[index_x][index_y] = *deps_.current_tile16;
}

// ---------------------------------------------------------------------------
// RenderUpdatedMapBitmap - update bitmap pixels after tile paint
// ---------------------------------------------------------------------------
void TilePaintingManager::RenderUpdatedMapBitmap(
    const ImVec2& click_position, const std::vector<uint8_t>& tile_data) {
  // Bounds checking to prevent crashes
  if (*deps_.current_map < 0 ||
      *deps_.current_map >= static_cast<int>(deps_.maps_bmp->size())) {
    LOG_ERROR("TilePaintingManager",
              "ERROR: RenderUpdatedMapBitmap - Invalid current_map %d "
              "(maps_bmp size=%zu)",
              *deps_.current_map, deps_.maps_bmp->size());
    return;  // Invalid map index, skip rendering
  }

  // Calculate the tile index for x and y based on the click_position
  int tile_index_x =
      (static_cast<int>(click_position.x) % kOverworldMapSize) / kTile16Size;
  int tile_index_y =
      (static_cast<int>(click_position.y) % kOverworldMapSize) / kTile16Size;

  // Calculate the pixel start position based on tile index and tile size
  ImVec2 start_position;
  start_position.x = static_cast<float>(tile_index_x * kTile16Size);
  start_position.y = static_cast<float>(tile_index_y * kTile16Size);

  // Update the bitmap's pixel data based on the start_position and tile_data
  gfx::Bitmap& current_bitmap = (*deps_.maps_bmp)[*deps_.current_map];

  // Validate bitmap state before writing
  if (!current_bitmap.is_active() || current_bitmap.size() == 0) {
    LOG_ERROR(
        "TilePaintingManager",
        "ERROR: RenderUpdatedMapBitmap - Bitmap %d is not active or has no "
        "data (active=%s, size=%zu)",
        *deps_.current_map, current_bitmap.is_active() ? "true" : "false",
        current_bitmap.size());
    return;
  }

  for (int y = 0; y < kTile16Size; ++y) {
    for (int x = 0; x < kTile16Size; ++x) {
      int pixel_index =
          (start_position.y + y) * kOverworldMapSize + (start_position.x + x);

      // Bounds check for pixel index
      if (pixel_index < 0 ||
          pixel_index >= static_cast<int>(current_bitmap.size())) {
        LOG_ERROR(
            "TilePaintingManager",
            "ERROR: RenderUpdatedMapBitmap - pixel_index %d out of bounds "
            "(bitmap size=%zu)",
            pixel_index, current_bitmap.size());
        continue;
      }

      // Bounds check for tile data
      int tile_data_index = y * kTile16Size + x;
      if (tile_data_index < 0 ||
          tile_data_index >= static_cast<int>(tile_data.size())) {
        LOG_ERROR(
            "TilePaintingManager",
            "ERROR: RenderUpdatedMapBitmap - tile_data_index %d out of bounds "
            "(tile_data size=%zu)",
            tile_data_index, tile_data.size());
        continue;
      }

      current_bitmap.WriteToPixel(pixel_index, tile_data[tile_data_index]);
    }
  }

  current_bitmap.set_modified(true);

  // Immediately update the texture to reflect changes
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &current_bitmap);
}

// ---------------------------------------------------------------------------
// CheckForOverworldEdits - main painting entry point
// ---------------------------------------------------------------------------
void TilePaintingManager::CheckForOverworldEdits() {
  LOG_DEBUG("TilePaintingManager", "CheckForOverworldEdits: Frame %d",
            ImGui::GetFrameCount());

  CheckForSelectRectangle();

  // User has selected a tile they want to draw from the blockset
  // and clicked on the canvas.
  if (*deps_.current_mode == EditingMode::DRAW_TILE &&
      *deps_.current_tile16 >= 0 &&
      !deps_.ow_map_canvas->select_rect_active() &&
      deps_.ow_map_canvas->DrawTilemapPainter(*deps_.tile16_blockset,
                                               *deps_.current_tile16)) {
    DrawOverworldEdits();
  }

  // Fill tool: fill the entire 32x32 tile16 screen under the cursor using the
  // current selection pattern (if any) or the current tile16.
  if (*deps_.current_mode == EditingMode::FILL_TILE &&
      deps_.ow_map_canvas->IsMouseHovering() &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    float scale = deps_.ow_map_canvas->global_scale();
    if (scale <= 0.0f) {
      scale = 1.0f;
    }

    const bool allow_special_tail =
        core::FeatureFlags::get().overworld.kEnableSpecialWorldExpansion;
    const auto scaled_position = deps_.ow_map_canvas->hover_mouse_pos();
    const int map_x =
        static_cast<int>(scaled_position.x / scale) / kOverworldMapSize;
    const int map_y =
        static_cast<int>(scaled_position.y / scale) / kOverworldMapSize;

    // Bounds guard.
    if (map_x >= 0 && map_x < 8 && map_y >= 0 && map_y < 8) {
      // Special world only renders 4 rows unless tail expansion is enabled.
      if (allow_special_tail || *deps_.current_world != 2 || map_y < 4) {
        const int local_map = map_x + (map_y * 8);
        const int target_map = local_map + (*deps_.current_world * 0x40);
        if (target_map >= 0 && target_map < zelda3::kNumOverworldMaps) {
          // Build pattern from active rectangle selection (if present).
          std::vector<int> pattern_ids;
          int pattern_w = 1;
          int pattern_h = 1;

          if (deps_.ow_map_canvas->select_rect_active() &&
              deps_.ow_map_canvas->selected_points().size() >= 2) {
            const auto start = deps_.ow_map_canvas->selected_points()[0];
            const auto end = deps_.ow_map_canvas->selected_points()[1];

            const int start_x =
                static_cast<int>(std::floor(std::min(start.x, end.x) / 16.0f));
            const int end_x =
                static_cast<int>(std::floor(std::max(start.x, end.x) / 16.0f));
            const int start_y =
                static_cast<int>(std::floor(std::min(start.y, end.y) / 16.0f));
            const int end_y =
                static_cast<int>(std::floor(std::max(start.y, end.y) / 16.0f));

            pattern_w = std::max(1, end_x - start_x + 1);
            pattern_h = std::max(1, end_y - start_y + 1);
            pattern_ids.reserve(pattern_w * pattern_h);

            deps_.overworld->set_current_world(*deps_.current_world);
            deps_.overworld->set_current_map(target_map);
            for (int y = start_y; y <= end_y; ++y) {
              for (int x = start_x; x <= end_x; ++x) {
                pattern_ids.push_back(deps_.overworld->GetTile(x, y));
              }
            }
          } else {
            pattern_ids = {*deps_.current_tile16};
          }

          auto& world_tiles =
              (*deps_.current_world == 0)
                  ? deps_.overworld->mutable_map_tiles()->light_world
              : (*deps_.current_world == 1)
                  ? deps_.overworld->mutable_map_tiles()->dark_world
                  : deps_.overworld->mutable_map_tiles()->special_world;

          // Apply the fill (repeat pattern across 32x32).
          for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
              const int pattern_x = x % pattern_w;
              const int pattern_y = y % pattern_h;
              const int new_tile_id =
                  pattern_ids[pattern_y * pattern_w + pattern_x];

              const int global_x = map_x * 32 + x;
              const int global_y = map_y * 32 + y;
              if (global_x < 0 || global_x >= 256 || global_y < 0 ||
                  global_y >= 256) {
                continue;
              }

              const int old_tile_id = world_tiles[global_x][global_y];
              if (old_tile_id == new_tile_id) {
                continue;
              }

              callbacks_.create_undo_point(target_map, *deps_.current_world,
                                           global_x, global_y, old_tile_id);
              world_tiles[global_x][global_y] = new_tile_id;
            }
          }

          deps_.rom->set_dirty(true);
          callbacks_.finalize_paint_operation();
          *deps_.current_map = target_map;
          callbacks_.refresh_overworld_map_on_demand(target_map);
        }
      }
    }
  }

  // Rectangle selection stamping (brush mode only).
  if (*deps_.current_mode == EditingMode::DRAW_TILE &&
      deps_.ow_map_canvas->select_rect_active()) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      LOG_DEBUG("TilePaintingManager",
                "CheckForOverworldEdits: About to apply rectangle selection");

      auto& selected_world =
          (*deps_.current_world == 0)
              ? deps_.overworld->mutable_map_tiles()->light_world
          : (*deps_.current_world == 1)
              ? deps_.overworld->mutable_map_tiles()->dark_world
              : deps_.overworld->mutable_map_tiles()->special_world;
      // selected_points are now stored in world coordinates
      auto start = deps_.ow_map_canvas->selected_points()[0];
      auto end = deps_.ow_map_canvas->selected_points()[1];

      // Calculate the bounds of the rectangle in terms of 16x16 tile indices
      int start_x = std::floor(start.x / kTile16Size) * kTile16Size;
      int start_y = std::floor(start.y / kTile16Size) * kTile16Size;
      int end_x = std::floor(end.x / kTile16Size) * kTile16Size;
      int end_y = std::floor(end.y / kTile16Size) * kTile16Size;

      if (start_x > end_x)
        std::swap(start_x, end_x);
      if (start_y > end_y)
        std::swap(start_y, end_y);

      constexpr int local_map_size = 512;  // Size of each local map
      // Number of tiles per local map (since each tile is 16x16)
      constexpr int tiles_per_local_map = local_map_size / kTile16Size;

      LOG_DEBUG("TilePaintingManager",
                "CheckForOverworldEdits: About to fill rectangle with "
                "current_tile16=%d",
                *deps_.current_tile16);

      // Apply the selected tiles to each position in the rectangle
      // CRITICAL FIX: Use pre-computed tile16_ids instead of recalculating
      // from selected_tiles. This prevents wrapping issues when dragging near
      // boundaries.
      int i = 0;
      for (int y = start_y;
           y <= end_y &&
           i < static_cast<int>(deps_.selected_tile16_ids->size());
           y += kTile16Size) {
        for (int x = start_x;
             x <= end_x &&
             i < static_cast<int>(deps_.selected_tile16_ids->size());
             x += kTile16Size, ++i) {
          // Determine which local map (512x512) the tile is in
          int local_map_x = x / local_map_size;
          int local_map_y = y / local_map_size;

          // Calculate the tile's position within its local map
          int tile16_x = (x % local_map_size) / kTile16Size;
          int tile16_y = (y % local_map_size) / kTile16Size;

          // Calculate the index within the overall map structure
          int index_x = local_map_x * tiles_per_local_map + tile16_x;
          int index_y = local_map_y * tiles_per_local_map + tile16_y;

          // FIXED: Use pre-computed tile ID from the ORIGINAL selection
          int tile16_id = (*deps_.selected_tile16_ids)[i];
          // Bounds check for the selected world array
          int rect_width = ((end_x - start_x) / kTile16Size) + 1;
          int rect_height = ((end_y - start_y) / kTile16Size) + 1;

          // Prevent painting from wrapping around at the edges of large maps
          int start_local_map_x = start_x / local_map_size;
          int start_local_map_y = start_y / local_map_size;
          int end_local_map_x = end_x / local_map_size;
          int end_local_map_y = end_y / local_map_size;

          bool in_same_local_map = (start_local_map_x == end_local_map_x) &&
                                   (start_local_map_y == end_local_map_y);

          if (in_same_local_map && index_x >= 0 &&
              (index_x + rect_width - 1) < 0x200 && index_y >= 0 &&
              (index_y + rect_height - 1) < 0x200) {
            // Get old tile value for undo tracking
            int old_tile_id = selected_world[index_x][index_y];
            if (old_tile_id != tile16_id) {
              callbacks_.create_undo_point(*deps_.current_map,
                                           *deps_.current_world, index_x,
                                           index_y, old_tile_id);
            }

            selected_world[index_x][index_y] = tile16_id;

            // CRITICAL FIX: Also update the bitmap directly like single tile
            // drawing
            ImVec2 tile_position(x, y);
            auto tile_data =
                gfx::GetTilemapData(*deps_.tile16_blockset, tile16_id);
            if (!tile_data.empty()) {
              RenderUpdatedMapBitmap(tile_position, tile_data);
              LOG_DEBUG(
                  "TilePaintingManager",
                  "CheckForOverworldEdits: Updated bitmap at position (%d,%d) "
                  "with tile16_id=%d",
                  x, y, tile16_id);
            } else {
              LOG_ERROR("TilePaintingManager",
                        "ERROR: Failed to get tile data for tile16_id=%d",
                        tile16_id);
            }
          }
        }
      }

      // Finalize the undo batch operation after all tiles are placed
      callbacks_.finalize_paint_operation();

      deps_.rom->set_dirty(true);
      callbacks_.refresh_overworld_map();
    }
  }
}

// ---------------------------------------------------------------------------
// CheckForSelectRectangle - rectangle drag-to-select tiles
// ---------------------------------------------------------------------------
void TilePaintingManager::CheckForSelectRectangle() {
  // Pass the canvas scale for proper zoom handling
  float scale = deps_.ow_map_canvas->global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;
  deps_.ow_map_canvas->DrawSelectRect(*deps_.current_map, 0x10, scale);

  // Single tile case
  if (deps_.ow_map_canvas->selected_tile_pos().x != -1) {
    *deps_.current_tile16 =
        deps_.overworld->GetTileFromPosition(
            deps_.ow_map_canvas->selected_tile_pos());
    deps_.ow_map_canvas->set_selected_tile_pos(ImVec2(-1, -1));

    // Scroll blockset canvas to show the selected tile
    callbacks_.scroll_blockset_to_current_tile();
  }

  // Rectangle selection case - use member variable instead of static local
  if (deps_.ow_map_canvas->select_rect_active()) {
    // Get the tile16 IDs from the selected tile ID positions
    deps_.selected_tile16_ids->clear();

    if (deps_.ow_map_canvas->selected_tiles().size() > 0) {
      // Set the current world and map in overworld for proper tile lookup
      deps_.overworld->set_current_world(*deps_.current_world);
      deps_.overworld->set_current_map(*deps_.current_map);
      for (auto& each : deps_.ow_map_canvas->selected_tiles()) {
        deps_.selected_tile16_ids->push_back(
            deps_.overworld->GetTileFromPosition(each));
      }
    }
  }
  // Create a composite image of all the tile16s selected
  deps_.ow_map_canvas->DrawBitmapGroup(*deps_.selected_tile16_ids,
                                        *deps_.tile16_blockset, 0x10,
                                        deps_.ow_map_canvas->global_scale());
}

// ---------------------------------------------------------------------------
// PickTile16FromHoveredCanvas - eyedropper tool
// ---------------------------------------------------------------------------
bool TilePaintingManager::PickTile16FromHoveredCanvas() {
  if (!deps_.ow_map_canvas->IsMouseHovering()) {
    return false;
  }

  const bool allow_special_tail =
      core::FeatureFlags::get().overworld.kEnableSpecialWorldExpansion;

  const ImVec2 scaled_position = deps_.ow_map_canvas->hover_mouse_pos();
  float scale = deps_.ow_map_canvas->global_scale();
  if (scale <= 0.0f) {
    scale = 1.0f;
  }

  const int map_x =
      static_cast<int>(scaled_position.x / scale) / kOverworldMapSize;
  const int map_y =
      static_cast<int>(scaled_position.y / scale) / kOverworldMapSize;
  if (map_x < 0 || map_x >= 8 || map_y < 0 || map_y >= 8) {
    return false;
  }
  if (!allow_special_tail && *deps_.current_world == 2 && map_y >= 4) {
    return false;
  }

  const int local_tile_x =
      (static_cast<int>(scaled_position.x / scale) % kOverworldMapSize) /
      kTile16Size;
  const int local_tile_y =
      (static_cast<int>(scaled_position.y / scale) % kOverworldMapSize) /
      kTile16Size;
  if (local_tile_x < 0 || local_tile_x >= 32 || local_tile_y < 0 ||
      local_tile_y >= 32) {
    return false;
  }

  const int world_tile_x = map_x * 32 + local_tile_x;
  const int world_tile_y = map_y * 32 + local_tile_y;
  if (world_tile_x < 0 || world_tile_x >= 256 || world_tile_y < 0 ||
      world_tile_y >= 256) {
    return false;
  }

  const auto* map_tiles = deps_.overworld->mutable_map_tiles();
  if (!map_tiles) {
    return false;
  }
  const auto& world_tiles =
      (*deps_.current_world == 0)   ? map_tiles->light_world
      : (*deps_.current_world == 1) ? map_tiles->dark_world
                                    : map_tiles->special_world;
  const int tile_id = world_tiles[world_tile_x][world_tile_y];
  if (tile_id < 0) {
    return false;
  }

  *deps_.current_tile16 = tile_id;
  auto set_tile_status = deps_.tile16_editor->SetCurrentTile(*deps_.current_tile16);
  if (!set_tile_status.ok()) {
    util::logf("Failed to sync Tile16 editor after eyedropper: %s",
               set_tile_status.message().data());
  }

  callbacks_.scroll_blockset_to_current_tile();
  return true;
}

// ---------------------------------------------------------------------------
// ToggleBrushTool - switch between DRAW_TILE and MOUSE modes
// ---------------------------------------------------------------------------
void TilePaintingManager::ToggleBrushTool() {
  if (*deps_.current_mode == EditingMode::DRAW_TILE) {
    *deps_.current_mode = EditingMode::MOUSE;
  } else {
    *deps_.current_mode = EditingMode::DRAW_TILE;
  }

  if (*deps_.current_mode == EditingMode::MOUSE) {
    deps_.ow_map_canvas->SetUsageMode(gui::CanvasUsage::kEntityManipulation);
  } else {
    deps_.ow_map_canvas->SetUsageMode(gui::CanvasUsage::kTilePainting);
  }
}

// ---------------------------------------------------------------------------
// ActivateFillTool - toggle FILL_TILE mode on/off
// ---------------------------------------------------------------------------
void TilePaintingManager::ActivateFillTool() {
  if (*deps_.current_mode == EditingMode::FILL_TILE) {
    *deps_.current_mode = EditingMode::DRAW_TILE;
  } else {
    *deps_.current_mode = EditingMode::FILL_TILE;
  }

  // Fill tool is still a tile painting interaction.
  deps_.ow_map_canvas->SetUsageMode(gui::CanvasUsage::kTilePainting);
}

}  // namespace yaze::editor
