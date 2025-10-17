#include "app/gfx/render/tilemap.h"

#include <vector>

#include "app/gfx/resource/arena.h"
#include "app/gfx/render/atlas_renderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze {
namespace gfx {

Tilemap CreateTilemap(IRenderer* renderer, std::vector<uint8_t> &data, int width, int height,
                      int tile_size, int num_tiles, SnesPalette &palette) {
  Tilemap tilemap;
  tilemap.tile_size.x = tile_size;
  tilemap.tile_size.y = tile_size;
  tilemap.map_size.x = num_tiles;
  tilemap.map_size.y = num_tiles;
  tilemap.atlas = Bitmap(width, height, 8, data);
  tilemap.atlas.SetPalette(palette);
  
  // Queue texture creation directly via Arena
  if (tilemap.atlas.is_active() && tilemap.atlas.surface()) {
    Arena::Get().QueueTextureCommand(Arena::TextureCommandType::CREATE, &tilemap.atlas);
  }
  
  return tilemap;
}

void UpdateTilemap(IRenderer* renderer, Tilemap &tilemap, const std::vector<uint8_t> &data) {
  tilemap.atlas.set_data(data);
  
  // Queue texture update directly via Arena
  if (tilemap.atlas.texture() && tilemap.atlas.is_active() && tilemap.atlas.surface()) {
    Arena::Get().QueueTextureCommand(Arena::TextureCommandType::UPDATE, &tilemap.atlas);
  } else if (!tilemap.atlas.texture() && tilemap.atlas.is_active() && tilemap.atlas.surface()) {
    // Create if doesn't exist yet
    Arena::Get().QueueTextureCommand(Arena::TextureCommandType::CREATE, &tilemap.atlas);
  }
}

void RenderTile(IRenderer* renderer, Tilemap &tilemap, int tile_id) {
  // Validate tilemap state before proceeding
  if (!tilemap.atlas.is_active() || tilemap.atlas.vector().empty()) {
    return;
  }
  
  if (tile_id < 0) {
    return;
  }
  
  // Get tile data without using problematic tile cache
  auto tile_data = GetTilemapData(tilemap, tile_id);
  if (tile_data.empty()) {
    return;
  }
  
  // Note: Tile cache disabled to prevent std::move() related crashes
}

void RenderTile16(IRenderer* renderer, Tilemap &tilemap, int tile_id) {
  // Validate tilemap state before proceeding
  if (!tilemap.atlas.is_active() || tilemap.atlas.vector().empty()) {
    return;
  }
  
  if (tile_id < 0) {
    return;
  }
  
  int tiles_per_row = tilemap.atlas.width() / tilemap.tile_size.x;
  if (tiles_per_row <= 0) {
    return;
  }
  
  int tile_x = (tile_id % tiles_per_row) * tilemap.tile_size.x;
  int tile_y = (tile_id / tiles_per_row) * tilemap.tile_size.y;
  
  // Validate tile position
  if (tile_x < 0 || tile_x >= tilemap.atlas.width() || 
      tile_y < 0 || tile_y >= tilemap.atlas.height()) {
    return;
  }
  
  // Note: Tile cache disabled to prevent std::move() related crashes
}

void UpdateTile16(IRenderer* renderer, Tilemap &tilemap, int tile_id) {
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
    
    // Queue texture update directly via Arena
    if (cached_tile->texture() && cached_tile->is_active()) {
      Arena::Get().QueueTextureCommand(Arena::TextureCommandType::UPDATE, cached_tile);
    }
  } else {
    // Tile not cached, render it fresh
    RenderTile16(renderer, tilemap, tile_id);
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
  
  // Comprehensive validation to prevent crashes
  if (tile_id < 0) {
    SDL_Log("GetTilemapData: Invalid tile_id %d (negative)", tile_id);
    return std::vector<uint8_t>(256, 0); // Return empty 16x16 tile data
  }
  
  if (!tilemap.atlas.is_active()) {
    SDL_Log("GetTilemapData: Atlas is not active for tile_id %d", tile_id);
    return std::vector<uint8_t>(256, 0); // Return empty 16x16 tile data
  }
  
  if (tilemap.atlas.vector().empty()) {
    SDL_Log("GetTilemapData: Atlas vector is empty for tile_id %d", tile_id);
    return std::vector<uint8_t>(256, 0); // Return empty 16x16 tile data
  }
  
  if (tilemap.tile_size.x <= 0 || tilemap.tile_size.y <= 0) {
    SDL_Log("GetTilemapData: Invalid tile size (%d, %d) for tile_id %d", 
            tilemap.tile_size.x, tilemap.tile_size.y, tile_id);
    return std::vector<uint8_t>(256, 0); // Return empty 16x16 tile data
  }
  
  int tile_size = tilemap.tile_size.x;
  int width = tilemap.atlas.width();
  int height = tilemap.atlas.height();
  

  // Validate atlas dimensions
  if (width <= 0 || height <= 0) {
    SDL_Log("GetTilemapData: Invalid atlas dimensions (%d, %d) for tile_id %d", 
            width, height, tile_id);
    return std::vector<uint8_t>(tile_size * tile_size, 0);
  }
  
  // Calculate maximum possible tile_id based on atlas size
  int tiles_per_row = width / tile_size;
  int tiles_per_column = height / tile_size;
  int max_tile_id = tiles_per_row * tiles_per_column - 1;
  
  if (tile_id > max_tile_id) {
    SDL_Log("GetTilemapData: tile_id %d exceeds maximum %d (atlas: %dx%d, tile_size: %d)", 
            tile_id, max_tile_id, width, height, tile_size);
    return std::vector<uint8_t>(tile_size * tile_size, 0);
  }

  std::vector<uint8_t> data(tile_size * tile_size);
  
  
  for (int ty = 0; ty < tile_size; ty++) {
    for (int tx = 0; tx < tile_size; tx++) {
      // Calculate atlas position more safely
      int tile_row = tile_id / tiles_per_row;
      int tile_col = tile_id % tiles_per_row;
      int atlas_x = tile_col * tile_size + tx;
      int atlas_y = tile_row * tile_size + ty;
      int atlas_index = atlas_y * width + atlas_x;
      
      // Comprehensive bounds checking
      if (atlas_x >= 0 && atlas_x < width && 
          atlas_y >= 0 && atlas_y < height &&
          atlas_index >= 0 && atlas_index < static_cast<int>(tilemap.atlas.vector().size())) {
        uint8_t value = tilemap.atlas.vector()[atlas_index];
        data[ty * tile_size + tx] = value;
      } else {
        SDL_Log("GetTilemapData: Atlas position (%d, %d) or index %d out of bounds (atlas: %dx%d, size: %zu)", 
                atlas_x, atlas_y, atlas_index, width, height, tilemap.atlas.vector().size());
        data[ty * tile_size + tx] = 0; // Default to 0 if out of bounds
      }
    }
  }

  return data;
}

void RenderTilesBatch(IRenderer* renderer, Tilemap& tilemap, const std::vector<int>& tile_ids, 
                      const std::vector<std::pair<float, float>>& positions,
                      const std::vector<std::pair<float, float>>& scales) {
  if (tile_ids.empty() || positions.empty() || tile_ids.size() != positions.size()) {
    return;
  }
  
  ScopedTimer timer("tilemap_batch_render");
  
  // Initialize atlas renderer if not already done
  auto& atlas_renderer = AtlasRenderer::Get();
  if (!renderer) {
    // For now, we'll use the existing rendering approach
    // In a full implementation, we'd get the renderer from the core system
    return;
  }
  
  // Prepare render commands
  std::vector<RenderCommand> render_commands;
  render_commands.reserve(tile_ids.size());
  
  for (size_t i = 0; i < tile_ids.size(); ++i) {
    int tile_id = tile_ids[i];
    float x = positions[i].first;
    float y = positions[i].second;
    
    // Get scale factors (default to 1.0 if not provided)
    float scale_x = 1.0F;
    float scale_y = 1.0F;
    if (i < scales.size()) {
      scale_x = scales[i].first;
      scale_y = scales[i].second;
    }
    
    // Try to get tile from cache first
    Bitmap* cached_tile = tilemap.tile_cache.GetTile(tile_id);
    if (!cached_tile) {
      // Create and cache the tile if not found
      gfx::Bitmap new_tile = gfx::Bitmap(
          tilemap.tile_size.x, tilemap.tile_size.y, 8,
          gfx::GetTilemapData(tilemap, tile_id), tilemap.atlas.palette());
      tilemap.tile_cache.CacheTile(tile_id, std::move(new_tile));
      cached_tile = tilemap.tile_cache.GetTile(tile_id);
      if (cached_tile) {
        cached_tile->CreateTexture();
      }
    }
    
    if (cached_tile && cached_tile->is_active()) {
      // Queue texture creation if needed
      if (!cached_tile->texture() && cached_tile->surface()) {
        Arena::Get().QueueTextureCommand(Arena::TextureCommandType::CREATE, cached_tile);
      }
      
      // Add to atlas renderer
      int atlas_id = atlas_renderer.AddBitmap(*cached_tile);
      if (atlas_id >= 0) {
        render_commands.emplace_back(atlas_id, x, y, scale_x, scale_y);
      }
    }
  }
  
  // Render all commands in batch
  if (!render_commands.empty()) {
    atlas_renderer.RenderBatch(render_commands);
  }
}

}  // namespace gfx
}  // namespace yaze
