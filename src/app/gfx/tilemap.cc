#include "app/gfx/tilemap.h"

#include <vector>

#include "app/core/window.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/performance_profiler.h"
#include "app/gfx/snes_tile.h"

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
  core::Renderer::Get().RenderBitmap(&tilemap.atlas);
  return tilemap;
}

void UpdateTilemap(Tilemap &tilemap, const std::vector<uint8_t> &data) {
  tilemap.atlas.set_data(data);
  core::Renderer::Get().UpdateBitmap(&tilemap.atlas);
}

void RenderTile(Tilemap &tilemap, int tile_id) {
  ScopedTimer timer("tile_cache_operation");
  
  // Try to get tile from cache first
  Bitmap* cached_tile = tilemap.tile_cache.GetTile(tile_id);
  if (cached_tile) {
    core::Renderer::Get().UpdateBitmap(cached_tile);
    return;
  }
  
  // Create new tile and cache it
  Bitmap new_tile = Bitmap(tilemap.tile_size.x, tilemap.tile_size.y, 8,
                          GetTilemapData(tilemap, tile_id), tilemap.atlas.palette());
  tilemap.tile_cache.CacheTile(tile_id, std::move(new_tile));
  
  // Get the cached tile and render it
  Bitmap* tile_to_render = tilemap.tile_cache.GetTile(tile_id);
  if (tile_to_render) {
    core::Renderer::Get().RenderBitmap(tile_to_render);
  }
}

void RenderTile16(Tilemap &tilemap, int tile_id) {
  // Try to get tile from cache first
  Bitmap* cached_tile = tilemap.tile_cache.GetTile(tile_id);
  if (cached_tile) {
    core::Renderer::Get().UpdateBitmap(cached_tile);
    return;
  }
  
  // Create new 16x16 tile and cache it
  int tiles_per_row = tilemap.atlas.width() / tilemap.tile_size.x;
  int tile_x = (tile_id % tiles_per_row) * tilemap.tile_size.x;
  int tile_y = (tile_id / tiles_per_row) * tilemap.tile_size.y;
  std::vector<uint8_t> tile_data(tilemap.tile_size.x * tilemap.tile_size.y, 0x00);
  int tile_data_offset = 0;
  tilemap.atlas.Get16x16Tile(tile_x, tile_y, tile_data, tile_data_offset);
  
  Bitmap new_tile = Bitmap(tilemap.tile_size.x, tilemap.tile_size.y, 8, tile_data,
                          tilemap.atlas.palette());
  tilemap.tile_cache.CacheTile(tile_id, std::move(new_tile));
  
  // Get the cached tile and render it
  Bitmap* tile_to_render = tilemap.tile_cache.GetTile(tile_id);
  if (tile_to_render) {
    core::Renderer::Get().RenderBitmap(tile_to_render);
  }
}

void UpdateTile16(Tilemap &tilemap, int tile_id) {
  // Check if tile is cached
  Bitmap* cached_tile = tilemap.tile_cache.GetTile(tile_id);
  if (cached_tile) {
    // Update cached tile data
    int tiles_per_row = tilemap.atlas.width() / tilemap.tile_size.x;
    int tile_x = (tile_id % tiles_per_row) * tilemap.tile_size.x;
    int tile_y = (tile_id / tiles_per_row) * tilemap.tile_size.y;
    std::vector<uint8_t> tile_data(tilemap.tile_size.x * tilemap.tile_size.y, 0x00);
    int tile_data_offset = 0;
    tilemap.atlas.Get16x16Tile(tile_x, tile_y, tile_data, tile_data_offset);
    cached_tile->set_data(tile_data);
    core::Renderer::Get().UpdateBitmap(cached_tile);
  } else {
    // Tile not cached, render it fresh
    RenderTile16(tilemap, tile_id);
  }
}

std::vector<uint8_t> FetchTileDataFromGraphicsBuffer(
    const std::vector<uint8_t> &data, int tile_id, int sheet_offset) {
  const int tile_width = 8;
  const int tile_height = 8;
  const int buffer_width = 128;
  const int sheet_height = 32;

  const int tiles_per_row = buffer_width / tile_width;
  const int rows_per_sheet = sheet_height / tile_height;
  const int tiles_per_sheet = tiles_per_row * rows_per_sheet;

  int sheet = (tile_id / tiles_per_sheet) % 4 + sheet_offset;
  int position_in_sheet = tile_id % tiles_per_sheet;
  int row_in_sheet = position_in_sheet / tiles_per_row;
  int column_in_sheet = position_in_sheet % tiles_per_row;

  assert(sheet >= sheet_offset && sheet <= sheet_offset + 3);

  std::vector<uint8_t> tile_data(tile_width * tile_height);
  for (int y = 0; y < tile_height; ++y) {
    for (int x = 0; x < tile_width; ++x) {
      int src_x = column_in_sheet * tile_width + x;
      int src_y = (sheet * sheet_height) + (row_in_sheet * tile_height) + y;

      int src_index = (src_y * buffer_width) + src_x;
      int dest_index = y * tile_width + x;
      tile_data[dest_index] = data[src_index];
    }
  }
  return tile_data;
}

