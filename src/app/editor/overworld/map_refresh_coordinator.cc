// Related header
#include "app/editor/overworld/map_refresh_coordinator.h"

#include "absl/status/status.h"
#include "app/editor/overworld/tile16_editor.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "rom/rom.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::editor {

void MapRefreshCoordinator::InvalidateGraphicsCache(int map_id) {
  if (map_id < 0) {
    // Invalidate all maps - clear both editor cache and Overworld's tileset
    // cache
    ctx_.current_graphics_set->clear();
    ctx_.overworld->ClearGraphicsConfigCache();
  } else {
    // Invalidate specific map and its siblings in the Overworld's tileset cache
    ctx_.current_graphics_set->erase(map_id);
    ctx_.overworld->InvalidateSiblingMapCaches(map_id);
  }
}

void MapRefreshCoordinator::RefreshChildMap(int map_index) {
  ctx_.overworld->mutable_overworld_map(map_index)->LoadAreaGraphics();
  *ctx_.status =
      ctx_.overworld->mutable_overworld_map(map_index)->BuildTileset();
  PRINT_IF_ERROR(*ctx_.status);
  *ctx_.status =
      ctx_.overworld->mutable_overworld_map(map_index)->BuildTiles16Gfx(
          *ctx_.overworld->mutable_tiles16(), ctx_.overworld->tiles16().size());
  PRINT_IF_ERROR(*ctx_.status);
  *ctx_.status = ctx_.overworld->mutable_overworld_map(map_index)->BuildBitmap(
      ctx_.overworld->GetMapTiles(*ctx_.current_world));
  (*ctx_.maps_bmp)[map_index].set_data(
      ctx_.overworld->mutable_overworld_map(map_index)->bitmap_data());
  (*ctx_.maps_bmp)[map_index].set_modified(true);
  PRINT_IF_ERROR(*ctx_.status);
}

void MapRefreshCoordinator::RefreshOverworldMap() {
  // Use the new on-demand refresh system
  RefreshOverworldMapOnDemand(*ctx_.current_map);
}

/**
 * @brief On-demand map refresh that only updates what's actually needed
 *
 * This method intelligently determines what needs to be refreshed based on
 * the type of change and only updates the necessary components, avoiding
 * expensive full rebuilds when possible.
 */
void MapRefreshCoordinator::RefreshOverworldMapOnDemand(int map_index) {
  if (map_index < 0 || map_index >= zelda3::kNumOverworldMaps) {
    return;
  }

  // Check if the map is actually visible or being edited
  bool is_current_map = (map_index == *ctx_.current_map);
  bool is_current_world = (map_index / 0x40 == *ctx_.current_world);

  // For non-current maps in non-current worlds, defer the refresh
  if (!is_current_map && !is_current_world) {
    // Mark for deferred refresh - will be processed when the map becomes
    // visible
    (*ctx_.maps_bmp)[map_index].set_modified(true);
    return;
  }

  // For visible maps, do immediate refresh
  RefreshChildMapOnDemand(map_index);
}

/**
 * @brief On-demand child map refresh with selective updates
 */
