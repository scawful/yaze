#include "app/gui/canvas/coordinate_mapper.h"

#include <algorithm>
#include <cmath>

#include "app/gui/canvas/canvas.h"

namespace yaze {
namespace gui {

ScreenToTileResult CoordinateMapper::ScreenToTile(float screen_x,
                                                   float screen_y) const {
  ScreenToTileResult result;
  result.screen_x = screen_x;
  result.screen_y = screen_y;

  // Step 1: Check if within canvas bounds
  result.in_canvas_bounds = IsInCanvasBounds(screen_x, screen_y);

  // Step 2: Convert to canvas-relative coordinates
  result.canvas_x = screen_x - geometry_.canvas_p0.x;
  result.canvas_y = screen_y - geometry_.canvas_p0.y;

  // Step 3: Apply inverse scale (for zoomed canvases)
  ImVec2 scaled = ApplyInverseScale(result.canvas_x, result.canvas_y);
  result.scaled_x = scaled.x;
  result.scaled_y = scaled.y;

  // Step 4: Apply scroll offset to get content position
  ImVec2 content = ApplyScrollOffset(result.scaled_x, result.scaled_y);
  result.content_x = content.x;
  result.content_y = content.y;

  // Step 5: Get tile information
  if (result.in_canvas_bounds && result.content_x >= 0 && result.content_y >= 0) {
    result.tile_info = CanvasToTile(result.content_x, result.content_y);
    result.tile_info.canvas_id = canvas_id_;
  }

  return result;
}

TileHitInfo CoordinateMapper::CanvasToTile(float canvas_x,
                                            float canvas_y) const {
  TileHitInfo info;

  // Snap to tile grid
  auto [tile_x, tile_y] = SnapToTileGrid(canvas_x, canvas_y);

  // Validate tile coordinates
  if (!IsValidTileCoord(tile_x, tile_y)) {
    info.is_valid = false;
    return info;
  }

  info.map_x = tile_x;
  info.map_y = tile_y;
  info.is_valid = true;

  // Calculate tile IDs
  info.tile_id = GetTileId(canvas_x, canvas_y);

  if (config_.use_tile16) {
    info.tile16_index = GetTile16Index(canvas_x, canvas_y);
  }

  // Calculate tile origin in canvas space
  info.tile_origin_px = ImVec2(tile_x * config_.tile_size,
                                tile_y * config_.tile_size);
  info.tile_size_px = ImVec2(config_.tile_size, config_.tile_size);

  // Calculate local map ID (for overworld)
  if (config_.local_map_size > 0) {
    info.local_map_id = CalculateLocalMapId(canvas_x, canvas_y);
  }

  // Calculate ROM offset
  if (rom_offset_calculator_) {
    info.rom_offset = rom_offset_calculator_(info.tile_id);
  } else {
    info.rom_offset = CalculateRomOffset(info.tile_id);
  }

  return info;
}

ImVec2 CoordinateMapper::TileToCanvas(int tile_x, int tile_y) const {
  float x = tile_x * config_.tile_size - geometry_.scrolling.x;
  float y = tile_y * config_.tile_size - geometry_.scrolling.y;
  return ImVec2(x * scale_, y * scale_);
}

ImVec2 CoordinateMapper::TileToScreen(int tile_x, int tile_y) const {
  ImVec2 canvas_pos = TileToCanvas(tile_x, tile_y);
  return ImVec2(geometry_.canvas_p0.x + canvas_pos.x,
                geometry_.canvas_p0.y + canvas_pos.y);
}

int CoordinateMapper::GetTileId(float canvas_x, float canvas_y) const {
  if (config_.tile_size <= 0 || config_.tiles_per_row <= 0) {
    return -1;
  }

  int tile_x = static_cast<int>(canvas_x / config_.tile_size);
  int tile_y = static_cast<int>(canvas_y / config_.tile_size);

  if (!IsValidTileCoord(tile_x, tile_y)) {
    return -1;
  }

  return tile_y * config_.tiles_per_row + tile_x;
}

int CoordinateMapper::GetTile16Index(float canvas_x, float canvas_y) const {
  // Tile16 uses 16x16 tiles regardless of the base tile size
  const float tile16_size = 16.0f;
  int tile16_per_row = config_.tiles_per_row;

  if (config_.tile_size == 8.0f) {
    // If using 8x8 tiles, tile16 has half the tiles per row
    tile16_per_row = config_.tiles_per_row / 2;
  }

  int tile_x = static_cast<int>(canvas_x / tile16_size);
  int tile_y = static_cast<int>(canvas_y / tile16_size);

  if (tile_x < 0 || tile_y < 0 || tile_x >= tile16_per_row) {
    return -1;
  }

  return tile_y * tile16_per_row + tile_x;
}

bool CoordinateMapper::IsInCanvasBounds(float screen_x, float screen_y) const {
  return screen_x >= geometry_.canvas_p0.x &&
         screen_x < geometry_.canvas_p1.x &&
         screen_y >= geometry_.canvas_p0.y &&
         screen_y < geometry_.canvas_p1.y;
}

bool CoordinateMapper::IsValidTileCoord(int tile_x, int tile_y) const {
  return tile_x >= 0 && tile_y >= 0 &&
         tile_x < config_.tiles_per_row &&
         tile_y < config_.tiles_per_col;
}

uint32_t CoordinateMapper::CalculateRomOffset(int tile_id,
                                               uint32_t base_offset) const {
  if (tile_id < 0) {
    return 0;
  }

  // Default calculation: assumes contiguous tile data
  // Each tile16 is 8 bytes (4 tile references)
  // Each tile8 in 4BPP is 32 bytes
  if (config_.use_tile16) {
    return base_offset + (tile_id * 8);
  } else {
    return base_offset + (tile_id * 32);
  }
}

int CoordinateMapper::CalculateLocalMapId(float content_x,
                                           float content_y) const {
  if (config_.local_map_size <= 0) {
    return -1;
  }

  int local_x = static_cast<int>(content_x / config_.local_map_size);
  int local_y = static_cast<int>(content_y / config_.local_map_size);

  // Standard overworld layout: 8 maps wide
  const int maps_per_row = 8;
  return local_y * maps_per_row + local_x;
}

std::vector<ScreenToTileResult> CoordinateMapper::ScreenToTileBatch(
    const std::vector<std::pair<float, float>>& positions) const {
  std::vector<ScreenToTileResult> results;
  results.reserve(positions.size());

  for (const auto& [x, y] : positions) {
    results.push_back(ScreenToTile(x, y));
  }

  return results;
}

ImVec2 CoordinateMapper::ApplyScrollOffset(float canvas_x,
                                            float canvas_y) const {
  return ImVec2(canvas_x - geometry_.scrolling.x,
                canvas_y - geometry_.scrolling.y);
}

ImVec2 CoordinateMapper::ApplyInverseScale(float x, float y) const {
  if (scale_ <= 0.0f) {
    return ImVec2(x, y);
  }
  return ImVec2(x / scale_, y / scale_);
}

std::pair<int, int> CoordinateMapper::SnapToTileGrid(float x, float y) const {
  if (config_.tile_size <= 0) {
    return {-1, -1};
  }

  int tile_x = static_cast<int>(std::floor(x / config_.tile_size));
  int tile_y = static_cast<int>(std::floor(y / config_.tile_size));
  return {tile_x, tile_y};
}

// --- Free Functions ---

TileHitInfo ScreenToTileQuick(const CanvasRuntime& runtime,
                               float screen_x, float screen_y,
                               float tile_size) {
  TileHitInfo info;

  // Check bounds
  if (screen_x < runtime.canvas_p0.x || screen_x >= runtime.canvas_p0.x + runtime.canvas_sz.x ||
      screen_y < runtime.canvas_p0.y || screen_y >= runtime.canvas_p0.y + runtime.canvas_sz.y) {
    info.is_valid = false;
    return info;
  }

  // Convert to canvas-relative coordinates
  float canvas_x = screen_x - runtime.canvas_p0.x;
  float canvas_y = screen_y - runtime.canvas_p0.y;

  // Apply inverse scale
  float scale = runtime.scale > 0 ? runtime.scale : 1.0f;
  float scaled_x = canvas_x / scale;
  float scaled_y = canvas_y / scale;

  // Apply scroll offset
  float content_x = scaled_x - runtime.scrolling.x;
  float content_y = scaled_y - runtime.scrolling.y;

  // Calculate tile coordinates
  if (content_x < 0 || content_y < 0 || tile_size <= 0) {
    info.is_valid = false;
    return info;
  }

  int tile_x = static_cast<int>(content_x / tile_size);
  int tile_y = static_cast<int>(content_y / tile_size);

  // Calculate tile ID (assuming 32 tiles per row as default)
  int tiles_per_row = static_cast<int>(runtime.canvas_sz.x / (tile_size * scale));
  if (tiles_per_row <= 0) tiles_per_row = 32;

  info.map_x = tile_x;
  info.map_y = tile_y;
  info.tile_id = tile_y * tiles_per_row + tile_x;
  info.tile_origin_px = ImVec2(tile_x * tile_size, tile_y * tile_size);
  info.tile_size_px = ImVec2(tile_size, tile_size);
  info.is_valid = true;

  return info;
}

int CalculateTileIndex(float local_x, float local_y,
                       float tile_size, int tiles_per_row) {
  if (tile_size <= 0 || tiles_per_row <= 0) {
    return -1;
  }

  int tile_x = static_cast<int>(local_x / tile_size);
  int tile_y = static_cast<int>(local_y / tile_size);

  if (tile_x < 0 || tile_y < 0 || tile_x >= tiles_per_row) {
    return -1;
  }

  return tile_y * tiles_per_row + tile_x;
}

ImVec4 GetTileBounds(int tile_x, int tile_y, float tile_size, float scale) {
  float x = tile_x * tile_size * scale;
  float y = tile_y * tile_size * scale;
  float w = tile_size * scale;
  float h = tile_size * scale;
  return ImVec4(x, y, x + w, y + h);
}

}  // namespace gui
}  // namespace yaze
