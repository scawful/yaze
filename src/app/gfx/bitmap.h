#ifndef YAZE_APP_UTILS_BITMAP_H
#define YAZE_APP_UTILS_BITMAP_H

#include <SDL2/SDL.h>

#include <memory>

#include "app/core/constants.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {
namespace gfx {

class Bitmap {
 public:
  Bitmap() = default;
  Bitmap(int width, int height, int depth, uchar *data);

  void Create(int width, int height, int depth, uchar *data);
  void Create(int width, int height, int depth, int data_size);
  void CreateTexture(std::shared_ptr<SDL_Renderer> renderer);

  void ApplyPalette(const SNESPalette &palette);

  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }
  uchar *GetData() const { return pixel_data_; }
  SDL_Texture *GetTexture() const { return texture_; }

 private:
  int width_;
  int height_;
  int depth_;
  int data_size_;
  uchar *pixel_data_;
  SDL_Surface *surface_;
  SDL_Texture *texture_;
  SNESPalette palette_;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif