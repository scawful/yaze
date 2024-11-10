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
          int src_index = tile_index + (y * 8 + x);
          int dest_x = col * 8 + x;
          int dest_y = row * 8 + y;
          int dest_index = (dest_y * width * 8) + dest_x;
          tilesheet.mutable_bitmap()->mutable_data()[dest_index] =
              graphics_buffer[src_index];
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
                              const TileInfo& bottom_right, int sheet_offset) {
  sheet_offset_ = sheet_offset;
  // Calculate the base position for this Tile16 in the full-size bitmap
  int tiles_per_row = bitmap_->width() / tile_width_;
  int tile16_row = num_tiles_ / tiles_per_row;
  int tile16_column = num_tiles_ % tiles_per_row;
  int base_x = tile16_column * tile_width_;
  int base_y = tile16_row * tile_height_;

  // Compose and place each part of the Tile16
  ComposeAndPlaceTilePart(graphics_buffer, top_left, base_x, base_y);
  ComposeAndPlaceTilePart(graphics_buffer, top_right, base_x + 8, base_y);
  ComposeAndPlaceTilePart(graphics_buffer, bottom_left, base_x, base_y + 8);
  ComposeAndPlaceTilePart(graphics_buffer, bottom_right, base_x + 8,
                          base_y + 8);

  tile_info_.push_back({top_left, top_right, bottom_left, bottom_right});

  num_tiles_++;
}

void Tilesheet::ModifyTile16(const std::vector<uint8_t>& graphics_buffer,
                             const TileInfo& top_left,
                             const TileInfo& top_right,
                             const TileInfo& bottom_left,
                             const TileInfo& bottom_right, int tile_id,
                             int sheet_offset) {
  sheet_offset_ = sheet_offset;
  // Calculate the base position for this Tile16 in the full-size bitmap
  int tiles_per_row = bitmap_->width() / tile_width_;
  int tile16_row = tile_id / tiles_per_row;
  int tile16_column = tile_id % tiles_per_row;
  int base_x = tile16_column * tile_width_;
  int base_y = tile16_row * tile_height_;

  // Compose and place each part of the Tile16
  ComposeAndPlaceTilePart(graphics_buffer, top_left, base_x, base_y);
  ComposeAndPlaceTilePart(graphics_buffer, top_right, base_x + 8, base_y);
  ComposeAndPlaceTilePart(graphics_buffer, bottom_left, base_x, base_y + 8);
  ComposeAndPlaceTilePart(graphics_buffer, bottom_right, base_x + 8,
                          base_y + 8);

  tile_info_[tile_id] = {top_left, top_right, bottom_left, bottom_right};
}

void Tilesheet::ComposeAndPlaceTilePart(
    const std::vector<uint8_t>& graphics_buffer, const TileInfo& tile_info,
    int base_x, int base_y) {
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
      int src_index = y * 8 + x;
      int dest_x = base_x + x;
      int dest_y = base_y + y;
      int dest_index = (dest_y * bitmap_->width()) + dest_x;
      internal_data_[dest_index] = tile_data[src_index];
    }
  }

  bitmap_->set_data(internal_data_);
}

std::vector<uint8_t> Tilesheet::FetchTileDataFromGraphicsBuffer(
    const std::vector<uint8_t>& graphics_buffer, int tile_id) {
  const int tile_width = 8;
  const int tile_height = 8;
  const int buffer_width = 128;
  const int sheet_height = 32;

  const int tiles_per_row = buffer_width / tile_width;
  const int rows_per_sheet = sheet_height / tile_height;
  const int tiles_per_sheet = tiles_per_row * rows_per_sheet;

  // Calculate the position in the graphics_buffer_ based on tile_id
  std::vector<uint8_t> tile_data(0x40, 0x00);
  int sheet = (tile_id / tiles_per_sheet) % 4 + sheet_offset_;
  int position_in_sheet = tile_id % tiles_per_sheet;
  int row_in_sheet = position_in_sheet / tiles_per_row;
  int column_in_sheet = position_in_sheet % tiles_per_row;

  // Ensure that the sheet ID is between 212 and 215 if using full gfx buffer
  assert(sheet >= sheet_offset_ && sheet <= sheet_offset_ + 3);

  // Copy the tile data from the graphics_buffer_ to tile_data
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      // Calculate the position in the graphics_buffer_ based on tile_id
      int src_x = column_in_sheet * tile_width + x;
      int src_y = (sheet * sheet_height) + (row_in_sheet * tile_height) + y;

      int src_index = (src_y * buffer_width) + src_x;
      int dest_index = y * tile_width + x;

      tile_data[dest_index] = graphics_buffer[src_index];
    }
  }

  return tile_data;
}

void Tilesheet::MirrorTileDataVertically(std::vector<uint8_t>& tile_data) {
  std::vector<uint8_t> tile_data_copy = tile_data;
  for (int i = 0; i < 8; ++i) {    // For each row
    for (int j = 0; j < 8; ++j) {  // For each column
      int src_index = i * 8 + j;
      int dest_index = (7 - i) * 8 + j;  // Calculate the mirrored row
      tile_data_copy[dest_index] = tile_data[src_index];
    }
  }
  tile_data = tile_data_copy;
}

void Tilesheet::MirrorTileDataHorizontally(std::vector<uint8_t>& tile_data) {
  std::vector<uint8_t> tile_data_copy = tile_data;
  for (int i = 0; i < 8; ++i) {    // For each row
    for (int j = 0; j < 8; ++j) {  // For each column
      int src_index = i * 8 + j;
      int dest_index = i * 8 + (7 - j);  // Calculate the mirrored column
      tile_data_copy[dest_index] = tile_data[src_index];
    }
  }
  tile_data = tile_data_copy;
}

void Tilesheet::MirrorTileData(std::vector<uint8_t>& tile_data, bool mirrorX,
                               bool mirrorY) {
  std::vector tile_data_copy = tile_data;
  if (mirrorX) {
    MirrorTileDataHorizontally(tile_data_copy);
  }
  if (mirrorY) {
    MirrorTileDataVertically(tile_data_copy);
  }
  tile_data = tile_data_copy;
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze
