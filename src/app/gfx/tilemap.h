#ifndef YAZE_GFX_TILEMAP_H
#define YAZE_GFX_TILEMAP_H

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "absl/container/flat_hash_map.h"

namespace yaze {
namespace gfx {

struct Pair {
  int x;
  int y;
};

struct Tilemap {
  Bitmap atlas;
  absl::flat_hash_map<int, Bitmap> tile_bitmaps;
  Pair tile_size;
  Pair map_size;
};

Tilemap CreateTilemap(std::vector<uint8_t> &data, int width, int height,
                      int tile_size, int num_tiles, SnesPalette &palette);

void UpdateTilemap(Tilemap &tilemap, const std::vector<uint8_t> &data);

void RenderTile(Tilemap &tilemap, int tile_id);

void ComposeTile16(Tilemap &tilemap, const std::vector<uint8_t> &data,
                   const TileInfo &top_left, const TileInfo &top_right,
                   const TileInfo &bottom_left, const TileInfo &bottom_right,
                   int sheet_offset);

std::vector<uint8_t> GetTilemapData(Tilemap &tilemap, int tile_id);

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_GFX_TILEMAP_H
