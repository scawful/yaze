#ifndef YAZE_GFX_TILEMAP_H
#define YAZE_GFX_TILEMAP_H

#include <list>
#include <memory>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze {
namespace gfx {

/**
 * @brief Simple 2D coordinate pair for tilemap dimensions
 */
struct Pair {
  int x;  ///< X coordinate or width
  int y;  ///< Y coordinate or height
};

/**
 * @brief Smart tile cache with LRU eviction for efficient memory management
 *
 * Performance Optimizations:
 * - LRU eviction policy to keep frequently used tiles in memory
 * - Configurable cache size to balance memory usage and performance
 * - O(1) tile access and insertion
 * - Automatic cache management with minimal overhead
 *
 * Memory Safety:
 * - Uses unique_ptr<Bitmap> to ensure stable pointers across map rehashing
 * - Returned Bitmap* pointers remain valid until explicitly evicted or cleared
 */
struct TileCache {
  static constexpr size_t MAX_CACHE_SIZE = 1024;
  // Use unique_ptr to ensure stable Bitmap* pointers across rehashing
  std::unordered_map<int, std::unique_ptr<Bitmap>> cache_;
  std::list<int> access_order_;

  /**
   * @brief Get a cached tile by ID
   * @param tile_id Tile identifier
   * @return Pointer to cached tile bitmap or nullptr if not cached
   * @note Returned pointer is stable and won't be invalidated by subsequent
   *       CacheTile calls (unless this specific tile is evicted)
   */
  Bitmap* GetTile(int tile_id) {
    auto it = cache_.find(tile_id);
    if (it != cache_.end()) {
      // Move to front of access order (most recently used)
      access_order_.remove(tile_id);
      access_order_.push_front(tile_id);
      return it->second.get();
    }
    return nullptr;
  }

  /**
   * @brief Cache a tile bitmap by copying it
   * @param tile_id Tile identifier
   * @param bitmap Tile bitmap to cache (copied, not moved)
   * @note Uses copy semantics to ensure the original bitmap remains valid
   */
  void CacheTile(int tile_id, const Bitmap& bitmap) {
    if (cache_.size() >= MAX_CACHE_SIZE) {
      // Remove least recently used tile
      int lru_tile = access_order_.back();
      access_order_.pop_back();
      cache_.erase(lru_tile);
    }

    cache_[tile_id] = std::make_unique<Bitmap>(bitmap);  // Copy, not move
    access_order_.push_front(tile_id);
  }

  /**
   * @brief Clear the cache
   */
  void Clear() {
    cache_.clear();
    access_order_.clear();
  }

  /**
   * @brief Get cache statistics
   * @return Number of cached tiles
   */
  size_t Size() const { return cache_.size(); }
};

/**
 * @brief Tilemap structure for SNES tile-based graphics management
 *
 * The Tilemap class provides comprehensive tile management for ROM hacking:
 *
 * Key Features:
 * - Atlas bitmap containing all tiles in a single texture
 * - Smart tile cache with LRU eviction for optimal memory usage
 * - Tile metadata storage (mirroring, palette, etc.)
 * - Support for both 8x8 and 16x16 tile sizes
 * - Efficient tile lookup and rendering
 *
 * Performance Optimizations:
 * - Hash map storage for O(1) tile access
 * - LRU tile caching to minimize memory usage
 * - Atlas-based rendering to minimize draw calls
 * - Tile metadata caching for fast property access
 *
 * ROM Hacking Specific:
 * - SNES tile format support (4BPP, 8BPP)
 * - Tile mirroring and flipping support
 * - Palette index management per tile
 * - Integration with SNES graphics buffer format
 */
struct Tilemap {
  Bitmap atlas;          ///< Master bitmap containing all tiles
  TileCache tile_cache;  ///< Smart tile cache with LRU eviction
  std::vector<std::array<gfx::TileInfo, 4>>
      tile_info;   ///< Tile metadata (4 tiles per 16x16)
  Pair tile_size;  ///< Size of individual tiles (8x8 or 16x16)
  Pair map_size;   ///< Size of tilemap in tiles
};

std::vector<uint8_t> FetchTileDataFromGraphicsBuffer(
    const std::vector<uint8_t>& data, int tile_id, int sheet_offset);

Tilemap CreateTilemap(IRenderer* renderer, std::vector<uint8_t>& data,
                      int width, int height, int tile_size, int num_tiles,
                      SnesPalette& palette);

void UpdateTilemap(IRenderer* renderer, Tilemap& tilemap,
                   const std::vector<uint8_t>& data);

void RenderTile(IRenderer* renderer, Tilemap& tilemap, int tile_id);

void RenderTile16(IRenderer* renderer, Tilemap& tilemap, int tile_id);
void UpdateTile16(IRenderer* renderer, Tilemap& tilemap, int tile_id);

void ModifyTile16(Tilemap& tilemap, const std::vector<uint8_t>& data,
                  const TileInfo& top_left, const TileInfo& top_right,
                  const TileInfo& bottom_left, const TileInfo& bottom_right,
                  int sheet_offset, int tile_id);

void ComposeTile16(Tilemap& tilemap, const std::vector<uint8_t>& data,
                   const TileInfo& top_left, const TileInfo& top_right,
                   const TileInfo& bottom_left, const TileInfo& bottom_right,
                   int sheet_offset);

std::vector<uint8_t> GetTilemapData(Tilemap& tilemap, int tile_id);

/**
 * @brief Render multiple tiles using atlas rendering for improved performance
 * @param tilemap Tilemap containing tiles to render
 * @param tile_ids Vector of tile IDs to render
 * @param positions Vector of screen positions for each tile
 * @param scales Vector of scale factors for each tile (optional, defaults
 * to 1.0)
 * @note This function uses atlas rendering to reduce draw calls significantly
 */
void RenderTilesBatch(IRenderer* renderer, Tilemap& tilemap,
                      const std::vector<int>& tile_ids,
                      const std::vector<std::pair<float, float>>& positions,
                      const std::vector<std::pair<float, float>>& scales = {});

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_GFX_TILEMAP_H
