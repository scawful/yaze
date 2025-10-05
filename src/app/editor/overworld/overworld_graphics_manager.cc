#include "overworld_graphics_manager.h"

#include <algorithm>

#include "app/core/features.h"
#include "app/core/window.h"
#include "app/gfx/performance_profiler.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

using core::Renderer;
using zelda3::kNumOverworldMaps;
using zelda3::kOverworldMapSize;

constexpr int kTile16Size = 16;

// ============================================================================
// Loading Operations
// ============================================================================

absl::Status OverworldGraphicsManager::LoadGraphics() {
  gfx::ScopedTimer timer("LoadGraphics");

  LOG_INFO("OverworldGraphicsManager", "Loading overworld.");
  // Load the Link to the Past overworld.
  {
    gfx::ScopedTimer load_timer("Overworld::Load");
    RETURN_IF_ERROR(overworld_->Load(rom_));
  }
  *palette_ = overworld_->current_area_palette();

  LOG_INFO("OverworldGraphicsManager", "Loading overworld graphics (optimized).");

  // Phase 1: Create bitmaps without textures for faster loading
  // This avoids blocking the main thread with GPU texture creation
  {
    gfx::ScopedTimer gfx_timer("CreateBitmapWithoutTexture_Graphics");
    Renderer::Get().CreateBitmapWithoutTexture(0x80, kOverworldMapSize, 0x40,
                                               overworld_->current_graphics(),
                                               *current_gfx_bmp_, *palette_);
  }

  LOG_INFO("OverworldGraphicsManager", "Loading overworld tileset (deferred textures).");
  {
    gfx::ScopedTimer tileset_timer("CreateBitmapWithoutTexture_Tileset");
    Renderer::Get().CreateBitmapWithoutTexture(
        0x80, 0x2000, 0x08, overworld_->tile16_blockset_data(),
        *tile16_blockset_bmp_, *palette_);
  }
  map_blockset_loaded_ = true;

  // Copy the tile16 data into individual tiles.
  auto tile16_blockset_data = overworld_->tile16_blockset_data();
  LOG_INFO("OverworldGraphicsManager", "Loading overworld tile16 graphics.");

  {
    gfx::ScopedTimer tilemap_timer("CreateTilemap");
    *tile16_blockset_ =
        gfx::CreateTilemap(tile16_blockset_data, 0x80, 0x2000, kTile16Size,
                           zelda3::kNumTile16Individual, *palette_);
  }

  // Phase 2: Create bitmaps only for essential maps initially
  // Non-essential maps will be created on-demand when accessed
  constexpr int kEssentialMapsPerWorld = 8;
  constexpr int kLightWorldEssential = kEssentialMapsPerWorld;
  constexpr int kDarkWorldEssential =
      zelda3::kDarkWorldMapIdStart + kEssentialMapsPerWorld;
  constexpr int kSpecialWorldEssential =
      zelda3::kSpecialWorldMapIdStart + kEssentialMapsPerWorld;

  LOG_INFO("OverworldGraphicsManager",
           "Creating bitmaps for essential maps only (first %d maps per world)",
           kEssentialMapsPerWorld);

  std::vector<gfx::Bitmap*> maps_to_texture;
  maps_to_texture.reserve(kEssentialMapsPerWorld *
                          3);  // 8 maps per world * 3 worlds

  {
    gfx::ScopedTimer maps_timer("CreateEssentialOverworldMaps");
    for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
      bool is_essential = false;

      // Check if this is an essential map
      if (i < kLightWorldEssential) {
        is_essential = true;
      } else if (i >= zelda3::kDarkWorldMapIdStart && i < kDarkWorldEssential) {
        is_essential = true;
      } else if (i >= zelda3::kSpecialWorldMapIdStart &&
                 i < kSpecialWorldEssential) {
        is_essential = true;
      }

      if (is_essential) {
        overworld_->set_current_map(i);
        auto palette = overworld_->current_area_palette();
        try {
          // Create bitmap data and surface but defer texture creation
          (*maps_bmp_)[i].Create(kOverworldMapSize, kOverworldMapSize, 0x80,
                                 overworld_->current_map_bitmap_data());
          (*maps_bmp_)[i].SetPalette(palette);
          maps_to_texture.push_back(&(*maps_bmp_)[i]);
        } catch (const std::bad_alloc& e) {
          LOG_ERROR("OverworldGraphicsManager", "Error allocating map %d: %s", 
                    i, e.what());
          continue;
        }
      }
      // Non-essential maps will be created on-demand when accessed
    }
  }

  // Phase 3: Create textures only for currently visible maps
  // Only create textures for the first few maps initially
  const int initial_texture_count =
      std::min(4, static_cast<int>(maps_to_texture.size()));
  {
    gfx::ScopedTimer initial_textures_timer("CreateInitialTextures");
    for (int i = 0; i < initial_texture_count; ++i) {
      Renderer::Get().RenderBitmap(maps_to_texture[i]);
    }
  }

  // Store remaining maps for lazy texture creation
  deferred_map_textures_.assign(maps_to_texture.begin() + initial_texture_count,
                                maps_to_texture.end());

  if (core::FeatureFlags::get().overworld.kDrawOverworldSprites) {
    {
      gfx::ScopedTimer sprites_timer("LoadSpriteGraphics");
      RETURN_IF_ERROR(LoadSpriteGraphics());
    }
  }

  all_gfx_loaded_ = true;
  return absl::OkStatus();
}

absl::Status OverworldGraphicsManager::LoadSpriteGraphics() {
  // Render the sprites for each Overworld map
  const int depth = 0x10;
  for (int i = 0; i < 3; i++)
    for (auto const& sprite : *overworld_->mutable_sprites(i)) {
      int width = sprite.width();
      int height = sprite.height();
      if (width == 0 || height == 0) {
        continue;
      }
      if (sprite_previews_->size() < sprite.id()) {
        sprite_previews_->resize(sprite.id() + 1);
      }
      (*sprite_previews_)[sprite.id()].Create(width, height, depth,
                                              *sprite.preview_graphics());
      (*sprite_previews_)[sprite.id()].SetPalette(*palette_);
      Renderer::Get().RenderBitmap(&(*sprite_previews_)[sprite.id()]);
    }
  return absl::OkStatus();
}

// ============================================================================
// Texture Management
// ============================================================================

void OverworldGraphicsManager::ProcessDeferredTextures() {
  std::lock_guard<std::mutex> lock(deferred_textures_mutex_);

  // Always process deferred textures progressively, even if the list is "empty"
  // This allows for continuous background loading

  // PHASE 1: Priority loading for current world
  const int high_priority_per_frame = 4;  // Current world maps
  const int low_priority_per_frame = 2;   // Other world maps
  const int refresh_per_frame = 2;        // Modified map refreshes
  int processed = 0;

  // Process high-priority deferred textures (current world)
  if (!deferred_map_textures_.empty()) {
    auto it = deferred_map_textures_.begin();
    while (it != deferred_map_textures_.end() && processed < high_priority_per_frame) {
      if (*it && !(*it)->texture()) {
        // Find map index for priority check
        int map_index = -1;
        for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
          if (&(*maps_bmp_)[i] == *it) {
            map_index = i;
            break;
          }
        }

        bool is_current_world = false;
        if (map_index >= 0) {
          int map_world = map_index / 0x40;  // 64 maps per world
          is_current_world = (map_world == current_world_);
        }

        if (is_current_world) {
          Renderer::Get().RenderBitmap(*it);
          processed++;
          it = deferred_map_textures_.erase(it);
        } else {
          ++it;
        }
      } else {
        ++it;
      }
    }
  }

  // PHASE 2: Background loading for other worlds (lower priority)
  if (!deferred_map_textures_.empty() && processed < high_priority_per_frame) {
    auto it = deferred_map_textures_.begin();
    int low_priority_processed = 0;
    while (it != deferred_map_textures_.end() && low_priority_processed < low_priority_per_frame) {
      if (*it && !(*it)->texture()) {
        Renderer::Get().RenderBitmap(*it);
        low_priority_processed++;
        processed++;
        it = deferred_map_textures_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // PHASE 3: Process modified maps that need refresh (highest priority)
  int refresh_processed = 0;
  for (int i = 0; i < zelda3::kNumOverworldMaps && refresh_processed < refresh_per_frame; ++i) {
    if ((*maps_bmp_)[i].modified() && (*maps_bmp_)[i].is_active()) {
      // Check if this map is in current world (high priority) or visible
      bool is_current_world = (i / 0x40 == current_world_);
      bool is_current_map = (i == current_map_);
      
      if (is_current_map || is_current_world) {
        RefreshOverworldMapOnDemand(i);
        refresh_processed++;
      }
    }
  }

  // PHASE 4: Background refresh for modified maps in other worlds (very low priority)
  if (refresh_processed == 0) {
    for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
      if ((*maps_bmp_)[i].modified() && (*maps_bmp_)[i].is_active()) {
        bool is_current_world = (i / 0x40 == current_world_);
        if (!is_current_world) {
          // Just mark for later, don't refresh now to avoid lag
          // These will be refreshed when the world is switched
          break;
        }
      }
    }
  }
}

void OverworldGraphicsManager::EnsureMapTexture(int map_index) {
  if (map_index < 0 || map_index >= zelda3::kNumOverworldMaps) {
    return;
  }

  // Ensure the map is built first (on-demand loading)
  auto status = overworld_->EnsureMapBuilt(map_index);
  if (!status.ok()) {
    LOG_ERROR("OverworldGraphicsManager", "Failed to build map %d: %s", map_index,
              status.message().data());
    return;
  }

  auto& bitmap = (*maps_bmp_)[map_index];

  // If bitmap doesn't exist yet (non-essential map), create it now
  if (!bitmap.is_active()) {
    overworld_->set_current_map(map_index);
    auto palette = overworld_->current_area_palette();
    try {
      bitmap.Create(kOverworldMapSize, kOverworldMapSize, 0x80,
                    overworld_->current_map_bitmap_data());
      bitmap.SetPalette(palette);
    } catch (const std::bad_alloc& e) {
      LOG_ERROR("OverworldGraphicsManager", "Error allocating bitmap for map %d: %s",
                map_index, e.what());
      return;
    }
  }

  if (!bitmap.texture() && bitmap.is_active()) {
    Renderer::Get().RenderBitmap(&bitmap);

    // Remove from deferred list if it was there
    std::lock_guard<std::mutex> lock(deferred_textures_mutex_);
    auto it = std::find(deferred_map_textures_.begin(),
                        deferred_map_textures_.end(), &bitmap);
    if (it != deferred_map_textures_.end()) {
      deferred_map_textures_.erase(it);
    }
  }
}

// ============================================================================
// Refresh Operations
// ============================================================================

void OverworldGraphicsManager::RefreshOverworldMap() {
  // Use the new on-demand refresh system
  RefreshOverworldMapOnDemand(current_map_);
}

void OverworldGraphicsManager::RefreshOverworldMapOnDemand(int map_index) {
  if (map_index < 0 || map_index >= zelda3::kNumOverworldMaps) {
    return;
  }

  // Check if the map is actually visible or being edited
  bool is_current_map = (map_index == current_map_);
  bool is_current_world = (map_index / 0x40 == current_world_);

  // For non-current maps in non-current worlds, defer the refresh
  if (!is_current_map && !is_current_world) {
    // Mark for deferred refresh - will be processed when the map becomes visible
    (*maps_bmp_)[map_index].set_modified(true);
    return;
  }

  // For visible maps, do immediate refresh
  RefreshChildMapOnDemand(map_index);
}

void OverworldGraphicsManager::RefreshChildMap(int map_index) {
  overworld_->mutable_overworld_map(map_index)->LoadAreaGraphics();
  auto status = overworld_->mutable_overworld_map(map_index)->BuildTileset();
  PRINT_IF_ERROR(status);
  status = overworld_->mutable_overworld_map(map_index)->BuildTiles16Gfx(
      *overworld_->mutable_tiles16(), overworld_->tiles16().size());
  PRINT_IF_ERROR(status);
  status = overworld_->mutable_overworld_map(map_index)->BuildBitmap(
      overworld_->GetMapTiles(current_world_));
  (*maps_bmp_)[map_index].set_data(
      overworld_->mutable_overworld_map(map_index)->bitmap_data());
  (*maps_bmp_)[map_index].set_modified(true);
  PRINT_IF_ERROR(status);
}

void OverworldGraphicsManager::RefreshChildMapOnDemand(int map_index) {
  auto* map = overworld_->mutable_overworld_map(map_index);

  // Check what actually needs to be refreshed
  bool needs_graphics_rebuild = (*maps_bmp_)[map_index].modified();

  if (needs_graphics_rebuild) {
    // Only rebuild what's actually changed
    map->LoadAreaGraphics();

    // Rebuild tileset only if graphics changed
    auto status = map->BuildTileset();
    if (!status.ok()) {
      LOG_ERROR("OverworldGraphicsManager", "Failed to build tileset for map %d: %s",
                map_index, status.message().data());
      return;
    }

    // Rebuild tiles16 graphics
    status = map->BuildTiles16Gfx(*overworld_->mutable_tiles16(),
                                  overworld_->tiles16().size());
    if (!status.ok()) {
      LOG_ERROR("OverworldGraphicsManager", "Failed to build tiles16 graphics for map %d: %s",
                map_index, status.message().data());
      return;
    }

    // Rebuild bitmap
    status = map->BuildBitmap(overworld_->GetMapTiles(current_world_));
    if (!status.ok()) {
      LOG_ERROR("OverworldGraphicsManager", "Failed to build bitmap for map %d: %s",
                map_index, status.message().data());
      return;
    }

    // Update bitmap data
    (*maps_bmp_)[map_index].set_data(map->bitmap_data());
    (*maps_bmp_)[map_index].set_modified(false);

    // Validate surface synchronization to help debug crashes
    if (!(*maps_bmp_)[map_index].ValidateDataSurfaceSync()) {
      LOG_WARN("OverworldGraphicsManager", "Warning: Surface synchronization issue detected for map %d",
               map_index);
    }

    // Update texture on main thread
    if ((*maps_bmp_)[map_index].texture()) {
      Renderer::Get().UpdateBitmap(&(*maps_bmp_)[map_index]);
    } else {
      // Create texture if it doesn't exist
      EnsureMapTexture(map_index);
    }
  }

  // Handle multi-area maps (large, wide, tall) with safe coordination
  // Check if ZSCustomOverworld v3 is present
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  bool use_v3_area_sizes = (asm_version >= 3 && asm_version != 0xFF);

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

void OverworldGraphicsManager::RefreshMultiAreaMapsSafely(
    int map_index, zelda3::OverworldMap* map) {
  using zelda3::AreaSizeEnum;

  // Skip if this is already a processed sibling to avoid double-processing
  static std::set<int> currently_processing;
  if (currently_processing.count(map_index)) {
    return;
  }

  auto area_size = map->area_size();
  if (area_size == AreaSizeEnum::SmallArea) {
    return;  // No siblings to coordinate
  }

  LOG_DEBUG("OverworldGraphicsManager",
            "RefreshMultiAreaMapsSafely: Processing %s area map %d (parent: %d)",
            (area_size == AreaSizeEnum::LargeArea)  ? "large"
            : (area_size == AreaSizeEnum::WideArea) ? "wide"
                                                    : "tall",
            map_index, map->parent());

  // Determine all maps that are part of this multi-area structure
  std::vector<int> sibling_maps;
  int parent_id = map->parent();

  // Use the same logic as ZScream for area coordination
  switch (area_size) {
    case AreaSizeEnum::LargeArea: {
      // Large Area: 2x2 grid (4 maps total)
      sibling_maps = {parent_id, parent_id + 1, parent_id + 8, parent_id + 9};
      LOG_DEBUG("OverworldGraphicsManager",
                "RefreshMultiAreaMapsSafely: Large area siblings: %d, %d, %d, %d",
                parent_id, parent_id + 1, parent_id + 8, parent_id + 9);
      break;
    }

    case AreaSizeEnum::WideArea: {
      // Wide Area: 2x1 grid (2 maps total, horizontally adjacent)
      sibling_maps = {parent_id, parent_id + 1};
      LOG_DEBUG("OverworldGraphicsManager",
                "RefreshMultiAreaMapsSafely: Wide area siblings: %d, %d",
                parent_id, parent_id + 1);
      break;
    }

    case AreaSizeEnum::TallArea: {
      // Tall Area: 1x2 grid (2 maps total, vertically adjacent)
      sibling_maps = {parent_id, parent_id + 8};
      LOG_DEBUG("OverworldGraphicsManager",
                "RefreshMultiAreaMapsSafely: Tall area siblings: %d, %d",
                parent_id, parent_id + 8);
      break;
    }

    default:
      LOG_WARN("OverworldGraphicsManager",
               "RefreshMultiAreaMapsSafely: Unknown area size %d for map %d",
               static_cast<int>(area_size), map_index);
      return;
  }

  // Mark all siblings as being processed to prevent recursion
  for (int sibling : sibling_maps) {
    currently_processing.insert(sibling);
  }

  // Only refresh siblings that are visible/current and need updating
  for (int sibling : sibling_maps) {
    if (sibling == map_index) {
      continue;  // Skip self (already processed above)
    }

    // Bounds check
    if (sibling < 0 || sibling >= zelda3::kNumOverworldMaps) {
      continue;
    }

    // Only refresh if it's visible or current
    bool is_current_map = (sibling == current_map_);
    bool is_current_world = (sibling / 0x40 == current_world_);
    bool needs_refresh = (*maps_bmp_)[sibling].modified();

    if ((is_current_map || is_current_world) && needs_refresh) {
      LOG_DEBUG("OverworldGraphicsManager",
                "RefreshMultiAreaMapsSafely: Refreshing %s area sibling map %d "
                "(parent: %d)",
                (area_size == AreaSizeEnum::LargeArea)  ? "large"
                : (area_size == AreaSizeEnum::WideArea) ? "wide"
                                                        : "tall",
                sibling, parent_id);

      // Direct refresh without calling RefreshChildMapOnDemand to avoid recursion
      auto* sibling_map = overworld_->mutable_overworld_map(sibling);
      if (sibling_map && (*maps_bmp_)[sibling].modified()) {
        sibling_map->LoadAreaGraphics();

        if (auto status = sibling_map->BuildTileset(); !status.ok()) {
          LOG_ERROR("OverworldGraphicsManager",
                    "RefreshMultiAreaMapsSafely: Failed to refresh sibling map %d: %s",
                    sibling, status.message().data());
          continue;
        }

        if (auto status = sibling_map->BuildTiles16Gfx(*overworld_->mutable_tiles16(),
                                                       overworld_->tiles16().size()); !status.ok()) {
          LOG_ERROR("OverworldGraphicsManager",
                    "RefreshMultiAreaMapsSafely: Failed to build tiles16 graphics for sibling map %d: %s",
                    sibling, status.message().data());
          continue;
        }

        if (auto status = sibling_map->LoadPalette(); !status.ok()) {
          LOG_ERROR("OverworldGraphicsManager",
                    "RefreshMultiAreaMapsSafely: Failed to load palette for sibling map %d: %s",
                    sibling, status.message().data());
          continue;
        }

        if (auto status = sibling_map->BuildBitmap(overworld_->GetMapTiles(current_world_)); status.ok()) {
          (*maps_bmp_)[sibling].set_data(sibling_map->bitmap_data());
          (*maps_bmp_)[sibling].SetPalette(overworld_->current_area_palette());
          (*maps_bmp_)[sibling].set_modified(false);

          // Update texture if it exists
          if ((*maps_bmp_)[sibling].texture()) {
            Renderer::Get().UpdateBitmap(&(*maps_bmp_)[sibling]);
          } else {
            EnsureMapTexture(sibling);
          }
        } else {
          LOG_ERROR("OverworldGraphicsManager",
                    "RefreshMultiAreaMapsSafely: Failed to build bitmap for sibling map %d: %s",
                    sibling, status.message().data());
        }
      }
    } else if (!is_current_map && !is_current_world) {
      // Mark non-visible siblings for deferred refresh
      (*maps_bmp_)[sibling].set_modified(true);
    }
  }

  // Clear processing set after completion
  for (int sibling : sibling_maps) {
    currently_processing.erase(sibling);
  }
}

absl::Status OverworldGraphicsManager::RefreshMapPalette() {
  RETURN_IF_ERROR(
      overworld_->mutable_overworld_map(current_map_)->LoadPalette());
  const auto current_map_palette = overworld_->current_area_palette();

  // Check if ZSCustomOverworld v3 is present
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  bool use_v3_area_sizes = (asm_version >= 3 && asm_version != 0xFF);

  if (use_v3_area_sizes) {
    // Use v3 area size system
    using zelda3::AreaSizeEnum;
    auto area_size = overworld_->overworld_map(current_map_)->area_size();

    if (area_size != AreaSizeEnum::SmallArea) {
      // Get all sibling maps that need palette updates
      std::vector<int> sibling_maps;
      int parent_id = overworld_->overworld_map(current_map_)->parent();

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

      // Update palette for all siblings
      for (int sibling_index : sibling_maps) {
        if (sibling_index < 0 || sibling_index >= zelda3::kNumOverworldMaps) {
          continue;
        }
        RETURN_IF_ERROR(
            overworld_->mutable_overworld_map(sibling_index)->LoadPalette());
        (*maps_bmp_)[sibling_index].SetPalette(current_map_palette);
      }
    } else {
      // Small area - only update current map
      (*maps_bmp_)[current_map_].SetPalette(current_map_palette);
    }
  } else {
    // Legacy logic for vanilla and v2 ROMs
    if (overworld_->overworld_map(current_map_)->is_large_map()) {
      // We need to update the map and its siblings if it's a large map
      for (int i = 1; i < 4; i++) {
        int sibling_index =
            overworld_->overworld_map(current_map_)->parent() + i;
        if (i >= 2)
          sibling_index += 6;
        RETURN_IF_ERROR(
            overworld_->mutable_overworld_map(sibling_index)->LoadPalette());
        (*maps_bmp_)[sibling_index].SetPalette(current_map_palette);
      }
    }
    (*maps_bmp_)[current_map_].SetPalette(current_map_palette);
  }

  return absl::OkStatus();
}

absl::Status OverworldGraphicsManager::RefreshTile16Blockset() {
  LOG_DEBUG("OverworldGraphicsManager", "RefreshTile16Blockset called");
  if (current_blockset_ ==
      overworld_->overworld_map(current_map_)->area_graphics()) {
    return absl::OkStatus();
  }
  current_blockset_ = overworld_->overworld_map(current_map_)->area_graphics();

  overworld_->set_current_map(current_map_);
  *palette_ = overworld_->current_area_palette();

  const auto tile16_data = overworld_->tile16_blockset_data();

  gfx::UpdateTilemap(*tile16_blockset_, tile16_data);
  tile16_blockset_->atlas.SetPalette(*palette_);
  return absl::OkStatus();
}

void OverworldGraphicsManager::ForceRefreshGraphics(int map_index) {
  if (map_index < 0 || map_index >= zelda3::kNumOverworldMaps) {
    return;
  }
  
  LOG_INFO("OverworldGraphicsManager", 
           "ForceRefreshGraphics: Forcing graphics reload for map %d", map_index);
  
  // Mark bitmap as modified to force refresh
  (*maps_bmp_)[map_index].set_modified(true);
  
  // Clear the blockset cache to force tile16 reload
  current_blockset_ = 0xFF;
  
  // If this is the current map, also ensure sibling maps are refreshed for multi-area maps
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  bool use_v3_area_sizes = (asm_version >= 3 && asm_version != 0xFF);
  
  auto* map = overworld_->mutable_overworld_map(map_index);
  if (use_v3_area_sizes) {
    using zelda3::AreaSizeEnum;
    auto area_size = map->area_size();
    
    if (area_size != AreaSizeEnum::SmallArea) {
      std::vector<int> sibling_maps;
      int parent_id = map->parent();
      
      switch (area_size) {
        case AreaSizeEnum::LargeArea:
          sibling_maps = {parent_id, parent_id + 1, parent_id + 8, parent_id + 9};
          break;
        case AreaSizeEnum::WideArea:
          sibling_maps = {parent_id, parent_id + 1};
          break;
        case AreaSizeEnum::TallArea:
          sibling_maps = {parent_id, parent_id + 8};
          break;
        default:
          break;
      }
      
      // Mark all sibling maps as needing refresh
      for (int sibling : sibling_maps) {
        if (sibling >= 0 && sibling < zelda3::kNumOverworldMaps) {
          (*maps_bmp_)[sibling].set_modified(true);
        }
      }
    }
  } else if (map->is_large_map()) {
    // Legacy large map handling
    int parent_id = map->parent();
    for (int i = 0; i < 4; ++i) {
      int sibling = parent_id + (i < 2 ? i : i + 6);
      if (sibling >= 0 && sibling < zelda3::kNumOverworldMaps) {
        (*maps_bmp_)[sibling].set_modified(true);
      }
    }
  }
}

}  // namespace editor
}  // namespace yaze

