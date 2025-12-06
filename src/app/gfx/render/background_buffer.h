#ifndef YAZE_APP_GFX_BACKGROUND_BUFFER_H
#define YAZE_APP_GFX_BACKGROUND_BUFFER_H

#include <cstdint>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze {
namespace gfx {

class BackgroundBuffer {
 public:
  BackgroundBuffer(int width = 512, int height = 512);

  // Buffer manipulation methods
  void SetTileAt(int x, int y, uint16_t value);
  uint16_t GetTileAt(int x, int y) const;
  void ClearBuffer();

  // Drawing methods
  void DrawTile(const TileInfo& tile_info, uint8_t* canvas,
                const uint8_t* tiledata, int indexoffset);
  void DrawBackground(std::span<uint8_t> gfx16_data);

  // Floor drawing methods
  void DrawFloor(const std::vector<uint8_t>& rom_data, int tile_address,
                 int tile_address_floor, uint8_t floor_graphics);

  // Ensure bitmap is initialized before accessing
  // Call this before using bitmap() if the buffer was created standalone
  void EnsureBitmapInitialized();

  // Priority buffer methods for per-tile priority support
  // SNES Mode 1 uses priority bits to control Z-ordering between layers
  void ClearPriorityBuffer();
  uint8_t GetPriorityAt(int x, int y) const;
  void SetPriorityAt(int x, int y, uint8_t priority);
  const std::vector<uint8_t>& priority_data() const { return priority_buffer_; }
  std::vector<uint8_t>& mutable_priority_data() { return priority_buffer_; }

  // Accessors
  auto buffer() { return buffer_; }
  auto& bitmap() { return bitmap_; }
  const gfx::Bitmap& bitmap() const { return bitmap_; }

 private:
  std::vector<uint16_t> buffer_;
  std::vector<uint8_t> priority_buffer_;  // Per-pixel priority (0 or 1)
  gfx::Bitmap bitmap_;
  int width_;
  int height_;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_BACKGROUND_BUFFER_H