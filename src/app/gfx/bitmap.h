#ifndef YAZE_APP_UTILS_BITMAP_H
#define YAZE_APP_UTILS_BITMAP_H

#include <SDL2/SDL.h>
#include <rommapping.h>

#include "app/core/constants.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace gfx {

class Bitmap {
 public:
  Bitmap() = default;
  Bitmap(int width, int height, int depth, char *data);

  void Create(int width, int height, int depth, uchar *data);
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }
  void CreateTexture(std::shared_ptr<SDL_Renderer> renderer);
  inline SDL_Texture *GetTexture() const { return texture_; }

 private:
  int width_;
  int height_;
  int depth_;
  char *pixel_data_;
  SDL_Surface *surface_;
  SDL_Texture *texture_;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif