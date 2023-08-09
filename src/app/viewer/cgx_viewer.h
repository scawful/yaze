#ifndef YAZE_APP_VIEWER_CGX_VIEWER_H
#define YAZE_APP_VIEWER_CGX_VIEWER_H
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "app/core/pipeline.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace viewer {


class CgxViewer {
 public:
  void LoadCgx(ROM&);
  auto GetCgxData() const { return all_tiles_data_; }

  void DrawBG1(int p, int bpp);
  void DrawBG2();
  void DrawOAM(int bpp, int drawmode, gfx::OAMTile data, int frame);

 private:
  void LoadGfx(int comboBpp);
  void LoadScr();

  void RefreshPalettes();

  gfx::Bitmap screen_bitmap_;
  gfx::Bitmap room_bg1_bitmap_;
  gfx::Bitmap room_bg2_bitmap_;
  gfx::Bitmap indexed_bitmap_;

  std::string label1_text;

  int bpp_;
  int current_selection_ = 2;

  ROM all_tiles_data_;
  ROM raw_data_;
};

}  // namespace viewer
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_VIEWER_CGX_VIEWER_H