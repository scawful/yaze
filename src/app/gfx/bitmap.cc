#include "bitmap.h"

#include <SDL2/SDL.h>

#include <memory>

#include "app/core/constants.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {
namespace gfx {

Bitmap::Bitmap(int width, int height, int depth, uchar *data)
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
  pixel_data_ = data;
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

// Reserve data and draw to surface via pointer
void Bitmap::Create(int width, int height, int depth, int size) {
  width_ = width;
  height_ = height;
  depth_ = depth;
  data_size_ = size;
  surface_ = SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                            SDL_PIXELFORMAT_INDEX8);
  // Default grayscale palette
  for (int i = 0; i < 8; i++) {
    surface_->format->palette->colors[i].r = i * 31;
    surface_->format->palette->colors[i].g = i * 31;
    surface_->format->palette->colors[i].b = i * 31;
  }

  pixel_data_ = (uchar *)SDL_malloc(size);
  surface_->pixels = pixel_data_;
}

void Bitmap::Create(int width, int height, int depth, uchar *data, int size, const SNESPalette &palette) {
  width_ = width;
  height_ = height;
  depth_ = depth;
  pixel_data_ = data;
  surface_ = SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                            SDL_PIXELFORMAT_INDEX8);
  surface_->format->palette = palette.GetSDL_Palette();
  surface_->pixels = pixel_data_;
}

void Bitmap::CreateTexture(std::shared_ptr<SDL_Renderer> renderer) {
  texture_ = SDL_CreateTextureFromSurface(renderer.get(), surface_);
}

void Bitmap::ApplyPalette(const SNESPalette &palette) { palette_ = palette; }

}  // namespace gfx
}  // namespace app
}  // namespace yaze