void MapRefreshCoordinator::RefreshChildMapOnDemand(int map_index) {
  auto* map = ctx_.overworld->mutable_overworld_map(map_index);
  if (!map) {
    return;  // Map not loaded yet (e.g. Overworld not fully initialized)
  }

  // Check what actually needs to be refreshed
  bool needs_graphics_rebuild = (*ctx_.maps_bmp)[map_index].modified();

  if (needs_graphics_rebuild) {
    // Only rebuild what's actually changed
    map->LoadAreaGraphics();

    // Rebuild tileset only if graphics changed
    auto status = map->BuildTileset();
    if (!status.ok()) {
      LOG_ERROR("MapRefreshCoordinator",
                "Failed to build tileset for map %d: %s", map_index,
                status.message().data());
      return;
    }

    // Rebuild tiles16 graphics
    status = map->BuildTiles16Gfx(*ctx_.overworld->mutable_tiles16(),
                                  ctx_.overworld->tiles16().size());
    if (!status.ok()) {
      LOG_ERROR("MapRefreshCoordinator",
                "Failed to build tiles16 graphics for map %d: %s", map_index,
                status.message().data());
      return;
    }

    // Rebuild bitmap
    status = map->BuildBitmap(ctx_.overworld->GetMapTiles(*ctx_.current_world));
    if (!status.ok()) {
      LOG_ERROR("MapRefreshCoordinator",
                "Failed to build bitmap for map %d: %s", map_index,
                status.message().data());
      return;
    }

    // Update bitmap data
    (*ctx_.maps_bmp)[map_index].set_data(map->bitmap_data());
    (*ctx_.maps_bmp)[map_index].set_modified(false);

    // Validate surface synchronization to help debug crashes
    if (!(*ctx_.maps_bmp)[map_index].ValidateDataSurfaceSync()) {
      LOG_WARN("MapRefreshCoordinator",
               "Warning: Surface synchronization issue detected for map %d",
               map_index);
    }

    // Queue texture update to ensure changes are visible
    if ((*ctx_.maps_bmp)[map_index].texture()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &(*ctx_.maps_bmp)[map_index]);
    } else {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &(*ctx_.maps_bmp)[map_index]);
    }
  }

  // Handle multi-area maps (large, wide, tall) with safe coordination
  // Use centralized version detection
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*ctx_.rom);
  bool use_v3_area_sizes =
      zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version);

  if (use_v3_area_sizes) {
    // Use v3 multi-area coordination
    RefreshMultiAreaMapsSafely(map_index, map);
  } else {
    // Legacy logic: only handle large maps for vanilla/v2
    if (map->is_large_map()) {
      RefreshMultiAreaMapsSafely(map_index, map);
    }
  }
}

/**
 * @brief Safely refresh multi-area maps without recursion
 *
 * This function handles the coordination of large, wide, and tall area maps
 * by using a non-recursive approach with explicit map list processing.
 * It always works from the parent perspective to ensure consistent behavior
 * whether the trigger map is the parent or a child.
 *
 * Key improvements:
 * - Uses parameter-based recursion guard instead of static set
 * - Always works from parent perspective for consistent sibling coordination
 * - Respects ZScream area size logic for v3+ ROMs
 * - Falls back to large_map flag for vanilla/v2 ROMs
 */
