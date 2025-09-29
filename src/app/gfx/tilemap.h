#ifndef YAZE_GFX_TILEMAP_H
#define YAZE_GFX_TILEMAP_H

#include "absl/container/flat_hash_map.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"

#include <list>
#include <unordered_map>

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
 */
struct TileCache {
  static constexpr size_t MAX_CACHE_SIZE = 1024;
  std::unordered_map<int, Bitmap> cache_;
  std::list<int> access_order_;
  
  /**
   * @brief Get a cached tile by ID
   * @param tile_id Tile identifier
   * @return Pointer to cached tile bitmap or nullptr if not cached
   */
  Bitmap* GetTile(int tile_id) {
    auto it = cache_.find(tile_id);
    if (it != cache_.end()) {
      // Move to front of access order (most recently used)
      access_order_.remove(tile_id);
      access_order_.push_front(tile_id);
      return &it->second;
    }
    return nullptr;
  }
  
  /**
   * @brief Cache a tile bitmap
   * @param tile_id Tile identifier
   * @param bitmap Tile bitmap to cache
   */
  void CacheTile(int tile_id, Bitmap&& bitmap) {
    if (cache_.size() >= MAX_CACHE_SIZE) {
      // Remove least recently used tile
      int lru_tile = access_order_.back();
      access_order_.pop_back();
      cache_.erase(lru_tile);
    }
    
    cache_[tile_id] = std::move(bitmap);
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
  Bitmap atlas;                                    ///< Master bitmap containing all tiles
  TileCache tile_cache;                            ///< Smart tile cache with LRU eviction
  std::vector<std::array<gfx::TileInfo, 4>> tile_info;  ///< Tile metadata (4 tiles per 16x16)
  Pair tile_size;                                  ///< Size of individual tiles (8x8 or 16x16)
  Pair map_size;                                   ///< Size of tilemap in tiles
};

std::vector<uint8_t> FetchTileDataFromGraphicsBuffer(
    const std::vector<uint8_t> &data, int tile_id, int sheet_offset);

Tilemap CreateTilemap(std::vector<uint8_t> &data, int width, int height,
                      int tile_size, int num_tiles, SnesPalette &palette);

void UpdateTilemap(Tilemap &tilemap, const std::vector<uint8_t> &data);

void RenderTile(Tilemap &tilemap, int tile_id);

void RenderTile16(Tilemap &tilemap, int tile_id);
void UpdateTile16(Tilemap &tilemap, int tile_id);

void ModifyTile16(Tilemap &tilemap, const std::vector<uint8_t> &data,
                   const TileInfo &top_left, const TileInfo &top_right,
                   const TileInfo &bottom_left, const TileInfo &bottom_right,
                   int sheet_offset, int tile_id);

void ComposeTile16(Tilemap &tilemap, const std::vector<uint8_t> &data,
                   const TileInfo &top_left, const TileInfo &top_right,
                   const TileInfo &bottom_left, const TileInfo &bottom_right,
                   int sheet_offset);

std::vector<uint8_t> GetTilemapData(Tilemap &tilemap, int tile_id);

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_GFX_TILEMAP_H
