#ifndef YAZE_GFX_TILEMAP_H
#define YAZE_GFX_TILEMAP_H

#include "absl/container/flat_hash_map.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace gfx {

struct Pair {
  int x;
  int y;
};

struct Tilemap {
  Bitmap atlas;
  absl::flat_hash_map<int, Bitmap> tile_bitmaps;
  std::vector<std::array<gfx::TileInfo, 4>> tile_info;
  Pair tile_size;
  Pair map_size;
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
