#include "inventory.h"

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "gui/canvas.h"

namespace yaze {
namespace app {
namespace zelda3 {
void Inventory::Create(Bytes& all_gfx) {
  data_.reserve(256 * 256);
  for (int i = 0; i < 256 * 256; i++) {
    data_.push_back(0xFF);
  }
  PRINT_IF_ERROR(BuildTileset(all_gfx))
  //tiles_.push_back(gfx::GetTilesInfo(rom_.toint16(kInventoryStart)));

  tiles_.push_back(gfx::GetTilesInfo(rom_.toint16(kBowItemPos)));
  tiles_.push_back(gfx::GetTilesInfo(rom_.toint16(kBowItemPos + 0x02)));
  tiles_.push_back(gfx::GetTilesInfo(rom_.toint16(kBowItemPos + 0x04)));
  tiles_.push_back(gfx::GetTilesInfo(rom_.toint16(kBowItemPos + 0x08)));
  // tiles_.push_back(gfx::GetTilesInfo(rom_.toint16(kLampItemPos)));
  // tiles_.push_back(gfx::GetTilesInfo(rom_.toint16(kLampItemPos + 0x02)));
  // tiles_.push_back(gfx::GetTilesInfo(rom_.toint16(kLampItemPos + 0x04)));
  // tiles_.push_back(gfx::GetTilesInfo(rom_.toint16(kLampItemPos + 0x08)));
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
        data_[destination] = (tilesheets_[source] & 0x0F);
      }
    }

    if (i == 4) {
      i = 0;
      xx += 0x10;
      if (xx >= 0x80) {
        yy += 0x800;
        xx = 0;
      }
    } else {
      i++;
    }
  }
  bitmap_.Create(256, 256, 128, data_);
  // bitmap_.ApplyPalette(rom_.GetPaletteGroup("hud")[0]);
  rom_.RenderBitmap(&bitmap_);
}

absl::Status Inventory::BuildTileset(Bytes& all_gfx) {
  tilesheets_.reserve(6 * 0x2000);
  for (int i = 0; i < 6 * 0x2000; i++) tilesheets_.push_back(0xFF);
  ASSIGN_OR_RETURN(tilesheets_, rom_.Load2bppGraphics())
  tilesheets_bmp_.Create(128, 192 + 96 + 48 - 16, 64, tilesheets_);
  rom_.RenderBitmap(&tilesheets_bmp_);
  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze