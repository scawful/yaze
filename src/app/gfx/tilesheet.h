#ifndef YAZE_APP_GFX_TILESHEET_H
#define YAZE_APP_GFX_TILESHEET_H
#include <memory>
#include <vector>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {
namespace gfx {

enum class TileType { Tile8, Tile16 };

class Tilesheet {
 public:
  Tilesheet() = default;
  Tilesheet(std::shared_ptr<Bitmap> bitmap, int tileWidth, int tileHeight,
            TileType tile_type)
      : bitmap_(std::move(bitmap)),
        tile_width_(tileWidth),
        tile_height_(tileHeight),
        tile_type_(tile_type) {}

  void Init(int width, int height, TileType tile_type) {
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

  void ComposeTile16(const std::vector<uint8_t>& graphics_buffer,
                     const TileInfo& top_left, const TileInfo& top_right,
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
    ComposeAndPlaceTilePart(graphics_buffer, bottom_right, baseX + 8,
                            baseY + 8);

    tile_info_.push_back({top_left, top_right, bottom_left, bottom_right});

    num_tiles_++;
  }

  void ComposeAndPlaceTilePart(const std::vector<uint8_t>& graphics_buffer,
                               const TileInfo& tile_info, int baseX,
                               int baseY) {
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

  // Extracts a tile from the tilesheet
  Bitmap GetTile(int tileX, int tileY, int bmp_width, int bmp_height) {
    std::vector<uint8_t> tileData(tile_width_ * tile_height_);
    int tileDataOffset = 0;
    bitmap_->Get8x8Tile(CalculateTileIndex(tileX, tileY), tileX, tileY,
                        tileData, tileDataOffset);
    return Bitmap(bmp_width, bmp_height, bitmap_->depth(), tileData);
  }

  Bitmap GetTile16(int tile_x, int tile_y) {
    std::vector<uint8_t> tile_data(tile_width_ * tile_height_, 0x00);
    int tileDataOffset = 0;
    bitmap_->Get16x16Tile(tile_x, tile_y, tile_data, tileDataOffset);
    return Bitmap(16, 16, bitmap_->depth(), tile_data);
  }

  Bitmap GetTile16(int tile_id) {
    int tiles_per_row = bitmap_->width() / tile_width_;
    int tile_x = (tile_id % tiles_per_row) * tile_width_;
    int tile_y = (tile_id / tiles_per_row) * tile_height_;
    return GetTile16(tile_x, tile_y);
  }

  // Copy a tile within the tilesheet
  void CopyTile(int srcX, int srcY, int destX, int destY, bool mirrorX = false,
                bool mirrorY = false) {
    auto srcTile = GetTile(srcX, srcY, tile_width_, tile_height_);
    auto destTileData = srcTile.vector();
    MirrorTileData(destTileData, mirrorX, mirrorY);
    WriteTile(destX, destY, destTileData);
  }

  // Other methods and properties
  auto bitmap() const { return bitmap_; }
  auto mutable_bitmap() { return bitmap_; }
  auto num_tiles() const { return num_tiles_; }
  auto tile_width() const { return tile_width_; }
  auto tile_height() const { return tile_height_; }
  auto set_palette(gfx::SNESPalette& palette) { palette_ = palette; }
  auto palette() const { return palette_; }
  auto tile_type() const { return tile_type_; }
  auto tile_info() const { return tile_info_; }
  auto mutable_tile_info() { return tile_info_; }

 private:
  int CalculateTileIndex(int x, int y) {
    return y * (bitmap_->width() / tile_width_) + x;
  }

  std::vector<uint8_t> FetchTileDataFromGraphicsBuffer(
      const std::vector<uint8_t>& graphics_buffer, int tile_id) {
    const int tileWidth = 8;
    const int tileHeight = 8;
    const int bufferWidth = 128;
    const int sheetHeight = 32;

    const int tilesPerRow = bufferWidth / tileWidth;
    const int rowsPerSheet = sheetHeight / tileHeight;
    const int tilesPerSheet = tilesPerRow * rowsPerSheet;

    // Calculate the position in the graphics_buffer_ based on tile_id
    std::vector<uint8_t> tile_data(0x40, 0x00);
    int sheet = (tile_id / tilesPerSheet) % 4 + 212;
    int positionInSheet = tile_id % tilesPerSheet;
    int rowInSheet = positionInSheet / tilesPerRow;
    int columnInSheet = positionInSheet % tilesPerRow;

    // Ensure that the sheet ID is between 212 and 215
    assert(sheet >= 212 && sheet <= 215);

    // Copy the tile data from the graphics_buffer_ to tile_data
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        // Calculate the position in the graphics_buffer_ based on tile_id

        int srcX = columnInSheet * tileWidth + x;
        int srcY = (sheet * sheetHeight) + (rowInSheet * tileHeight) + y;

        int src_index = (srcY * bufferWidth) + srcX;
        int dest_index = y * tileWidth + x;

        tile_data[dest_index] = graphics_buffer[src_index];
      }
    }

    return tile_data;
  }

  void MirrorTileDataVertically(std::vector<uint8_t>& tileData) {
    std::vector<uint8_t> tile_data_copy = tileData;
    for (int i = 0; i < 8; ++i) {    // For each row
      for (int j = 0; j < 8; ++j) {  // For each column
        int src_index = i * 8 + j;
        int dest_index = (7 - i) * 8 + j;  // Calculate the mirrored row
        tile_data_copy[dest_index] = tileData[src_index];
      }
    }
    tileData = tile_data_copy;
  }

  void MirrorTileDataHorizontally(std::vector<uint8_t>& tileData) {
    std::vector<uint8_t> tile_data_copy = tileData;
    for (int i = 0; i < 8; ++i) {    // For each row
      for (int j = 0; j < 8; ++j) {  // For each column
        int src_index = i * 8 + j;
        int dest_index = i * 8 + (7 - j);  // Calculate the mirrored column
        tile_data_copy[dest_index] = tileData[src_index];
      }
    }
    tileData = tile_data_copy;
  }

  void MirrorTileData(std::vector<uint8_t>& tileData, bool mirrorX,
                      bool mirrorY) {
    // Implement logic to mirror tile data horizontally and/or vertically
    std::vector tile_data_copy = tileData;
    if (mirrorX) {
      MirrorTileDataHorizontally(tile_data_copy);
    }
    if (mirrorY) {
      MirrorTileDataVertically(tile_data_copy);
    }
    tileData = tile_data_copy;
  }

  void WriteTile(int x, int y, const std::vector<uint8_t>& tileData) {
    int tileDataOffset = 0;
    bitmap_->Get8x8Tile(CalculateTileIndex(x, y), x, y,
                        const_cast<std::vector<uint8_t>&>(tileData),
                        tileDataOffset);
  }

  gfx::SNESPalette palette_;
  std::vector<uint8_t> internal_data_;
  std::shared_ptr<Bitmap> bitmap_;
  struct InternalTile16 {
    std::array<TileInfo, 4> tiles;
  };
  std::vector<InternalTile16> tile_info_;
  int num_tiles_ = 0;
  int tile_width_ = 0;
  int tile_height_ = 0;
  TileType tile_type_;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_TILESHEET_H