namespace {

void MirrorTileDataVertically(std::vector<uint8_t> &tile_data) {
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 8; ++x) {
      std::swap(tile_data[y * 8 + x], tile_data[(7 - y) * 8 + x]);
    }
  }
}

void MirrorTileDataHorizontally(std::vector<uint8_t> &tile_data) {
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 4; ++x) {
      std::swap(tile_data[y * 8 + x], tile_data[y * 8 + (7 - x)]);
    }
  }
}

void ComposeAndPlaceTilePart(Tilemap &tilemap, const std::vector<uint8_t> &data,
                             const TileInfo &tile_info, int base_x, int base_y,
                             int sheet_offset) {
  std::vector<uint8_t> tile_data =
      FetchTileDataFromGraphicsBuffer(data, tile_info.id_, sheet_offset);

  if (tile_info.vertical_mirror_) {
    MirrorTileDataVertically(tile_data);
  }
  if (tile_info.horizontal_mirror_) {
    MirrorTileDataHorizontally(tile_data);
  }

  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      int src_index = y * 8 + x;
      int dest_x = base_x + x;
      int dest_y = base_y + y;
      int dest_index = (dest_y * tilemap.atlas.width()) + dest_x;
      tilemap.atlas.WriteToPixel(dest_index, tile_data[src_index]);
    }
  };
}
}  // namespace

void ModifyTile16(Tilemap &tilemap, const std::vector<uint8_t> &data,
                  const TileInfo &top_left, const TileInfo &top_right,
                  const TileInfo &bottom_left, const TileInfo &bottom_right,
                  int sheet_offset, int tile_id) {
  // Calculate the base position for this Tile16 in the full-size bitmap
  int tiles_per_row = tilemap.atlas.width() / tilemap.tile_size.x;
  int tile16_row = tile_id / tiles_per_row;
  int tile16_column = tile_id % tiles_per_row;
  int base_x = tile16_column * tilemap.tile_size.x;
  int base_y = tile16_row * tilemap.tile_size.y;

  // Compose and place each part of the Tile16
  ComposeAndPlaceTilePart(tilemap, data, top_left, base_x, base_y,
                          sheet_offset);
  ComposeAndPlaceTilePart(tilemap, data, top_right, base_x + 8, base_y,
                          sheet_offset);
  ComposeAndPlaceTilePart(tilemap, data, bottom_left, base_x, base_y + 8,
                          sheet_offset);
  ComposeAndPlaceTilePart(tilemap, data, bottom_right, base_x + 8, base_y + 8,
                          sheet_offset);

  tilemap.tile_info[tile_id] = {top_left, top_right, bottom_left, bottom_right};
}

void ComposeTile16(Tilemap &tilemap, const std::vector<uint8_t> &data,
                   const TileInfo &top_left, const TileInfo &top_right,
                   const TileInfo &bottom_left, const TileInfo &bottom_right,
                   int sheet_offset) {
  int num_tiles = tilemap.tile_info.size();
  int tiles_per_row = tilemap.atlas.width() / tilemap.tile_size.x;
  int tile16_row = num_tiles / tiles_per_row;
  int tile16_column = num_tiles % tiles_per_row;
  int base_x = tile16_column * tilemap.tile_size.x;
  int base_y = tile16_row * tilemap.tile_size.y;

  ComposeAndPlaceTilePart(tilemap, data, top_left, base_x, base_y,
                          sheet_offset);
  ComposeAndPlaceTilePart(tilemap, data, top_right, base_x + 8, base_y,
                          sheet_offset);
  ComposeAndPlaceTilePart(tilemap, data, bottom_left, base_x, base_y + 8,
                          sheet_offset);
  ComposeAndPlaceTilePart(tilemap, data, bottom_right, base_x + 8, base_y + 8,
                          sheet_offset);

  tilemap.tile_info.push_back({top_left, top_right, bottom_left, bottom_right});
}

std::vector<uint8_t> GetTilemapData(Tilemap &tilemap, int tile_id) {
  int tile_size = tilemap.tile_size.x;
  std::vector<uint8_t> data(tile_size * tile_size);
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
