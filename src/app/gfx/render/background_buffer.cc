#include "app/gfx/render/background_buffer.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_tile.h"
#include "util/log.h"

namespace yaze::gfx {

BackgroundBuffer::BackgroundBuffer(int width, int height)
    : width_(width), height_(height) {
  // Initialize buffer with size for SNES layers
  const int total_tiles = (width / 8) * (height / 8);
  buffer_.resize(total_tiles, 0);
}

void BackgroundBuffer::SetTileAt(int x, int y, uint16_t value) {
  if (x < 0 || y < 0)
    return;
  int tiles_w = width_ / 8;
  int tiles_h = height_ / 8;
  if (x >= tiles_w || y >= tiles_h)
    return;
  buffer_[y * tiles_w + x] = value;
}

uint16_t BackgroundBuffer::GetTileAt(int x, int y) const {
  int tiles_w = width_ / 8;
  int tiles_h = height_ / 8;
  if (x < 0 || y < 0 || x >= tiles_w || y >= tiles_h)
    return 0;
  return buffer_[y * tiles_w + x];
}

void BackgroundBuffer::ClearBuffer() {
  std::ranges::fill(buffer_, 0);
}

void BackgroundBuffer::DrawTile(const TileInfo& tile, uint8_t* canvas,
                                const uint8_t* tiledata, int indexoffset) {
  // tiledata is now 8BPP linear data (1 byte per pixel)
  
  // Calculate tile position in the 8BPP buffer
  int tile_col_idx = tile.id_ % 16;
  int tile_row_idx = tile.id_ / 16;
  
  int tile_base_x = tile_col_idx * 8;    // 8 pixels wide (8 bytes)
  int tile_base_y = tile_row_idx * 1024; // 8 rows * 128 bytes stride (sheet width)
  
  // Palette handling
  uint8_t palette_idx = tile.palette_ & 0x0F;
  uint8_t palette_offset = palette_idx * 8; // 8-color packed groups

  // Copy 8x8 pixels
  for (int py = 0; py < 8; py++) {
    int src_row = tile.vertical_mirror_ ? (7 - py) : py;
    
    for (int px = 0; px < 8; px++) {
      int src_col = tile.horizontal_mirror_ ? (7 - px) : px;
      
      // Calculate source index
      // Stride is 128 bytes (sheet width)
      int src_index = (src_row * 128) + src_col + tile_base_x + tile_base_y;
      
      uint8_t pixel = tiledata[src_index];
      
      if (pixel != 0) {
        uint8_t final_color = pixel + palette_offset;
        int dest_index = indexoffset + (py * width_) + px;
        canvas[dest_index] = final_color;
      }
    }
  }
}

void BackgroundBuffer::DrawBackground(std::span<uint8_t> gfx16_data) {
  int tiles_w = width_ / 8;
  int tiles_h = height_ / 8;
  if ((int)buffer_.size() < tiles_w * tiles_h) {
    buffer_.resize(tiles_w * tiles_h);
  }

  // NEVER recreate bitmap here - it should be created by DrawFloor or
  // initialized earlier If bitmap doesn't exist, create it ONCE with zeros
  if (!bitmap_.is_active() || bitmap_.width() == 0) {
    bitmap_.Create(width_, height_, 8,
                   std::vector<uint8_t>(width_ * height_, 0));
  }

  // For each tile on the tile buffer
  int drawn_count = 0;
  int skipped_count = 0;
  for (int yy = 0; yy < tiles_h; yy++) {
    for (int xx = 0; xx < tiles_w; xx++) {
      uint16_t word = buffer_[xx + yy * tiles_w];

      // Skip empty tiles (0xFFFF) - these show the floor
      if (word == 0xFFFF) {
        skipped_count++;
        continue;
      }

      // Skip zero tiles - also show the floor
      if (word == 0) {
        skipped_count++;
        continue;
      }

      auto tile = gfx::WordToTileInfo(word);

      // Skip floor tiles (0xEC-0xFD) - don't overwrite DrawFloor's work
      // These are the animated floor tiles, already drawn by DrawFloor
      if (tile.id_ >= 0xEC && tile.id_ <= 0xFD) {
        skipped_count++;
        continue;
      }

      // Calculate pixel offset for tile position (xx, yy) in the 512x512 bitmap
      // Each tile is 8x8, so pixel Y = yy * 8, pixel X = xx * 8
      // Linear offset = (pixel_y * width) + pixel_x = (yy * 8 * 512) + (xx * 8)
      int tile_offset = (yy * 8 * width_) + (xx * 8);
      DrawTile(tile, bitmap_.mutable_data().data(), gfx16_data.data(),
               tile_offset);
      drawn_count++;
    }
  }
  // CRITICAL: Sync bitmap data back to SDL surface!
  // DrawTile() writes to bitmap_.mutable_data(), but the SDL surface needs
  // updating
  if (bitmap_.surface() && bitmap_.mutable_data().size() > 0) {
    SDL_LockSurface(bitmap_.surface());
    memcpy(bitmap_.surface()->pixels, bitmap_.mutable_data().data(),
           bitmap_.mutable_data().size());
    SDL_UnlockSurface(bitmap_.surface());
  }
}

void BackgroundBuffer::DrawFloor(const std::vector<uint8_t>& rom_data,
                                 int tile_address, int tile_address_floor,
                                 uint8_t floor_graphics) {
  // Create bitmap ONCE at the start if it doesn't exist
  if (!bitmap_.is_active() || bitmap_.width() == 0) {
    LOG_DEBUG("[DrawFloor]", "Creating bitmap: %dx%d, active=%d, width=%d",
              width_, height_, bitmap_.is_active(), bitmap_.width());
    bitmap_.Create(width_, height_, 8,
                   std::vector<uint8_t>(width_ * height_, 0));
    LOG_DEBUG("[DrawFloor]", "After Create: active=%d, width=%d, height=%d",
              bitmap_.is_active(), bitmap_.width(), bitmap_.height());
  } else {
    LOG_DEBUG("[DrawFloor]",
              "Bitmap already exists: active=%d, width=%d, height=%d",
              bitmap_.is_active(), bitmap_.width(), bitmap_.height());
  }

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

  // Floor tiles specify which 8-color sub-palette from the 90-color dungeon
  // palette e.g., palette 6 = colors 48-55 (6 * 8 = 48)

  // Draw the floor tiles in a pattern
  // Convert TileInfo to 16-bit words with palette information
  uint16_t word1 = gfx::TileInfoToWord(floorTile1);
  uint16_t word2 = gfx::TileInfoToWord(floorTile2);
  uint16_t word3 = gfx::TileInfoToWord(floorTile3);
  uint16_t word4 = gfx::TileInfoToWord(floorTile4);
  uint16_t word5 = gfx::TileInfoToWord(floorTile5);
  uint16_t word6 = gfx::TileInfoToWord(floorTile6);
  uint16_t word7 = gfx::TileInfoToWord(floorTile7);
  uint16_t word8 = gfx::TileInfoToWord(floorTile8);
  for (int xx = 0; xx < 16; xx++) {
    for (int yy = 0; yy < 32; yy++) {
      SetTileAt((xx * 4), (yy * 2), word1);
      SetTileAt((xx * 4) + 1, (yy * 2), word2);
      SetTileAt((xx * 4) + 2, (yy * 2), word3);
      SetTileAt((xx * 4) + 3, (yy * 2), word4);

      SetTileAt((xx * 4), (yy * 2) + 1, word5);
      SetTileAt((xx * 4) + 1, (yy * 2) + 1, word6);
      SetTileAt((xx * 4) + 2, (yy * 2) + 1, word7);
      SetTileAt((xx * 4) + 3, (yy * 2) + 1, word8);
    }
  }
}

}  // namespace yaze::gfx
