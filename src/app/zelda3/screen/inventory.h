#ifndef YAZE_APP_ZELDA3_INVENTORY_H
#define YAZE_APP_ZELDA3_INVENTORY_H

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace screen {

constexpr int kInventoryStart = 0x6564A;
constexpr int kBowItemPos = 0x6F631;

class Inventory : public SharedRom {
 public:
  auto Bitmap() const { return bitmap_; }
  auto Tilesheet() const { return tilesheets_bmp_; }
  auto Palette() const { return palette_; }

  absl::Status Create();

 private:
  absl::Status BuildTileset();

  std::vector<uint8_t> data_;
  gfx::Bitmap bitmap_;

  std::vector<uint8_t> tilesheets_;
  std::vector<uint8_t> test_;
  gfx::Bitmap tilesheets_bmp_;
  gfx::SnesPalette palette_;

  gui::Canvas canvas_;
  std::vector<gfx::TileInfo> tiles_;
};

}  // namespace screen
}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_INVENTORY_H