void MapRefreshCoordinator::RefreshMultiAreaMapsSafely(
    int map_index, zelda3::OverworldMap* map) {
  using zelda3::AreaSizeEnum;

  auto area_size = map->area_size();
  if (area_size == AreaSizeEnum::SmallArea) {
    return;  // No siblings to coordinate
  }

  // Always work from parent perspective for consistent coordination
  int parent_id = map->parent();

  // If we're not the parent, get the parent map to work from
  auto* parent_map = ctx_.overworld->mutable_overworld_map(parent_id);
  if (!parent_map) {
    LOG_WARN("MapRefreshCoordinator",
             "RefreshMultiAreaMapsSafely: Could not get parent map %d for "
             "map %d",
             parent_id, map_index);
    return;
  }

  LOG_DEBUG("MapRefreshCoordinator",
            "RefreshMultiAreaMapsSafely: Processing %s area from parent %d "
            "(trigger: %d)",
            (area_size == AreaSizeEnum::LargeArea)  ? "large"
            : (area_size == AreaSizeEnum::WideArea) ? "wide"
                                                    : "tall",
            parent_id, map_index);

  // Determine all maps that are part of this multi-area structure
  // based on the parent's position and area size
  std::vector<int> sibling_maps;

  switch (area_size) {
    case AreaSizeEnum::LargeArea:
      // Large Area: 2x2 grid (4 maps total)
      sibling_maps = {parent_id, parent_id + 1, parent_id + 8, parent_id + 9};
      break;

    case AreaSizeEnum::WideArea:
      // Wide Area: 2x1 grid (2 maps total, horizontally adjacent)
      sibling_maps = {parent_id, parent_id + 1};
      break;

    case AreaSizeEnum::TallArea:
      // Tall Area: 1x2 grid (2 maps total, vertically adjacent)
      sibling_maps = {parent_id, parent_id + 8};
      break;

    default:
      LOG_WARN("MapRefreshCoordinator",
               "RefreshMultiAreaMapsSafely: Unknown area size %d for map %d",
               static_cast<int>(area_size), map_index);
      return;
  }

  // Refresh all siblings (including self if different from trigger)
  // The trigger map (map_index) was already processed by the caller,
  // so we skip it to avoid double-processing
  for (int sibling : sibling_maps) {
    // Skip the trigger map - it was already processed by
    // RefreshChildMapOnDemand
    if (sibling == map_index) {
      continue;
    }

    // Bounds check
    if (sibling < 0 || sibling >= zelda3::kNumOverworldMaps) {
      continue;
    }

    // Check visibility - only immediately refresh visible maps
    bool is_current_map = (sibling == *ctx_.current_map);
    bool is_current_world = (sibling / 0x40 == *ctx_.current_world);

    // Always mark sibling as needing refresh to ensure consistency
    (*ctx_.maps_bmp)[sibling].set_modified(true);

    if (is_current_map || is_current_world) {
      LOG_DEBUG("MapRefreshCoordinator",
                "RefreshMultiAreaMapsSafely: Refreshing sibling map %d",
                sibling);

      // Direct refresh for visible siblings
      auto* sibling_map = ctx_.overworld->mutable_overworld_map(sibling);
      if (!sibling_map)
        continue;

      sibling_map->LoadAreaGraphics();

      auto status = sibling_map->BuildTileset();
      if (!status.ok()) {
        LOG_ERROR("MapRefreshCoordinator",
                  "Failed to build tileset for sibling %d: %s", sibling,
                  status.message().data());
        continue;
      }

      status = sibling_map->BuildTiles16Gfx(*ctx_.overworld->mutable_tiles16(),
                                            ctx_.overworld->tiles16().size());
      if (!status.ok()) {
        LOG_ERROR("MapRefreshCoordinator",
                  "Failed to build tiles16 for sibling %d: %s", sibling,
                  status.message().data());
        continue;
      }

      status = sibling_map->LoadPalette();
      if (!status.ok()) {
        LOG_ERROR("MapRefreshCoordinator",
                  "Failed to load palette for sibling %d: %s", sibling,
                  status.message().data());
        continue;
      }

      status = sibling_map->BuildBitmap(
          ctx_.overworld->GetMapTiles(*ctx_.current_world));
      if (!status.ok()) {
        LOG_ERROR("MapRefreshCoordinator",
                  "Failed to build bitmap for sibling %d: %s", sibling,
                  status.message().data());
        continue;
      }

      // Update bitmap data
      (*ctx_.maps_bmp)[sibling].set_data(sibling_map->bitmap_data());

      // Set palette if bitmap has a valid surface
      if ((*ctx_.maps_bmp)[sibling].is_active() &&
          (*ctx_.maps_bmp)[sibling].surface()) {
        (*ctx_.maps_bmp)[sibling].SetPalette(sibling_map->current_palette());
      }
      (*ctx_.maps_bmp)[sibling].set_modified(false);

      // Queue texture update/creation
      if ((*ctx_.maps_bmp)[sibling].texture()) {
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::UPDATE, &(*ctx_.maps_bmp)[sibling]);
      } else {
        if (ctx_.ensure_map_texture) {
          ctx_.ensure_map_texture(sibling);
        }
      }
    }
    // Non-visible siblings remain marked as modified for deferred refresh
  }
}

