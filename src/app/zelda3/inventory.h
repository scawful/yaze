#ifndef YAZE_APP_ZELDA3_INVENTORY_H
#define YAZE_APP_ZELDA3_INVENTORY_H

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/gfx/snes_palette.h"
#include "app/rom.h"
#include "gui/canvas.h"

namespace yaze {
namespace app {
namespace zelda3 {

constexpr int kInventoryStart = 0x6564A;
constexpr int kBowItemPos = 0x6F631;

class Inventory {
 public:
  void SetupROM(ROM& rom) { rom_ = rom; }
  auto Bitmap() const { return bitmap_; }
  auto Tilesheet() const { return tilesheets_bmp_; }
  auto Palette() const { return palette_; }

  void Create();

 private:
  absl::Status BuildTileset();

  ROM rom_;

  Bytes data_;
  gfx::Bitmap bitmap_;

  Bytes tilesheets_;
  Bytes test_;
  gfx::Bitmap tilesheets_bmp_;
  gfx::SNESPalette palette_;

  gui::Canvas canvas_;
  std::vector<gfx::TileInfo> tiles_;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif