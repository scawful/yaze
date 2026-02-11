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
  // Initialize priority buffer for per-pixel priority tracking
  // Uses 0xFF as "no priority set" (transparent/empty pixel)
  priority_buffer_.resize(width * height, 0xFF);
  // Coverage buffer tracks whether this layer wrote a given pixel.
  // (0 = not written, 1 = written)
  coverage_buffer_.resize(width * height, 0);
  // Note: bitmap_ is NOT initialized here to avoid circular dependency
  // with Arena::Get(). Call EnsureBitmapInitialized() before accessing bitmap().
}

void BackgroundBuffer::SetTileAt(int x_pos, int y_pos, uint16_t value) {
  if (x_pos < 0 || y_pos < 0) {
    return;
  }
  int tiles_w = width_ / 8;
  int tiles_h = height_ / 8;
  if (x_pos >= tiles_w || y_pos >= tiles_h) {
    return;
  }
  buffer_[y_pos * tiles_w + x_pos] = value;
}

uint16_t BackgroundBuffer::GetTileAt(int x_pos, int y_pos) const {
  int tiles_w = width_ / 8;
  int tiles_h = height_ / 8;
  if (x_pos < 0 || y_pos < 0 || x_pos >= tiles_w || y_pos >= tiles_h) {
    return 0;
  }
  return buffer_[y_pos * tiles_w + x_pos];
}

void BackgroundBuffer::ClearBuffer() {
  std::ranges::fill(buffer_, 0);
  ClearPriorityBuffer();
  ClearCoverageBuffer();
}

void BackgroundBuffer::ClearPriorityBuffer() {
  // 0xFF indicates no priority set (transparent/empty pixel)
  std::ranges::fill(priority_buffer_, 0xFF);
}

void BackgroundBuffer::ClearCoverageBuffer() {
  // 0 indicates the layer never wrote here; 1 indicates it did.
  std::ranges::fill(coverage_buffer_, 0);
}

uint8_t BackgroundBuffer::GetPriorityAt(int x, int y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    return 0xFF;  // Out of bounds = no priority
  }
  return priority_buffer_[y * width_ + x];
}

void BackgroundBuffer::SetPriorityAt(int x, int y, uint8_t priority) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    return;
  }
  priority_buffer_[y * width_ + x] = priority;
}

void BackgroundBuffer::EnsureBitmapInitialized() {
  if (!bitmap_.is_active() || bitmap_.width() == 0) {
    // IMPORTANT: Initialize to 255 (transparent fill), NOT 0!
    // In dungeon rendering, pixel value 0 represents palette[0] (actual color).
    // Only 255 is treated as transparent by IsTransparent().
    bitmap_.Create(width_, height_, 8,
                   std::vector<uint8_t>(width_ * height_, 255));
    
    // Fix: Set index 255 to be transparent so the background is actually transparent
    // instead of white (default SDL palette index 255)
    std::vector<SDL_Color> palette(256);
    // Initialize with grayscale for debugging
    for (int i = 0; i < 256; i++) {
      palette[i] = {static_cast<Uint8>(i), static_cast<Uint8>(i), static_cast<Uint8>(i), 255};
    }
    // Set index 255 to transparent
    palette[255] = {0, 0, 0, 0};
    bitmap_.SetPalette(palette);
    
    // Enable blending
    if (bitmap_.surface()) {
      SDL_SetSurfaceBlendMode(bitmap_.surface(), SDL_BLENDMODE_BLEND);
    }
  }
  
  // Ensure priority buffer is properly sized
  if (priority_buffer_.size() != static_cast<size_t>(width_ * height_)) {
    priority_buffer_.resize(width_ * height_, 0xFF);
  }

  // Ensure coverage buffer is properly sized
  if (coverage_buffer_.size() != static_cast<size_t>(width_ * height_)) {
    coverage_buffer_.resize(width_ * height_, 0);
  }
}

