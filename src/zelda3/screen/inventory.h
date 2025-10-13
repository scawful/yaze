#ifndef YAZE_APP_ZELDA3_INVENTORY_H
#define YAZE_APP_ZELDA3_INVENTORY_H

#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/rom.h"

namespace yaze {
namespace zelda3 {

constexpr int kInventoryStart = 0x6564A;
constexpr int kBowItemPos = 0x6F631;

class Inventory {
 public:
  absl::Status Create();

  auto &bitmap() { return bitmap_; }
  auto &tilesheet() { return tilesheets_bmp_; }
  auto &palette() { return palette_; }

  void LoadRom(Rom *rom) { rom_ = rom; }
  auto rom() { return rom_; }

 private:
  absl::Status BuildTileset();

  std::vector<uint8_t> data_;
  gfx::Bitmap bitmap_;

  std::vector<uint8_t> tilesheets_;
  std::vector<uint8_t> test_;
  gfx::Bitmap tilesheets_bmp_;
  gfx::SnesPalette palette_;

  Rom *rom_;
  gui::Canvas canvas_;
  std::vector<gfx::TileInfo> tiles_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_INVENTORY_H
