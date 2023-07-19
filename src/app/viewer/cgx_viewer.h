#ifndef YAZE_APP_VIEWER_CGX_VIEWER_H
#define YAZE_APP_VIEWER_CGX_VIEWER_H
#include <fstream>
#include <iostream>
#include <vector>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace viewer {

class CgxViewer {
 public:
  void Update();

 private:
  void LoadColFile();

  void LoadCgx(std::string pathfile);
  void LoadGfx(int comboBpp);
  void LoadScr();

  void RefreshPalettes();

  std::vector<unsigned char> matchBytes;  // Assuming it's a vector of bytes.
  std::string label1_text;

  int bpp_;
  int current_selection_;
  ROM rawData;
};

}  // namespace viewer
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_VIEWER_CGX_VIEWER_H