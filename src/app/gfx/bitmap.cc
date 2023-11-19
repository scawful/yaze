#include "bitmap.h"

#include <SDL.h>

#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "app/core/constants.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {
namespace gfx {

namespace {
void GrayscalePalette(SDL_Palette *palette) {
  for (int i = 0; i < 8; i++) {
    palette->colors[i].r = i * 31;
    palette->colors[i].g = i * 31;
    palette->colors[i].b = i * 31;
  }
}
}  // namespace

Bitmap::Bitmap(int width, int height, int depth, uchar *data) {
  Create(width, height, depth, data);
}

Bitmap::Bitmap(int width, int height, int depth, int data_size) {
  Create(width, height, depth, data_size);
}

Bitmap::Bitmap(int width, int height, int depth, uchar *data, int data_size) {
  Create(width, height, depth, data, data_size);
}

// Bitmap::Bitmap(int width, int height, int depth, Bytes data) {
//   Create(width, height, depth, data);
// }

// Pass raw pixel data directly to the surface
void Bitmap::Create(int width, int height, int depth, uchar *data) {
  active_ = true;
  width_ = width;
  height_ = height;
  depth_ = depth;
  pixel_data_ = data;
  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width_, height_, depth_,
                                     SDL_PIXELFORMAT_INDEX8),
      SDL_Surface_Deleter());
  surface_->pixels = pixel_data_;
  GrayscalePalette(surface_->format->palette);
}

// Reserves data to later draw to surface via pointer
void Bitmap::Create(int width, int height, int depth, int size) {
  active_ = true;
  width_ = width;
  height_ = height;
  depth_ = depth;
  data_size_ = size;
  data_.reserve(size);
  pixel_data_ = data_.data();
  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                     SDL_PIXELFORMAT_INDEX8),
      SDL_Surface_Deleter());
  surface_->pixels = pixel_data_;
  GrayscalePalette(surface_->format->palette);
}

// Pass raw pixel data directly to the surface
void Bitmap::Create(int width, int height, int depth, uchar *data, int size) {
  active_ = true;
  width_ = width;
  height_ = height;
  depth_ = depth;
  pixel_data_ = data;
  data_size_ = size;
  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width_, height_, depth_,
                                     SDL_PIXELFORMAT_INDEX8),
      SDL_Surface_Deleter());
  surface_->pixels = pixel_data_;
  GrayscalePalette(surface_->format->palette);
}

void Bitmap::Create(int width, int height, int depth, const Bytes &data) {
  active_ = true;
  width_ = width;
  height_ = height;
  depth_ = depth;
  data_ = std::move(data);
  pixel_data_ = data_.data();
  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width_, height_, depth_,
                                     SDL_PIXELFORMAT_INDEX8),
      SDL_Surface_Deleter());
  surface_->pixels = pixel_data_;
  GrayscalePalette(surface_->format->palette);
}

// Creates the texture that will be displayed to the screen.
void Bitmap::CreateTexture(std::shared_ptr<SDL_Renderer> renderer) {
  texture_ = std::shared_ptr<SDL_Texture>{
      SDL_CreateTextureFromSurface(renderer.get(), surface_.get()),
      SDL_Texture_Deleter{}};
}

void Bitmap::UpdateTexture(std::shared_ptr<SDL_Renderer> renderer) {
  SDL_DestroyTexture(texture_.get());
  texture_ = nullptr;
  texture_ = std::shared_ptr<SDL_Texture>{
      SDL_CreateTextureFromSurface(renderer.get(), surface_.get()),
      SDL_Texture_Deleter{}};
}

void Bitmap::SaveSurfaceToFile(std::string_view filename) {
  SDL_SaveBMP(surface_.get(), filename.data());
}

void Bitmap::SetSurface(SDL_Surface *surface) {
  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      surface, SDL_Surface_Deleter());
}

// Convert SNESPalette to SDL_Palette for surface.
void Bitmap::ApplyPalette(const SNESPalette &palette) {
  palette_ = palette;
  SDL_UnlockSurface(surface_.get());
  for (int i = 0; i < palette.size(); ++i) {
    if (palette.GetColor(i).IsTransparent()) {
      surface_->format->palette->colors[i].r = 0;
      surface_->format->palette->colors[i].g = 0;
      surface_->format->palette->colors[i].b = 0;
      surface_->format->palette->colors[i].a = 0;
    } else {
      surface_->format->palette->colors[i].r = palette.GetColor(i).GetRGB().x;
      surface_->format->palette->colors[i].g = palette.GetColor(i).GetRGB().y;
      surface_->format->palette->colors[i].b = palette.GetColor(i).GetRGB().z;
      surface_->format->palette->colors[i].a = palette.GetColor(i).GetRGB().w;
    }
  }
  SDL_LockSurface(surface_.get());
}

void Bitmap::ApplyPalette(const std::vector<SDL_Color> &palette) {
  SDL_UnlockSurface(surface_.get());
  for (int i = 0; i < palette.size(); ++i) {
    surface_->format->palette->colors[i].r = palette[i].r;
    surface_->format->palette->colors[i].g = palette[i].g;
    surface_->format->palette->colors[i].b = palette[i].b;
    surface_->format->palette->colors[i].a = palette[i].a;
  }
  SDL_LockSurface(surface_.get());
}

absl::Status Bitmap::InitializeFromData(uint32_t width, uint32_t height,
                                        uint32_t depth, const Bytes &data) {
  if (width == 0 || height == 0 || depth == 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid arguments: width: ", width, ", height: ", height,
                     ", depth: ", depth));
  }

  active_ = true;
  width_ = width;
  height_ = height;
  depth_ = depth;
  data_ = data;

  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width_, height_, depth_,
                                     SDL_PIXELFORMAT_INDEX8),
      SDL_Surface_Deleter());

  if (surface_ == nullptr) {
    return absl::InternalError("Failed to create surface.");
  }

  surface_->pixels = data_.data();
  GrayscalePalette(surface_->format->palette);
  return absl::OkStatus();
}

void Bitmap::ReserveData(uint32_t width, uint32_t height, uint32_t depth,
                         uint32_t size) {
  width_ = width;
  height_ = height;
  depth_ = depth;
  data_.reserve(size);
  pixel_data_ = data_.data();
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze
