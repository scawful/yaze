#include "inventory.h"

#include "app/core/platform/renderer.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace zelda3 {

using core::Renderer;

absl::Status Inventory::Create() {
  data_.reserve(256 * 256);
  for (int i = 0; i < 256 * 256; i++) {
    data_.push_back(0xFF);
  }
  RETURN_IF_ERROR(BuildTileset())
  for (int i = 0; i < 0x500; i += 0x08) {
    ASSIGN_OR_RETURN(auto t1, rom()->ReadWord(i + kBowItemPos));
    ASSIGN_OR_RETURN(auto t2, rom()->ReadWord(i + kBowItemPos + 0x02));
    ASSIGN_OR_RETURN(auto t3, rom()->ReadWord(i + kBowItemPos + 0x04));
    ASSIGN_OR_RETURN(auto t4, rom()->ReadWord(i + kBowItemPos + 0x06));
    tiles_.push_back(gfx::GetTilesInfo(t1));
    tiles_.push_back(gfx::GetTilesInfo(t2));
    tiles_.push_back(gfx::GetTilesInfo(t3));
    tiles_.push_back(gfx::GetTilesInfo(t4));
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

  bitmap_.Create(256, 256, 8, data_);
  bitmap_.SetPalette(palette_);
  Renderer::Get().RenderBitmap(&bitmap_);
  return absl::OkStatus();
}

absl::Status Inventory::BuildTileset() {
  tilesheets_.reserve(6 * 0x2000);
  for (int i = 0; i < 6 * 0x2000; i++) tilesheets_.push_back(0xFF);
  ASSIGN_OR_RETURN(tilesheets_, Load2BppGraphics(*rom()));
  std::vector<uint8_t> test;
  for (int i = 0; i < 0x4000; i++) {
    test_.push_back(tilesheets_[i]);
  }
  for (int i = 0x8000; i < +0x8000 + 0x2000; i++) {
    test_.push_back(tilesheets_[i]);
  }
  tilesheets_bmp_.Create(128, 0x130, 64, test_);
  auto hud_pal_group = rom()->palette_group().hud;
  palette_ = hud_pal_group[0];
  tilesheets_bmp_.SetPalette(palette_);
  Renderer::Get().RenderBitmap(&tilesheets_bmp_);
  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