void BackgroundBuffer::DrawTile(const TileInfo& tile, uint8_t* canvas,
                                const uint8_t* tiledata, int indexoffset) {
  // tiledata is now 8BPP linear data (1 byte per pixel)
  // Buffer size: 0x10000 (65536 bytes) = 64 tile rows max
  constexpr int kGfxBufferSize = 0x10000;
  constexpr int kMaxTileRow = 63;  // 64 rows (0-63), each 1024 bytes

  // Calculate tile position in the 8BPP buffer
  int tile_col_idx = tile.id_ % 16;
  int tile_row_idx = tile.id_ / 16;

  // CRITICAL: Validate tile_row to prevent index out of bounds
  if (tile_row_idx > kMaxTileRow) {
    return;  // Skip invalid tiles silently
  }

  int tile_base_x = tile_col_idx * 8;    // 8 pixels wide (8 bytes)
  int tile_base_y = tile_row_idx * 1024; // 8 rows * 128 bytes stride (sheet width)

  // Palette offset calculation using 16-color bank chunking (matches SNES CGRAM)
  //
  // SNES CGRAM layout:
  // - Each CGRAM row has 16 colors, with index 0 being transparent
  // - Dungeon tiles use palette bits 2-7, mapping to CGRAM rows 2-7
  // - We map palette bits 2-7 to SDL banks 0-5
  //
  // Drawing formula: final_color = pixel + (bank * 16)
  // Where pixel 0 = transparent (not written), pixel 1-15 = colors within bank
  uint8_t pal = tile.palette_ & 0x07;
  uint8_t palette_offset;
  if (pal >= 2 && pal <= 7) {
    // Map palette bits 2-7 to SDL banks 0-5 using 16-color stride
    palette_offset = (pal - 2) * 16;
  } else {
    // Palette 0-1 are for HUD/other - fallback to first bank
    palette_offset = 0;
  }

  // Pre-calculate max valid destination index
  int max_dest = width_ * height_;
  
  // Get priority bit from tile (over_ = priority bit in SNES tilemap)
  uint8_t priority = tile.over_ ? 1 : 0;

  // Copy 8x8 pixels
  for (int py = 0; py < 8; py++) {
    int src_row = tile.vertical_mirror_ ? (7 - py) : py;

    for (int px = 0; px < 8; px++) {
      int src_col = tile.horizontal_mirror_ ? (7 - px) : px;

      // Calculate source index
      // Stride is 128 bytes (sheet width)
      int src_index = (src_row * 128) + src_col + tile_base_x + tile_base_y;

      // Bounds check source
      if (src_index < 0 || src_index >= kGfxBufferSize) continue;

      uint8_t pixel = tiledata[src_index];

      if (pixel != 0) {
        // Pixel 0 is transparent (not written). Pixels 1-15 map to bank indices 1-15.
        // With 16-color bank chunking: final_color = pixel + (bank * 16)
        uint8_t final_color = pixel + palette_offset;
        int dest_index = indexoffset + (py * width_) + px;

        // Bounds check destination
        if (dest_index >= 0 && dest_index < max_dest) {
          canvas[dest_index] = final_color;
          // Also store priority for this pixel
          priority_buffer_[dest_index] = priority;
        }
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
  // initialized earlier. If bitmap doesn't exist, create it ONCE with 255 fill
  // IMPORTANT: Use 255 (transparent), NOT 0! Pixel value 0 = palette[0] (actual color)
  if (!bitmap_.is_active() || bitmap_.width() == 0) {
    bitmap_.Create(width_, height_, 8,
                   std::vector<uint8_t>(width_ * height_, 255));
  }
  
  // Ensure priority buffer is properly sized
  if (priority_buffer_.size() != static_cast<size_t>(width_ * height_)) {
    priority_buffer_.resize(width_ * height_, 0xFF);
  }

  // For each tile on the tile buffer
  // int drawn_count = 0;
  // int skipped_count = 0;
  for (int yy = 0; yy < tiles_h; yy++) {
    for (int xx = 0; xx < tiles_w; xx++) {
      uint16_t word = buffer_[xx + yy * tiles_w];

      // Skip empty tiles (0xFFFF) - these show the floor
      if (word == 0xFFFF) {
        // skipped_count++;
        continue;
      }

      // Skip zero tiles - also show the floor
      if (word == 0) {
        // skipped_count++;
        continue;
      }

      auto tile = gfx::WordToTileInfo(word);

      // Skip floor tiles (0xEC-0xFD) - don't overwrite DrawFloor's work
      // These are the animated floor tiles, already drawn by DrawFloor
      // Skip floor tiles (0xEC-0xFD) - don't overwrite DrawFloor's work
      // These are the animated floor tiles, already drawn by DrawFloor
      // if (tile.id_ >= 0xEC && tile.id_ <= 0xFD) {
      //   skipped_count++;
      //   continue;
      // }

      // Calculate pixel offset for tile position (xx, yy) in the 512x512 bitmap
      // Each tile is 8x8, so pixel Y = yy * 8, pixel X = xx * 8
      // Linear offset = (pixel_y * width) + pixel_x = (yy * 8 * 512) + (xx * 8)
      int tile_offset = (yy * 8 * width_) + (xx * 8);
      DrawTile(tile, bitmap_.mutable_data().data(), gfx16_data.data(),
               tile_offset);
      // drawn_count++;
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
  // IMPORTANT: Use 255 (transparent fill), NOT 0! Pixel value 0 = palette[0] (actual color)
  if (!bitmap_.is_active() || bitmap_.width() == 0) {
    LOG_DEBUG("[DrawFloor]", "Creating bitmap: %dx%d, active=%d, width=%d",
              width_, height_, bitmap_.is_active(), bitmap_.width());
    bitmap_.Create(width_, height_, 8,
                   std::vector<uint8_t>(width_ * height_, 255));
    LOG_DEBUG("[DrawFloor]", "After Create: active=%d, width=%d, height=%d",
              bitmap_.is_active(), bitmap_.width(), bitmap_.height());
  } else {
    LOG_DEBUG("[DrawFloor]",
              "Bitmap already exists: active=%d, width=%d, height=%d",
              bitmap_.is_active(), bitmap_.width(), bitmap_.height());
  }

  auto floor_offset = static_cast<uint8_t>(floor_graphics << 4);

  // Create floor tiles from ROM data
  gfx::TileInfo floorTile1(rom_data[tile_address + floor_offset],
                           rom_data[tile_address + floor_offset + 1]);
  gfx::TileInfo floorTile2(rom_data[tile_address + floor_offset + 2],
                           rom_data[tile_address + floor_offset + 3]);
  gfx::TileInfo floorTile3(rom_data[tile_address + floor_offset + 4],
                           rom_data[tile_address + floor_offset + 5]);
  gfx::TileInfo floorTile4(rom_data[tile_address + floor_offset + 6],
                           rom_data[tile_address + floor_offset + 7]);

  gfx::TileInfo floorTile5(rom_data[tile_address_floor + floor_offset],
                           rom_data[tile_address_floor + floor_offset + 1]);
  gfx::TileInfo floorTile6(rom_data[tile_address_floor + floor_offset + 2],
                           rom_data[tile_address_floor + floor_offset + 3]);
  gfx::TileInfo floorTile7(rom_data[tile_address_floor + floor_offset + 4],
                           rom_data[tile_address_floor + floor_offset + 5]);
  gfx::TileInfo floorTile8(rom_data[tile_address_floor + floor_offset + 6],
                           rom_data[tile_address_floor + floor_offset + 7]);

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
