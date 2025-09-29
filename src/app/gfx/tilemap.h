#ifndef YAZE_GFX_TILEMAP_H
#define YAZE_GFX_TILEMAP_H

#include "absl/container/flat_hash_map.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"

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
 * @brief Tilemap structure for SNES tile-based graphics management
 * 
 * The Tilemap class provides comprehensive tile management for ROM hacking:
 * 
 * Key Features:
 * - Atlas bitmap containing all tiles in a single texture
 * - Individual tile bitmap cache for fast access
 * - Tile metadata storage (mirroring, palette, etc.)
 * - Support for both 8x8 and 16x16 tile sizes
 * - Efficient tile lookup and rendering
 * 
 * Performance Optimizations:
 * - Hash map storage for O(1) tile access
 * - Lazy tile bitmap creation (only when needed)
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
  absl::flat_hash_map<int, Bitmap> tile_bitmaps;  ///< Individual tile cache
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
