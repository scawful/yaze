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
  LoadColFile();
  LoadGfx(current_bpp);
  LoadScr();
}

void CgxViewer::LoadColFile() {
  uchar data[512];

  std::vector<gfx::SNESColor> colors;
  for (int i = 0; i < 512; i += 2) {
    colors[i / 2] = gfx::GetCgxColor((short)((data[i + 1] << 8) + data[i]));
  }
}

void CgxViewer::LoadCgx(std::string pathfile) {
  unsigned char* ptr = rawData.data();

  for (int i = 0; i < 0x40000; i++) {
    ptr[i] = 0;
    rawData[i] = 0;
  }

  std::ifstream fs(pathfile, std::ios::binary);
  std::vector<unsigned char> data((std::istreambuf_iterator<char>(fs)),
                                  std::istreambuf_iterator<char>());
  fs.close();

  int matchingPos = -1;
  bool matched = false;
  for (int i = 0; i < data.size(); i++) {
    if (matched) {
      break;
    }
    rawData[i] = data[i];
    for (int j = 0; j < matchBytes.size(); j++) {
      if (data[i + j] == matchBytes[j]) {
        if (j == matchBytes.size() - 1) {
          matchingPos = i;
          matched = true;
          break;
        }
      } else {
        break;
      }
    }
  }

  char buffer[10];
  sprintf(buffer, "%X4", matchingPos);
  label1_text = "CGX In Folder L : " + std::string(buffer);
  LoadGfx(current_selection_);
}

struct GFX_Class {
  unsigned char* indexedPointer;
} GFX;
struct PictureBox_Class {
  void (*Refresh)();
} pictureBox1;

void CgxViewer::LoadGfx(int comboBpp) {
  if (comboBpp == 0) {
    bpp_ = 4;
  } else if (comboBpp == 1) {
    bpp_ = 2;
  } else if (comboBpp == 2) {
    bpp_ = 8;
  } else if (comboBpp == 3) {
    bpp_ = 40;
    unsigned char* ptr = GFX.indexedPointer;
    for (int i = 0; i < rawData.size(); i++) {
      ptr[i] = rawData[i];
    }
    RefreshPalettes();
    pictureBox1.Refresh();
    return;
  }

  unsigned char* ptr = GFX.indexedPointer;
  Bytes rawBytes;  // rawData.data()
  std::vector<unsigned char> dd = gfx::SnesTo8bppSheet(rawBytes, bpp_);
  for (int i = 0; i < dd.size(); i++) {
    ptr[i] = dd[i];
  }

  RefreshPalettes();
  pictureBox1.Refresh();
}

void CgxViewer::LoadScr() {}

void CgxViewer::RefreshPalettes() {}

}  // namespace viewer
}  // namespace app
}  // namespace yaze