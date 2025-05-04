#include "app/gfx/background_buffer.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "absl/log/log.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace gfx {

BackgroundBuffer::BackgroundBuffer(int width, int height)
    : width_(width), height_(height) {
  // Initialize buffer with size for SNES layers
  const int total_tiles = (width / 8) * (height / 8);
  buffer_.resize(total_tiles, 0);
}

void BackgroundBuffer::SetTileAt(int x, int y, uint16_t value) {
  if (x >= 0 && x < width_ / 8 && y >= 0 && y < height_ / 8) {
    buffer_[y * (width_ / 8) + x] = value;
  }
}

uint16_t BackgroundBuffer::GetTileAt(int x, int y) const {
  if (x >= 0 && x < width_ / 8 && y >= 0 && y < height_ / 8) {
    return buffer_[y * (width_ / 8) + x];
  }
  return 0;
}

void BackgroundBuffer::ClearBuffer() {
  std::fill(buffer_.begin(), buffer_.end(), 0);
}

void BackgroundBuffer::DrawTile(const TileInfo& tile, uint8_t* canvas,
                                const uint8_t* tiledata, int indexoffset) {
  int tx = (tile.id_ / 16 * 512) + ((tile.id_ & 0xF) * 4);
  uint8_t palnibble = (uint8_t)(tile.palette_ << 4);
  uint8_t r = tile.horizontal_mirror_ ? 1 : 0;

  for (int yl = 0; yl < 512; yl += 64) {
    int my = indexoffset + (tile.vertical_mirror_ ? 448 - yl : yl);

    for (int xl = 0; xl < 4; xl++) {
      int mx = 2 * (tile.horizontal_mirror_ ? 3 - xl : xl);

      uint8_t pixel = tiledata[tx + yl + xl];

      int index = mx + my;
      canvas[index + r ^ 1] = (uint8_t)((pixel & 0x0F) | palnibble);
      canvas[index + r] = (uint8_t)((pixel >> 4) | palnibble);
    }
  }
}

void BackgroundBuffer::DrawBackground(std::span<uint8_t> gfx16_data) {
  // Initialize bitmap
  bitmap_.Create(width_, height_, 8, std::vector<uint8_t>(width_ * height_, 0));
  if (buffer_.size() < width_ * height_) {
    buffer_.resize(width_ * height_);
  }

  // For each tile on the tile buffer
  for (int yy = 0; yy < (height_ / 8) * (width_ / 8); yy += (width_ / 8)) {
    for (int xx = 0; xx < width_ / 8; xx++) {
      // Prevent draw if tile == 0xFFFF since it's 0 indexed
      if (buffer_[xx + yy] != 0xFFFF) {
        auto tile = gfx::GetTilesInfo(buffer_[xx + yy]);
        DrawTile(tile, bitmap_.mutable_data().data(), gfx16_data.data(), 0);
      }
    }
  }
}

void BackgroundBuffer::DrawFloor(const std::vector<uint8_t>& rom_data,
                                 int tile_address, int tile_address_floor,
                                 uint8_t floor_graphics) {
  uint8_t f = (uint8_t)(floor_graphics << 4);

  // Create floor tiles from ROM data
  gfx::TileInfo floorTile1(rom_data[tile_address + f],
                           rom_data[tile_address + f + 1]);
  gfx::TileInfo floorTile2(rom_data[tile_address + f + 2],
                           rom_data[tile_address + f + 3]);
  gfx::TileInfo floorTile3(rom_data[tile_address + f + 4],
                           rom_data[tile_address + f + 5]);
  gfx::TileInfo floorTile4(rom_data[tile_address + f + 6],
                           rom_data[tile_address + f + 7]);

  gfx::TileInfo floorTile5(rom_data[tile_address_floor + f],
                           rom_data[tile_address_floor + f + 1]);
  gfx::TileInfo floorTile6(rom_data[tile_address_floor + f + 2],
                           rom_data[tile_address_floor + f + 3]);
  gfx::TileInfo floorTile7(rom_data[tile_address_floor + f + 4],
                           rom_data[tile_address_floor + f + 5]);
  gfx::TileInfo floorTile8(rom_data[tile_address_floor + f + 6],
                           rom_data[tile_address_floor + f + 7]);

  // Draw the floor tiles in a pattern
  for (int xx = 0; xx < 16; xx++) {
    for (int yy = 0; yy < 32; yy++) {
      SetTileAt((xx * 4), (yy * 2), floorTile1.id_);
      SetTileAt((xx * 4) + 1, (yy * 2), floorTile2.id_);
      SetTileAt((xx * 4) + 2, (yy * 2), floorTile3.id_);
      SetTileAt((xx * 4) + 3, (yy * 2), floorTile4.id_);

      SetTileAt((xx * 4), (yy * 2) + 1, floorTile5.id_);
      SetTileAt((xx * 4) + 1, (yy * 2) + 1, floorTile6.id_);
      SetTileAt((xx * 4) + 2, (yy * 2) + 1, floorTile7.id_);
      SetTileAt((xx * 4) + 3, (yy * 2) + 1, floorTile8.id_);
    }
  }
}

}  // namespace gfx
}  // namespace yaze