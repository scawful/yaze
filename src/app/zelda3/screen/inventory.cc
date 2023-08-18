#include "inventory.h"

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/rom.h"


namespace yaze {
namespace app {
namespace zelda3 {

void Inventory::Create() {
  data_.reserve(256 * 256);
  for (int i = 0; i < 256 * 256; i++) {
    data_.push_back(0xFF);
  }
  PRINT_IF_ERROR(BuildTileset())
  for (int i = 0; i < 0x500; i += 0x08) {
    tiles_.push_back(gfx::GetTilesInfo(rom()->toint16(i + kBowItemPos)));
    tiles_.push_back(gfx::GetTilesInfo(rom()->toint16(i + kBowItemPos + 0x02)));
    tiles_.push_back(gfx::GetTilesInfo(rom()->toint16(i + kBowItemPos + 0x04)));
    tiles_.push_back(gfx::GetTilesInfo(rom()->toint16(i + kBowItemPos + 0x08)));
  }
  const int offsets[] = {0x00, 0x08, 0x800, 0x808};
  auto xx = 0;
  auto yy = 0;

  int i = 0;
  for (const auto& tile : tiles_) {
    int offset = offsets[i];
    for (auto y = 0; y < 0x08; ++y) {
      for (auto x = 0; x < 0x08; ++x) {
        int mx = x;
        int my = y;

        if (tile.horizontal_mirror_ != 0) {
          mx = 0x07 - x;
        }

        if (tile.vertical_mirror_ != 0) {
          my = 0x07 - y;
        }

        int xpos = ((tile.id_ % 0x10) * 0x08);
        int ypos = (((tile.id_ / 0x10)) * 0x400);
        int source = ypos + xpos + (x + (y * 0x80));

        auto destination = xx + yy + offset + (mx + (my * 0x100));
        data_[destination] = (test_[source] & 0x0F) + tile.palette_ * 0x08;
      }
    }

    if (i == 4) {
      i = 0;
      xx += 0x10;
      if (xx >= 0x100) {
        yy += 0x1000;
        xx = 0;
      }
    } else {
      i++;
    }
  }
  bitmap_.Create(256, 256, 128, data_);
  bitmap_.ApplyPalette(palette_);
  rom()->RenderBitmap(&bitmap_);
}

absl::Status Inventory::BuildTileset() {
  tilesheets_.reserve(6 * 0x2000);
  for (int i = 0; i < 6 * 0x2000; i++) tilesheets_.push_back(0xFF);
  ASSIGN_OR_RETURN(tilesheets_, rom()->Load2bppGraphics())
  Bytes test;
  for (int i = 0; i < 0x4000; i++) {
    test_.push_back(tilesheets_[i]);
  }
  for (int i = 0x8000; i < +0x8000 + 0x2000; i++) {
    test_.push_back(tilesheets_[i]);
  }
  tilesheets_bmp_.Create(128, 0x130, 64, test_);
  palette_ = rom()->GetPaletteGroup("hud")[0];
  tilesheets_bmp_.ApplyPalette(palette_);
  rom()->RenderBitmap(&tilesheets_bmp_);
  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze