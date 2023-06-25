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
  Sprite();
  Sprite(Bytes src, uchar mapid, uchar id, uchar x, uchar y, int map_x,
         int map_y);
  void InitSprite(Bytes& src, uchar mapid, uchar id, uchar x, uchar y,
                  int map_x, int map_y);
  void updateBBox();

  void Draw();
  void DrawSpriteTile(int x, int y, int srcx, int srcy, int pal,
                      bool mirror_x = false, bool mirror_y = false,
                      int sizex = 2, int sizey = 2);

  // New methods
  void updateCoordinates(int map_x, int map_y);

  auto PreviewGraphics() const { return preview_gfx_; }
  auto GetRealX() const { return bounding_box_.x; }
  auto GetRealY() const { return bounding_box_.y; }
  auto id() const { return id_; }

  auto Width() const { return bounding_box_.w; }
  auto Height() const { return bounding_box_.h; }
  std::string Name() const { return name_; }

 private:
  Bytes current_gfx_;
  bool overworld_;
  uchar map_id_;
  uchar id_;
  uchar x_;
  uchar y_;
  uchar nx_;
  uchar ny_;
  uchar overlord_ = 0;
  std::string name_;
  int map_x_;
  int map_y_;
  Bytes preview_gfx_;
  uchar lowerX_;
  uchar lowerY_;
  uchar higherX_;
  uchar higherY_;
  SDL_Rect bounding_box_;

  int width_ = 16;
  int height_ = 16;

};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif