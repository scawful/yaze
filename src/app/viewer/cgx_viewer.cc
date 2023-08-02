#include "cgx_viewer.h"

#include <fstream>
#include <iostream>
#include <vector>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {
namespace viewer {

void CgxViewer::Update() {
  static int current_bpp = 1;
  // ImGui::Combo("BPP", current_bpp, "0\0 1\0 2\0 3\0", 4, 4);
  LoadGfx(current_bpp);
  LoadScr();
}

void CgxViewer::LoadCgx(ROM& cgx_rom) {
  raw_data_.malloc(0x40000);
  all_tiles_data_.malloc(0x40000);

  std::vector<unsigned char> matched_bytes;
  int matching_position = -1;
  bool matched = false;
  for (int i = 0; i < cgx_rom.size(); i++) {
    if (matched) {
      break;
    }

    raw_data_[i] = cgx_rom[i];
    for (int j = 0; j < matched_bytes.size(); j++) {
      if (cgx_rom[i + j] == matched_bytes[j]) {
        if (j == matched_bytes.size() - 1) {
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