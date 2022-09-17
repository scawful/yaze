#ifndef YAZE_APP_ZELDA3_SPRITE_H
#define YAZE_APP_ZELDA3_SPRITE_H

#include <SDL.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {

class Sprite {
 public:
  uchar x_, y_, id_;
  uchar nx_, ny_;
  uchar layer_ = 0;
  uchar subtype_ = 0;
  uchar overlord_ = 0;
  std::string name_;
  uchar keyDrop_ = 0;
  int sizeMap_ = 512;
  bool overworld_ = false;
  bool preview_ = false;
  uchar map_id_ = 0;
  int map_x_ = 0;
  int map_y_ = 0;
  short room_id_ = 0;
  bool picker_ = false;
  bool selected_ = false;
  SDL_Rect bounding_box_;

  Bytes current_gfx_;
  Bytes preview_gfx_;

  int lowerX_ = 32;
  int lowerY_ = 32;
  int higherX_ = 0;
  int higherY_ = 0;
  int width_ = 16;
  int height_ = 16;

  Sprite(Bytes src, uchar mapid, uchar id, uchar x, uchar y, int map_x,
         int map_y);
  void updateBBox();

  void Draw(bool picker = false);

  void DrawSpriteTile(int x, int y, int srcx, int srcy, int pal,
                      bool mirror_x = false, bool mirror_y = false,
                      int sizex = 2, int sizey = 2, bool iskey = false);

  auto PreviewGraphics() { return preview_gfx_; }
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif