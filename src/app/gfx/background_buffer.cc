#include "app/gfx/background_buffer.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"

namespace yaze::gfx {

BackgroundBuffer::BackgroundBuffer(int width, int height)
    : width_(width), height_(height) {
  // Initialize buffer with size for SNES layers
  const int total_tiles = (width / 8) * (height / 8);
  buffer_.resize(total_tiles, 0);
}

void BackgroundBuffer::SetTileAt(int x, int y, uint16_t value) {
  if (x < 0 || y < 0) return;
  int tiles_w = width_ / 8;
  int tiles_h = height_ / 8;
  if (x >= tiles_w || y >= tiles_h) return;
  buffer_[y * tiles_w + x] = value;
}

uint16_t BackgroundBuffer::GetTileAt(int x, int y) const {
  int tiles_w = width_ / 8;
  int tiles_h = height_ / 8;
  if (x < 0 || y < 0 || x >= tiles_w || y >= tiles_h) return 0;
  return buffer_[y * tiles_w + x];
}

void BackgroundBuffer::ClearBuffer() { std::ranges::fill(buffer_, 0); }

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
  int tiles_w = width_ / 8;
  int tiles_h = height_ / 8;
  if ((int)buffer_.size() < tiles_w * tiles_h) {
    buffer_.resize(tiles_w * tiles_h);
  }

  // For each tile on the tile buffer
  for (int yy = 0; yy < tiles_h; yy++) {
    for (int xx = 0; xx < tiles_w; xx++) {
      uint16_t word = buffer_[xx + yy * tiles_w];
      // Prevent draw if tile == 0xFFFF since it's 0 indexed
      if (word == 0xFFFF) continue;
      auto tile = gfx::WordToTileInfo(word);
      DrawTile(tile, bitmap_.mutable_data().data(), gfx16_data.data(),
               (yy * 512) + (xx * 8));
    }
  }
}

void BackgroundBuffer::DrawFloor(const std::vector<uint8_t>& rom_data,
                                 int tile_address, int tile_address_floor,
                                 uint8_t floor_graphics) {
  auto f = (uint8_t)(floor_graphics << 4);

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

}  // namespace yaze::gfx
