#ifndef YAZE_APP_GFX_TILESHEET_H
#define YAZE_APP_GFX_TILESHEET_H

#include <memory>
#include <vector>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace gfx {

enum class TileType { Tile8, Tile16 };

struct InternalTile16 {
  std::array<TileInfo, 4> tiles;
};

/**
 * @class Tilesheet
 * @brief Represents a tilesheet, which is a collection of tiles stored in a
 * bitmap.
 *
 * The Tilesheet class provides methods to manipulate and extract tiles from the
 * tilesheet. It also supports copying and mirroring tiles within the tilesheet.
 */
class Tilesheet {
 public:
  Tilesheet() = default;
  Tilesheet(std::shared_ptr<Bitmap> bitmap, int tileWidth, int tileHeight,
            TileType tile_type)
      : bitmap_(std::move(bitmap)),
        tile_width_(tileWidth),
        tile_height_(tileHeight),
        tile_type_(tile_type) {}

  void Init(int width, int height, TileType tile_type);

  void ComposeTile16(const std::vector<uint8_t>& graphics_buffer,
                     const TileInfo& top_left, const TileInfo& top_right,
                     const TileInfo& bottom_left, const TileInfo& bottom_right,
                     int sheet_offset = 0);
  void ModifyTile16(const std::vector<uint8_t>& graphics_buffer,
                    const TileInfo& top_left, const TileInfo& top_right,
                    const TileInfo& bottom_left, const TileInfo& bottom_right,
                    int tile_id, int sheet_offset = 0);

  void ComposeAndPlaceTilePart(const std::vector<uint8_t>& graphics_buffer,
                               const TileInfo& tile_info, int baseX, int baseY);

  // Extracts a tile from the tilesheet
  Bitmap GetTile(int tileX, int tileY, int bmp_width, int bmp_height) {
    std::vector<uint8_t> tileData(tile_width_ * tile_height_);
    int tile_data_offset = 0;
    bitmap_->Get8x8Tile(CalculateTileIndex(tileX, tileY), tileX, tileY,
                        tileData, tile_data_offset);
    return Bitmap(bmp_width, bmp_height, bitmap_->depth(), tileData);
  }

  Bitmap GetTile16(int tile_id) {
    int tiles_per_row = bitmap_->width() / tile_width_;
    int tile_x = (tile_id % tiles_per_row) * tile_width_;
    int tile_y = (tile_id / tiles_per_row) * tile_height_;
    std::vector<uint8_t> tile_data(tile_width_ * tile_height_, 0x00);
    int tile_data_offset = 0;
    bitmap_->Get16x16Tile(tile_x, tile_y, tile_data, tile_data_offset);
    return Bitmap(16, 16, bitmap_->depth(), tile_data);
  }

  // Copy a tile within the tilesheet
  void CopyTile(int srcX, int srcY, int destX, int destY, bool mirror_x = false,
                bool mirror_y = false) {
    auto src_tile = GetTile(srcX, srcY, tile_width_, tile_height_);
    auto dest_tile_data = src_tile.vector();
    MirrorTileData(dest_tile_data, mirror_x, mirror_y);
    WriteTile(destX, destY, dest_tile_data);
  }

  auto bitmap() const { return bitmap_; }
  auto mutable_bitmap() { return bitmap_; }
  auto num_tiles() const { return num_tiles_; }
  auto tile_width() const { return tile_width_; }
  auto tile_height() const { return tile_height_; }
  auto set_palette(gfx::SnesPalette& palette) { palette_ = palette; }
  auto palette() const { return palette_; }
  auto tile_type() const { return tile_type_; }
  auto tile_info() const { return tile_info_; }
  auto mutable_tile_info() { return tile_info_; }
  void clear() {
    palette_.clear();
    internal_data_.clear();
    tile_info_.clear();
    bitmap_.reset();
    num_tiles_ = 0;
  }

 private:
  int CalculateTileIndex(int x, int y) {
    return y * (bitmap_->width() / tile_width_) + x;
  }

  std::vector<uint8_t> FetchTileDataFromGraphicsBuffer(
      const std::vector<uint8_t>& graphics_buffer, int tile_id);

  void MirrorTileDataVertically(std::vector<uint8_t>& tileData);
  void MirrorTileDataHorizontally(std::vector<uint8_t>& tileData);
  void MirrorTileData(std::vector<uint8_t>& tileData, bool mirror_x,
                      bool mirror_y);

  void WriteTile(int x, int y, const std::vector<uint8_t>& tileData) {
    int tileDataOffset = 0;
    bitmap_->Get8x8Tile(CalculateTileIndex(x, y), x, y,
                        const_cast<std::vector<uint8_t>&>(tileData),
                        tileDataOffset);
  }

  int num_tiles_ = 0;
  int tile_width_ = 0;
  int tile_height_ = 0;
  int sheet_offset_ = 0;

  TileType tile_type_;
  SnesPalette palette_;
  std::vector<uint8_t> internal_data_;
  std::vector<InternalTile16> tile_info_;
  std::shared_ptr<Bitmap> bitmap_;
};

absl::StatusOr<Tilesheet> CreateTilesheetFromGraphicsBuffer(
    const uint8_t* graphics_buffer, int width, int height, TileType tile_type,
    int sheet_id);

}  // namespace gfx

}  // namespace yaze

#endif  // YAZE_APP_GFX_TILESHEET_H
