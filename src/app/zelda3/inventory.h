#ifndef YAZE_APP_ZELDA3_INVENTORY_H
#define YAZE_APP_ZELDA3_INVENTORY_H

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "gui/canvas.h"

namespace yaze {
namespace app {
namespace zelda3 {

constexpr int kMenuGfxStart = 0xE000;
constexpr int kLampItemPos = 0x6F6F0;
constexpr int kBowItemPos = 0x6F631;

class Inventory {
 public:
  void SetupROM(ROM& rom) { rom_ = rom; }
  auto Bitmap() const { return bitmap_; }
  auto Tilesheet() const { return tilesheets_bmp_; }

  void Create(Bytes& all_gfx);

 private:
  void BuildTileset(Bytes& all_gfx);


  ROM rom_;

  Bytes data_;
  gfx::Bitmap bitmap_;

  Bytes tilesheets_;
  gfx::Bitmap tilesheets_bmp_;

  gui::Canvas canvas_;
  std::vector<gfx::TileInfo> tiles_;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif