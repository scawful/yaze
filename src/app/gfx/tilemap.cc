#include "app/gfx/tilemap.h"

#include <vector>

#include "app/core/platform/renderer.h"
#include "app/gfx/bitmap.h"

namespace yaze {
namespace gfx {

Tilemap CreateTilemap(std::vector<uint8_t> &data, int width, int height,
                      int tile_size, int num_tiles, SnesPalette &palette) {
  Tilemap tilemap;
  tilemap.tile_size.x = tile_size;
  tilemap.tile_size.y = tile_size;
  tilemap.map_size.x = num_tiles;
  tilemap.map_size.y = num_tiles;
  tilemap.atlas = Bitmap(width, height, 8, data);
  tilemap.atlas.SetPalette(palette);
  core::Renderer::GetInstance().RenderBitmap(&tilemap.atlas);
  return tilemap;
}

void UpdateTilemap(Tilemap &tilemap, const std::vector<uint8_t> &data) {
  tilemap.atlas.set_data(data);
  core::Renderer::GetInstance().UpdateBitmap(&tilemap.atlas);
}

void RenderTile(Tilemap &tilemap, int tile_id) {
  if (tilemap.tile_bitmaps.find(tile_id) == tilemap.tile_bitmaps.end()) {
    tilemap.tile_bitmaps[tile_id] =
        Bitmap(tilemap.tile_size.x, tilemap.tile_size.y, 8,
               GetTilemapData(tilemap, tile_id));
    auto bitmap_ptr = &tilemap.tile_bitmaps[tile_id];
    core::Renderer::GetInstance().RenderBitmap(bitmap_ptr);
  }
}

std::vector<uint8_t> GetTilemapData(Tilemap &tilemap, int tile_id) {
  int tile_size = tilemap.tile_size.x;
  std::vector<uint8_t> data(tile_size * tile_size);

  int num_tiles = tilemap.map_size.x;
  int index = tile_id * tile_size * tile_size;
  int width = tilemap.atlas.width();

  for (int ty = 0; ty < tile_size; ty++) {
    for (int tx = 0; tx < tile_size; tx++) {
      uint8_t value =
          tilemap.atlas
              .vector()[(tile_id % 8 * tile_size) +
                        (tile_id / 8 * tile_size * width) + ty * width + tx];
      data[ty * tile_size + tx] = value;
    }
  }

  return data;
}

}  // namespace gfx
}  // namespace yaze
