#include "bitmap.h"

#include <SDL2/SDL.h>
#include <rommapping.h>

#include "app/core/constants.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace gfx {

Bitmap::Bitmap(int width, int height, int depth, char *data)
    : width_(width), height_(height), depth_(depth), pixel_data_(data) {
  surface_ = SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                            SDL_PIXELFORMAT_INDEX8);
  // Default grayscale palette
  for (int i = 0; i < 8; i++) {
    surface_->format->palette->colors[i].r = i * 31;
    surface_->format->palette->colors[i].g = i * 31;
    surface_->format->palette->colors[i].b = i * 31;
  }
  surface_->pixels = data;
}

void Bitmap::Create(int width, int height, int depth, uchar *data) {
  width_ = width;
  height_ = height;
  depth_ = depth;
  pixel_data_ = (char *)data;
  surface_ = SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                            SDL_PIXELFORMAT_INDEX8);
  // Default grayscale palette
  for (int i = 0; i < 8; i++) {
    surface_->format->palette->colors[i].r = i * 31;
    surface_->format->palette->colors[i].g = i * 31;
    surface_->format->palette->colors[i].b = i * 31;
  }
  surface_->pixels = pixel_data_;
}

void Bitmap::CreateTexture(std::shared_ptr<SDL_Renderer> renderer) {
  texture_ = SDL_CreateTextureFromSurface(renderer.get(), surface_);
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze
