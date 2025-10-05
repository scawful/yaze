#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_GRAPHICS_MANAGER_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_GRAPHICS_MANAGER_H

#include <array>
#include <mutex>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/tilemap.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "app/zelda3/overworld/overworld_map.h"

namespace yaze {
namespace editor {

using Tilemap = gfx::Tilemap;

/**
 * @class OverworldGraphicsManager
 * @brief Manages all graphics-related operations for the Overworld Editor
 *
 * This class handles:
 * - Graphics loading and initialization
 * - Sprite graphics loading
 * - Deferred texture processing for smooth loading
 * - Map texture management
 * - Map refreshing (full and on-demand)
 * - Palette refreshing
 * - Tile16 blockset refreshing
 * - Multi-area map coordination
 *
 * Separating graphics management from the main OverworldEditor improves:
 * - Code organization and maintainability
 * - Performance optimization opportunities
 * - Testing and debugging
 * - Clear separation of concerns
 */
class OverworldGraphicsManager {
 public:
  OverworldGraphicsManager(
      zelda3::Overworld* overworld, Rom* rom,
      std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>* maps_bmp,
      gfx::Bitmap* tile16_blockset_bmp, gfx::Bitmap* current_gfx_bmp,
      std::vector<gfx::Bitmap>* sprite_previews, gfx::SnesPalette* palette,
      Tilemap* tile16_blockset)
      : overworld_(overworld),
        rom_(rom),
        maps_bmp_(maps_bmp),
        tile16_blockset_bmp_(tile16_blockset_bmp),
        current_gfx_bmp_(current_gfx_bmp),
        sprite_previews_(sprite_previews),
        palette_(palette),
        tile16_blockset_(tile16_blockset) {}

  // ============================================================================
  // Loading Operations
  // ============================================================================

  /**
   * @brief Load all overworld graphics (maps, tilesets, sprites)
   * 
   * This uses a multi-phase loading strategy:
   * - Phase 1: Create bitmaps without textures
   * - Phase 2: Create bitmaps for essential maps only
   * - Phase 3: Create textures for visible maps
   * - Deferred loading for remaining maps
   */
  absl::Status LoadGraphics();

  /**
   * @brief Load sprite graphics for all overworld maps
   */
  absl::Status LoadSpriteGraphics();

  // ============================================================================
  // Texture Management
  // ============================================================================

  /**
   * @brief Process deferred texture creation (called per frame)
   * 
   * Creates textures gradually to avoid frame drops.
   * Prioritizes textures for the current world and visible maps.
   */
  void ProcessDeferredTextures();

  /**
   * @brief Ensure a specific map has a texture created
   * 
   * @param map_index Index of the map to ensure texture for
   */
  void EnsureMapTexture(int map_index);

  // ============================================================================
  // Refresh Operations
  // ============================================================================

  /**
   * @brief Refresh the current overworld map
   */
  void RefreshOverworldMap();

  /**
   * @brief Refresh a specific map on-demand (only if visible)
   * 
   * @param map_index Index of the map to refresh
   */
  void RefreshOverworldMapOnDemand(int map_index);

  /**
   * @brief Refresh a child map (legacy method)
   * 
   * @param map_index Index of the map to refresh
   */
  void RefreshChildMap(int map_index);

  /**
   * @brief Refresh a child map with selective updates
   * 
   * @param map_index Index of the map to refresh
   */
  void RefreshChildMapOnDemand(int map_index);

  /**
   * @brief Safely refresh multi-area maps (large, wide, tall)
   * 
   * Handles coordination of multi-area maps without recursion.
   * 
   * @param map_index Index of the map to refresh
   * @param map Pointer to the OverworldMap object
   */
  void RefreshMultiAreaMapsSafely(int map_index, zelda3::OverworldMap* map);

  /**
   * @brief Refresh the palette for the current map
   * 
   * Also handles palette updates for multi-area maps.
   */
  absl::Status RefreshMapPalette();

  /**
   * @brief Refresh the tile16 blockset
   * 
   * This should be called whenever area graphics change.
   */
  absl::Status RefreshTile16Blockset();

  /**
   * @brief Force a graphics refresh for a specific map
   * 
   * Marks the map's bitmap as modified and clears the blockset cache
   * to force a full reload on next refresh. Use this when graphics
   * properties change (area_graphics, animated_gfx, custom tilesets).
   * 
   * @param map_index Index of the map to force refresh
   */
  void ForceRefreshGraphics(int map_index);

  // ============================================================================
  // State Management
  // ============================================================================

  void set_current_map(int map_index) { current_map_ = map_index; }
  void set_current_world(int world_index) { current_world_ = world_index; }
  void set_current_blockset(uint8_t blockset) { current_blockset_ = blockset; }
  
  int current_map() const { return current_map_; }
  int current_world() const { return current_world_; }
  uint8_t current_blockset() const { return current_blockset_; }
  
  bool all_gfx_loaded() const { return all_gfx_loaded_; }
  bool map_blockset_loaded() const { return map_blockset_loaded_; }

 private:
  // Core dependencies
  zelda3::Overworld* overworld_;
  Rom* rom_;
  std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>* maps_bmp_;
  gfx::Bitmap* tile16_blockset_bmp_;
  gfx::Bitmap* current_gfx_bmp_;
  std::vector<gfx::Bitmap>* sprite_previews_;
  gfx::SnesPalette* palette_;
  Tilemap* tile16_blockset_;

  // State tracking
  int current_map_ = 0;
  int current_world_ = 0;
  uint8_t current_blockset_ = 0;
  bool all_gfx_loaded_ = false;
  bool map_blockset_loaded_ = false;

  // Deferred texture loading
  std::vector<gfx::Bitmap*> deferred_map_textures_;
  std::mutex deferred_textures_mutex_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_GRAPHICS_MANAGER_H

