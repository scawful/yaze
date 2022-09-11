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

// Pass raw pixel data directly to the surface
void Bitmap::Create(int width, int height, int depth, uchar *data) {
  width_ = width;
  height_ = height;
  depth_ = depth;
  pixel_data_ = data;
  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width_, height_, depth_,
                                     SDL_PIXELFORMAT_INDEX8),
      SDL_Surface_Deleter());
  GrayscalePalette(surface_->format->palette);
  surface_->pixels = pixel_data_;
}

// Reserves data to later draw to surface via pointer
void Bitmap::Create(int width, int height, int depth, int size) {
  width_ = width;
  height_ = height;
  depth_ = depth;
  data_size_ = size;
  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                     SDL_PIXELFORMAT_INDEX8),
      SDL_Surface_Deleter());
  GrayscalePalette(surface_->format->palette);
  pixel_data_ = (uchar *)SDL_malloc(size);
  surface_->pixels = pixel_data_;
}

// Pass raw pixel data directly to the surface
void Bitmap::Create(int width, int height, int depth, uchar *data, int size) {
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

void Bitmap::Create(int width, int height, int depth, Bytes data) {
  width_ = width;
  height_ = height;
  depth_ = depth;
  data_ = data;
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

// Convert SNESPalette to SDL_Palette for surface.
void Bitmap::ApplyPalette(const SNESPalette &palette) {
  palette_ = palette;
  for (int i = 0; i < palette.size_; ++i) {
    if (palette.GetColor(i).transparent) {
      surface_->format->palette->colors[i].a = 0;
    } else {
      surface_->format->palette->colors[i].r = palette.GetColor(i).rgb.x;
      surface_->format->palette->colors[i].g = palette.GetColor(i).rgb.y;
      surface_->format->palette->colors[i].b = palette.GetColor(i).rgb.z;
      surface_->format->palette->colors[i].a = 255;
    }
  }
}

void Bitmap::SetPaletteColor(int id, gfx::SNESColor color) {
  surface_->format->palette->colors[id].r = color.rgb.x;
  surface_->format->palette->colors[id].g = color.rgb.y;
  surface_->format->palette->colors[id].b = color.rgb.z;
}

// Creates a vector of bitmaps which are individual 8x8 tiles.
absl::StatusOr<std::vector<Bitmap>> Bitmap::CreateTiles() {
  std::vector<Bitmap> tiles;
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 4; ++j) {
      Bitmap bmp;
      bmp.Create(8, 8, 8, 32);
      auto surface = bmp.GetSurface();
      SDL_Rect src_rect = {i, j, 8, 8};
      if (SDL_BlitSurface(surface_.get(), &src_rect, surface, nullptr) != 0)
        return absl::InternalError(
            absl::StrCat("Failed to blit surface: ", SDL_GetError()));
      tiles.push_back(bmp);
    }
  }
  return tiles;
}

// Converts a vector of 8x8 tiles into a tilesheet.
absl::Status Bitmap::CreateFromTiles(const std::vector<Bitmap> &tiles) {
  if (tiles.empty())
    return absl::InvalidArgumentError(
        "Failed to create bitmap: `tiles` is empty.");

  SDL_Rect tile_rect = {0, 0, 8, 8};
  SDL_Rect dest_rect = {0, 0, 8, 8};
  for (const auto &tile : tiles) {
    auto src = tile.GetSurface();
    if (SDL_BlitSurface(src, &tile_rect, surface_.get(), &dest_rect) != 0)
      return absl::InternalError(
          absl::StrCat("Failed to blit surface: ", SDL_GetError()));

    dest_rect.x++;
    if (dest_rect.x == 15) {
      dest_rect.x = 0;
      dest_rect.y++;
    }
  }

  return absl::OkStatus();
}

absl::Status Bitmap::WritePixel(int pos, uchar pixel) {
  if (!surface_) {
    return absl::InternalError("Surface not loaded");
  }
  auto pixels = (char *)surface_->pixels;
  pixels[pos] = pixel;
  return absl::OkStatus();
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze
