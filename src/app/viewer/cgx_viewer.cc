#include "cgx_viewer.h"

#include <fstream>
#include <iostream>
#include <vector>

#include "app/core/pipeline.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {
namespace viewer {

constexpr int kMatchedBytes[] = {0x4E, 0x41, 0x4B, 0x31, 0x39, 0x38, 0x39};

void CgxViewer::LoadCgx(ROM &cgx_rom) {
  std::cout << "Loading CGX" << std::endl;
  raw_data_.malloc(0x40000);
  all_tiles_data_.malloc(0x40000);

  int matching_position = -1;
  bool matched = false;
  for (int i = 0; i < cgx_rom.size(); i++) {
    if (matched) {
      break;
    }

    raw_data_[i] = cgx_rom[i];
    for (int j = 0; j < 7; j++) {
      if (cgx_rom[i + j] == kMatchedBytes[j]) {
        if (j == 7 - 1) {
          matching_position = i;
          matched = true;
          break;
        }
      } else {
        break;
      }
    }
  }

  label1_text = absl::StrCat("CGX In Folder L : ",
                             absl::StrFormat("%X4", matching_position));
  LoadGfx(current_selection_);
}

void CgxViewer::DrawBG1(int p, int bpp) {
  auto *ptr = (uchar *)screen_bitmap_.data();
  // for each tile on the tile buffer
  for (int i = 0; i < 0x400; i++) {
    if (room_bg1_bitmap_.data()[i + p] != 0xFFFF) {
      gfx::TileInfo t = gfx::GetTilesInfo(room_bg1_bitmap_.data()[i + p]);

      for (uint16_t yl = 0; yl < 8; yl++) {
        for (uint16_t xl = 0; xl < 8; xl++) {
          int mx = xl * (1 - t.horizontal_mirror_) +
                   (7 - xl) * (t.horizontal_mirror_);
          int my =
              yl * (1 - t.vertical_mirror_) + (7 - yl) * (t.vertical_mirror_);

          int ty = (t.id_ / 16) * 1024;
          int tx = (t.id_ % 16) * 8;
          uchar pixel = all_tiles_data_[(tx + ty) + (yl * 128) + xl];

          int index =
              (((i % 32) * 8) + ((i / 32) * 2048) + ((mx) + (my * 256)));

          if (bpp != 8) {
            ptr[index] = (uchar)((((pixel)&0xFF) + t.palette_ * 16));
          } else {
            ptr[index] = (uchar)((((pixel)&0xFF)));
          }
        }
      }
    }
  }

  // Apply data to Bitmap
}
void CgxViewer::DrawBG2() {}

void CgxViewer::DrawOAM(int bpp, int drawmode, gfx::OAMTile data, int frame) {}

void CgxViewer::LoadGfx(int combo_bpp) {
  if (combo_bpp == 0) {
    bpp_ = 4;
  } else if (combo_bpp == 1) {
    bpp_ = 2;
  } else if (combo_bpp == 2) {
    bpp_ = 8;
  } else if (combo_bpp == 3) {
    bpp_ = 40;
    for (int i = 0; i < raw_data_.size(); i++) {
      all_tiles_data_[i] = raw_data_[i];
    }
    // Refresh palettes and "picture box" aka canvas
    RefreshPalettes();
    return;
  }

  Bytes decomp_sheet = gfx::BPP8SNESToIndexed(raw_data_.vector(), bpp_);
  for (int i = 0; i < decomp_sheet.size(); i++) {
    all_tiles_data_[i] = decomp_sheet[i];
  }

  RefreshPalettes();
}

void CgxViewer::LoadScr() {}

void CgxViewer::RefreshPalettes() {}

}  // namespace viewer
}  // namespace app
}  // namespace yaze