absl::Status MapRefreshCoordinator::RefreshMapPalette() {
  auto* current_map = ctx_.overworld->mutable_overworld_map(*ctx_.current_map);
  if (!current_map) {
    return absl::FailedPreconditionError("Current overworld map not loaded");
  }
  RETURN_IF_ERROR(current_map->LoadPalette());
  const auto current_map_palette = ctx_.overworld->current_area_palette();
  *ctx_.palette = current_map_palette;
  // Keep tile16 editor in sync with the currently active overworld palette
  ctx_.tile16_editor->set_palette(current_map_palette);
  // Ensure source graphics bitmap uses the refreshed palette so tile8 selector
  // isn't blank.
  if (ctx_.current_gfx_bmp->is_active()) {
    ctx_.current_gfx_bmp->SetPalette(*ctx_.palette);
    ctx_.current_gfx_bmp->set_modified(true);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, ctx_.current_gfx_bmp);
  }

  // Use centralized version detection
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*ctx_.rom);
  bool use_v3_area_sizes =
      zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version);

  if (use_v3_area_sizes) {
    // Use v3 area size system
    using zelda3::AreaSizeEnum;
    auto area_size = current_map->area_size();

    if (area_size != AreaSizeEnum::SmallArea) {
      // Get all sibling maps that need palette updates
      std::vector<int> sibling_maps;
      int parent_id = current_map->parent();

      switch (area_size) {
        case AreaSizeEnum::LargeArea:
          // 2x2 grid: parent, parent+1, parent+8, parent+9
          sibling_maps = {parent_id, parent_id + 1, parent_id + 8,
                          parent_id + 9};
          break;
        case AreaSizeEnum::WideArea:
          // 2x1 grid: parent, parent+1
          sibling_maps = {parent_id, parent_id + 1};
          break;
        case AreaSizeEnum::TallArea:
          // 1x2 grid: parent, parent+8
          sibling_maps = {parent_id, parent_id + 8};
          break;
        default:
          break;
      }

      // Update palette for all siblings - each uses its own loaded palette
      for (int sibling_index : sibling_maps) {
        if (sibling_index < 0 || sibling_index >= zelda3::kNumOverworldMaps) {
          continue;
        }
        auto* sibling_map =
            ctx_.overworld->mutable_overworld_map(sibling_index);
        if (!sibling_map) {
          continue;
        }
        RETURN_IF_ERROR(sibling_map->LoadPalette());
        (*ctx_.maps_bmp)[sibling_index].SetPalette(
            sibling_map->current_palette());
      }
    } else {
      // Small area - only update current map
      (*ctx_.maps_bmp)[*ctx_.current_map].SetPalette(current_map_palette);
    }
  } else {
    // Legacy logic for vanilla and v2 ROMs
    if (current_map->is_large_map()) {
      // We need to update the map and its siblings if it's a large map
      for (int i = 1; i < 4; i++) {
        int sibling_index = current_map->parent() + i;
        if (i >= 2)
          sibling_index += 6;
        auto* sibling_map =
            ctx_.overworld->mutable_overworld_map(sibling_index);
        if (!sibling_map) {
          continue;
        }
        RETURN_IF_ERROR(sibling_map->LoadPalette());

        // SAFETY: Only set palette if bitmap has a valid surface
        // Use sibling map's own loaded palette
        if ((*ctx_.maps_bmp)[sibling_index].is_active() &&
            (*ctx_.maps_bmp)[sibling_index].surface()) {
          (*ctx_.maps_bmp)[sibling_index].SetPalette(
              sibling_map->current_palette());
        }
      }
    }

    // SAFETY: Only set palette if bitmap has a valid surface
    if ((*ctx_.maps_bmp)[*ctx_.current_map].is_active() &&
        (*ctx_.maps_bmp)[*ctx_.current_map].surface()) {
      (*ctx_.maps_bmp)[*ctx_.current_map].SetPalette(current_map_palette);
    }
  }

  return absl::OkStatus();
}

void MapRefreshCoordinator::ForceRefreshGraphics(int map_index) {
  // Mark the bitmap as modified to force refresh on next update
  if (map_index >= 0 && map_index < static_cast<int>(ctx_.maps_bmp->size())) {
    (*ctx_.maps_bmp)[map_index].set_modified(true);

    // Clear blockset cache
    *ctx_.current_blockset = 0xFF;

    // Invalidate Overworld's tileset cache for this map and siblings
    // This ensures stale cached tilesets aren't reused after property changes
    ctx_.overworld->InvalidateSiblingMapCaches(map_index);

    LOG_DEBUG("MapRefreshCoordinator",
              "ForceRefreshGraphics: Map %d marked for refresh", map_index);
  }
}

void MapRefreshCoordinator::RefreshSiblingMapGraphics(int map_index,
                                                      bool include_self) {
  if (map_index < 0 || map_index >= static_cast<int>(ctx_.maps_bmp->size())) {
    return;
  }

  auto* map = ctx_.overworld->mutable_overworld_map(map_index);
  if (map->area_size() == zelda3::AreaSizeEnum::SmallArea) {
    return;  // No siblings for small areas
  }

  int parent_id = map->parent();
  std::vector<int> siblings;

  switch (map->area_size()) {
    case zelda3::AreaSizeEnum::LargeArea:
      siblings = {parent_id, parent_id + 1, parent_id + 8, parent_id + 9};
      break;
    case zelda3::AreaSizeEnum::WideArea:
      siblings = {parent_id, parent_id + 1};
      break;
    case zelda3::AreaSizeEnum::TallArea:
      siblings = {parent_id, parent_id + 8};
      break;
    default:
      return;
  }

  for (int sibling : siblings) {
    if (sibling >= 0 && sibling < 0xA0) {
      // Skip self unless include_self is true
      if (sibling == map_index && !include_self) {
        continue;
      }

      // Mark as modified FIRST before loading
      (*ctx_.maps_bmp)[sibling].set_modified(true);

      // Load graphics from ROM
      ctx_.overworld->mutable_overworld_map(sibling)->LoadAreaGraphics();

      // CRITICAL FIX: Bypass visibility check - force immediate refresh
      // Call RefreshChildMapOnDemand() directly instead of
      // RefreshOverworldMapOnDemand()
      RefreshChildMapOnDemand(sibling);

      LOG_DEBUG("MapRefreshCoordinator",
                "RefreshSiblingMapGraphics: Refreshed sibling map %d", sibling);
    }
  }
}

void MapRefreshCoordinator::RefreshMapProperties() {
  const auto& current_ow_map =
      *ctx_.overworld->mutable_overworld_map(*ctx_.current_map);

  // Use centralized version detection
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*ctx_.rom);
  bool use_v3_area_sizes =
      zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version);

  if (use_v3_area_sizes) {
    // Use v3 area size system
    using zelda3::AreaSizeEnum;
    auto area_size = current_ow_map.area_size();

    if (area_size != AreaSizeEnum::SmallArea) {
      // Get all sibling maps that need property updates
      std::vector<int> sibling_maps;
      int parent_id = current_ow_map.parent();

      switch (area_size) {
        case AreaSizeEnum::LargeArea:
          // 2x2 grid: parent+1, parent+8, parent+9 (skip parent itself)
          sibling_maps = {parent_id + 1, parent_id + 8, parent_id + 9};
          break;
        case AreaSizeEnum::WideArea:
          // 2x1 grid: parent+1 (skip parent itself)
          sibling_maps = {parent_id + 1};
          break;
        case AreaSizeEnum::TallArea:
          // 1x2 grid: parent+8 (skip parent itself)
          sibling_maps = {parent_id + 8};
          break;
        default:
          break;
      }

      // Copy properties from parent map to all siblings
      for (int sibling_index : sibling_maps) {
        if (sibling_index < 0 || sibling_index >= zelda3::kNumOverworldMaps) {
          continue;
        }
        auto& map = *ctx_.overworld->mutable_overworld_map(sibling_index);
        map.set_area_graphics(current_ow_map.area_graphics());
        map.set_area_palette(current_ow_map.area_palette());
        map.set_sprite_graphics(
            *ctx_.game_state, current_ow_map.sprite_graphics(*ctx_.game_state));
        map.set_sprite_palette(*ctx_.game_state,
                               current_ow_map.sprite_palette(*ctx_.game_state));
        map.set_message_id(current_ow_map.message_id());

        // CRITICAL FIX: Reload graphics after changing properties
        map.LoadAreaGraphics();
      }
    }
  } else {
    // Legacy logic for vanilla and v2 ROMs
    if (current_ow_map.is_large_map()) {
      // We need to copy the properties from the parent map to the children
      for (int i = 1; i < 4; i++) {
        int sibling_index = current_ow_map.parent() + i;
        if (i >= 2) {
          sibling_index += 6;
        }
        auto& map = *ctx_.overworld->mutable_overworld_map(sibling_index);
        map.set_area_graphics(current_ow_map.area_graphics());
        map.set_area_palette(current_ow_map.area_palette());
        map.set_sprite_graphics(
            *ctx_.game_state, current_ow_map.sprite_graphics(*ctx_.game_state));
        map.set_sprite_palette(*ctx_.game_state,
                               current_ow_map.sprite_palette(*ctx_.game_state));
        map.set_message_id(current_ow_map.message_id());

        // CRITICAL FIX: Reload graphics after changing properties
        map.LoadAreaGraphics();
      }
    }
  }
}

absl::Status MapRefreshCoordinator::RefreshTile16Blockset() {
  LOG_DEBUG("MapRefreshCoordinator", "RefreshTile16Blockset called");
  if (*ctx_.current_blockset ==
      ctx_.overworld->overworld_map(*ctx_.current_map)->area_graphics()) {
    return absl::OkStatus();
  }
  *ctx_.current_blockset =
      ctx_.overworld->overworld_map(*ctx_.current_map)->area_graphics();

  ctx_.overworld->set_current_map(*ctx_.current_map);
  *ctx_.palette = ctx_.overworld->current_area_palette();
  ctx_.tile16_editor->set_palette(*ctx_.palette);
  if (ctx_.current_gfx_bmp->is_active()) {
    ctx_.current_gfx_bmp->SetPalette(*ctx_.palette);
    ctx_.current_gfx_bmp->set_modified(true);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, ctx_.current_gfx_bmp);
  }

  const auto& tile16_data = ctx_.overworld->tile16_blockset_data();

  gfx::UpdateTilemap(ctx_.renderer, *ctx_.tile16_blockset, tile16_data);
  ctx_.tile16_blockset->atlas.SetPalette(*ctx_.palette);

  // Queue texture update for the atlas
  if (ctx_.tile16_blockset->atlas.texture() &&
      ctx_.tile16_blockset->atlas.is_active()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &ctx_.tile16_blockset->atlas);
  } else if (!ctx_.tile16_blockset->atlas.texture() &&
             ctx_.tile16_blockset->atlas.is_active()) {
    // Create texture if it doesn't exist yet
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &ctx_.tile16_blockset->atlas);
  }

  return absl::OkStatus();
}

void MapRefreshCoordinator::UpdateBlocksetWithPendingTileChanges() {
  // Skip if blockset not loaded or no pending changes
  if (!*ctx_.map_blockset_loaded) {
    return;
  }

  if (!ctx_.tile16_editor->has_pending_changes()) {
    return;
  }

  // Validate the atlas bitmap before modifying
  if (!ctx_.tile16_blockset->atlas.is_active() ||
      ctx_.tile16_blockset->atlas.vector().empty() ||
      ctx_.tile16_blockset->atlas.width() == 0 ||
      ctx_.tile16_blockset->atlas.height() == 0) {
    return;
  }

  // Calculate tile positions in the atlas (8 tiles per row, each 16x16)
  constexpr int kTilesPerRow = 8;
  constexpr int kTileSize = 16;
  int atlas_width = ctx_.tile16_blockset->atlas.width();
  int atlas_height = ctx_.tile16_blockset->atlas.height();

  bool atlas_modified = false;

  // Iterate through all possible tile IDs to check for modifications
  // Note: This is a brute-force approach; a more efficient method would
  // maintain a list of modified tile IDs
  for (int tile_id = 0; tile_id < zelda3::kNumTile16Individual; ++tile_id) {
    if (!ctx_.tile16_editor->is_tile_modified(tile_id)) {
      continue;
    }

    // Get the pending bitmap for this tile
    const gfx::Bitmap* pending_bmp =
        ctx_.tile16_editor->GetPendingTileBitmap(tile_id);
    if (!pending_bmp || !pending_bmp->is_active() ||
        pending_bmp->vector().empty()) {
      continue;
    }

    // Calculate position in the atlas
    int tile_x = (tile_id % kTilesPerRow) * kTileSize;
    int tile_y = (tile_id / kTilesPerRow) * kTileSize;

    // Validate tile position is within atlas bounds
    if (tile_x + kTileSize > atlas_width || tile_y + kTileSize > atlas_height) {
      continue;
    }

    // Copy pending bitmap data into the atlas at the correct position
    auto& atlas_data = ctx_.tile16_blockset->atlas.mutable_data();
    const auto& pending_data = pending_bmp->vector();

    for (int y = 0; y < kTileSize && y < pending_bmp->height(); ++y) {
      for (int x = 0; x < kTileSize && x < pending_bmp->width(); ++x) {
        int atlas_idx = (tile_y + y) * atlas_width + (tile_x + x);
        int pending_idx = y * pending_bmp->width() + x;

        if (atlas_idx >= 0 && atlas_idx < static_cast<int>(atlas_data.size()) &&
            pending_idx >= 0 &&
            pending_idx < static_cast<int>(pending_data.size())) {
          atlas_data[atlas_idx] = pending_data[pending_idx];
          atlas_modified = true;
        }
      }
    }
  }

  // Only queue texture update if we actually modified something
  if (atlas_modified && ctx_.tile16_blockset->atlas.texture()) {
    ctx_.tile16_blockset->atlas.set_modified(true);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &ctx_.tile16_blockset->atlas);
  }
}

}  // namespace yaze::editor
