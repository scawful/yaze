#ifndef YAZE_APP_EDITOR_OVERWORLD_MAP_REFRESH_COORDINATOR_H
#define YAZE_APP_EDITOR_OVERWORLD_MAP_REFRESH_COORDINATOR_H

#include <array>
#include <functional>

#include "absl/status/status.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/types/snes_palette.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {

class Rom;

namespace gfx {
class IRenderer;
}  // namespace gfx

namespace editor {

class Tile16Editor;

// =============================================================================
// MapRefreshCoordinator
// =============================================================================
//
// Extracted from OverworldEditor to encapsulate the Map Refresh System.
// Handles refreshing map graphics, palettes, tilesets, and multi-area
// coordination after property changes.
//
// This class does not own any of the data it operates on. It holds
// pointers/references to data owned by OverworldEditor.
// =============================================================================

/// @brief Context struct holding all data dependencies for map refresh
/// operations. All pointers/references must remain valid for the lifetime
/// of the MapRefreshCoordinator.
struct MapRefreshContext {
  Rom* rom = nullptr;
  zelda3::Overworld* overworld = nullptr;
  std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>* maps_bmp = nullptr;
  gfx::Tilemap* tile16_blockset = nullptr;
  gfx::Bitmap* current_gfx_bmp = nullptr;
  gfx::BitmapTable* current_graphics_set = nullptr;
  gfx::SnesPalette* palette = nullptr;
  gfx::IRenderer* renderer = nullptr;
  Tile16Editor* tile16_editor = nullptr;

  // Mutable state references (owned by OverworldEditor)
  int* current_world = nullptr;
  int* current_map = nullptr;
  int* current_blockset = nullptr;
  int* game_state = nullptr;
  bool* map_blockset_loaded = nullptr;
  absl::Status* status = nullptr;

  /// @brief Callback to ensure a map texture is created (stays in
  /// OverworldEditor)
  std::function<void(int map_index)> ensure_map_texture;
};

class MapRefreshCoordinator {
 public:
  explicit MapRefreshCoordinator(const MapRefreshContext& ctx) : ctx_(ctx) {}

  // ===========================================================================
  // Map Refresh Methods
  // ===========================================================================

  /// @brief Invalidate cached graphics for a specific map or all maps
  /// @param map_id The map to invalidate (-1 to invalidate all maps)
  void InvalidateGraphicsCache(int map_id = -1);

  /// @brief Refresh a child map's graphics pipeline (legacy full rebuild)
  void RefreshChildMap(int map_index);

  /// @brief Refresh the current overworld map
  void RefreshOverworldMap();

  /// @brief On-demand map refresh that only updates what's actually needed
  void RefreshOverworldMapOnDemand(int map_index);

  /// @brief On-demand child map refresh with selective updates
  void RefreshChildMapOnDemand(int map_index);

  /// @brief Safely refresh multi-area maps without recursion
  void RefreshMultiAreaMapsSafely(int map_index, zelda3::OverworldMap* map);

  /// @brief Refresh map palette after palette property changes
  absl::Status RefreshMapPalette();

  /// @brief Force refresh graphics for a specific map
  void ForceRefreshGraphics(int map_index);

  /// @brief Refresh sibling map graphics for multi-area maps
  void RefreshSiblingMapGraphics(int map_index, bool include_self = false);

  /// @brief Refresh map properties (copy parent properties to siblings)
  void RefreshMapProperties();

  /// @brief Refresh the tile16 blockset after graphics/palette changes
  absl::Status RefreshTile16Blockset();

  /// @brief Update blockset atlas with pending tile16 editor changes
  void UpdateBlocksetWithPendingTileChanges();

 private:
  MapRefreshContext ctx_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_MAP_REFRESH_COORDINATOR_H
