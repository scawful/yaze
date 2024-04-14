#include "app/gfx/tilesheet.h"

#include <memory>
#include <vector>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_color.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {
namespace gfx {

absl::StatusOr<Tilesheet> CreateTilesheetFromGraphicsBuffer(
    const uint8_t* graphics_buffer, int width, int height, TileType tile_type,
    int sheet_id) {
  Tilesheet tilesheet;

  // Calculate the offset in the graphics buffer based on the sheet ID
  int sheet_offset = sheet_id * width * height;

  // Initialize the tilesheet with the specified width, height, and tile type
  tilesheet.Init(width, height, tile_type);

  // Iterate over the tiles in the sheet and copy them into the tilesheet
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      // Calculate the index of the current tile in the graphics buffer
      int tile_index = sheet_offset + (row * width + col) * 64;

      // Copy the tile data into the tilesheet
      for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
          int srcIndex = tile_index + (y * 8 + x);
          int destX = col * 8 + x;
          int destY = row * 8 + y;
          int destIndex = (destY * width * 8) + destX;
          tilesheet.mutable_bitmap()->mutable_data()[destIndex] =
              graphics_buffer[srcIndex];
        }
      }
    }
  }

  return tilesheet;
}

void Tilesheet::Init(int width, int height, TileType tile_type) {
  bitmap_ = std::make_shared<Bitmap>(width, height, 8, 0x20000);
  internal_data_.resize(0x20000);
  tile_type_ = tile_type;
  if (tile_type_ == TileType::Tile8) {
    tile_width_ = 8;
    tile_height_ = 8;
  } else {
    tile_width_ = 16;
    tile_height_ = 16;
  }
}

void Tilesheet::ComposeTile16(const std::vector<uint8_t>& graphics_buffer,
                              const TileInfo& top_left,
                              const TileInfo& top_right,
                              const TileInfo& bottom_left,
                              const TileInfo& bottom_right) {
  // Calculate the base position for this Tile16 in the full-size bitmap
  int tiles_per_row = bitmap_->width() / tile_width_;
  int tile16_row = num_tiles_ / tiles_per_row;
  int tile16_column = num_tiles_ % tiles_per_row;
  int baseX = tile16_column * tile_width_;
  int baseY = tile16_row * tile_height_;

  // Compose and place each part of the Tile16
  ComposeAndPlaceTilePart(graphics_buffer, top_left, baseX, baseY);
  ComposeAndPlaceTilePart(graphics_buffer, top_right, baseX + 8, baseY);
  ComposeAndPlaceTilePart(graphics_buffer, bottom_left, baseX, baseY + 8);
  ComposeAndPlaceTilePart(graphics_buffer, bottom_right, baseX + 8, baseY + 8);

  tile_info_.push_back({top_left, top_right, bottom_left, bottom_right});

  num_tiles_++;
}

void Tilesheet::ComposeAndPlaceTilePart(
    const std::vector<uint8_t>& graphics_buffer, const TileInfo& tile_info,
    int baseX, int baseY) {
  std::vector<uint8_t> tile_data =
      FetchTileDataFromGraphicsBuffer(graphics_buffer, tile_info.id_);

  if (tile_info.vertical_mirror_) {
    MirrorTileDataVertically(tile_data);
  }
  if (tile_info.horizontal_mirror_) {
    MirrorTileDataHorizontally(tile_data);
  }

  // Place the tile data into the full-size bitmap at the calculated position
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      int srcIndex = y * 8 + x;
      int destX = baseX + x;
      int destY = baseY + y;
      int destIndex = (destY * bitmap_->width()) + destX;
      internal_data_[destIndex] = tile_data[srcIndex];
    }
  }

  bitmap_->set_data(internal_data_);
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze
