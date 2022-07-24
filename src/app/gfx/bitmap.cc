#include "bitmap.h"

#include <SDL2/SDL.h>

#include <memory>

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

Bitmap::Bitmap(int width, int height, int depth, uchar *data)
    : width_(width), height_(height), depth_(depth), pixel_data_(data) {
  surface_ = std::unique_ptr<SDL_Surface, sdl_deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                     SDL_PIXELFORMAT_INDEX8),
      sdl_deleter());
  GrayscalePalette(surface_->format->palette);
  surface_->pixels = data;
}

Bitmap::Bitmap(int width, int height, int depth, int data_size)
    : width_(width), height_(height), depth_(depth), data_size_(data_size) {
  surface_ = std::unique_ptr<SDL_Surface, sdl_deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                     SDL_PIXELFORMAT_INDEX8),
      sdl_deleter());
  GrayscalePalette(surface_->format->palette);
  pixel_data_ = (uchar *)SDL_malloc(data_size);
  surface_->pixels = pixel_data_;
}

// Pass raw pixel data directly to the surface
// Be sure to know what the hell you're doing if you use this one.
void Bitmap::Create(int width, int height, int depth, uchar *data) {
  width_ = width;
  height_ = height;
  depth_ = depth;
  pixel_data_ = data;
  surface_ = std::unique_ptr<SDL_Surface, sdl_deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                     SDL_PIXELFORMAT_INDEX8),
      sdl_deleter());
  GrayscalePalette(surface_->format->palette);

  surface_->pixels = pixel_data_;
}

// Reserve data and draw to surface via pointer
void Bitmap::Create(int width, int height, int depth, int size) {
  width_ = width;
  height_ = height;
  depth_ = depth;
  data_size_ = size;
  surface_ = std::unique_ptr<SDL_Surface, sdl_deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                     SDL_PIXELFORMAT_INDEX8),
      sdl_deleter());
  GrayscalePalette(surface_->format->palette);
  pixel_data_ = (uchar *)SDL_malloc(size);
  surface_->pixels = pixel_data_;
}

void Bitmap::Create(int width, int height, int depth, uchar *data, int size,
                    SNESPalette &palette) {
  width_ = width;
  height_ = height;
  depth_ = depth;
  pixel_data_ = data;
  surface_ = std::unique_ptr<SDL_Surface, sdl_deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                     SDL_PIXELFORMAT_INDEX8),
      sdl_deleter());
  surface_->format->palette = palette.GetSDL_Palette();
  surface_->pixels = pixel_data_;
}

void Bitmap::CreateTexture(std::shared_ptr<SDL_Renderer> renderer) {
  texture_ = std::unique_ptr<SDL_Texture, sdl_deleter>(
      SDL_CreateTextureFromSurface(renderer.get(), surface_.get()),
      sdl_deleter());
}

void Bitmap::ApplyPalette(const SNESPalette &palette) { palette_ = palette; }

absl::StatusOr<std::vector<Bitmap>> Bitmap::CreateTiles() {
  std::vector<Bitmap> tiles;
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 4; ++j) {
      Bitmap bmp;
      bmp.Create(8, 8, 8, 32);
      auto surface = bmp.GetSurface();
      SDL_Rect src_rect = {i, j, 8, 8};
      if (SDL_BlitSurface(surface_.get(), &src_rect, surface, nullptr) != 0) {
        return absl::InvalidArgumentError(
            "Failed to blit surface for Bitmap Tilesheet");
      }
      tiles.push_back(bmp);
    }
  }
  return tiles;
}

absl::Status Bitmap::CreateFromTiles(const std::vector<Bitmap> &tiles) {
  if (tiles.empty()) {
    return absl::InvalidArgumentError("Empty tiles");
  }

  SDL_Rect tile_rect = {0, 0, 8, 8};
  SDL_Rect dest_rect = {0, 0, 8, 8};
  for (const auto &tile : tiles) {
    auto src = tile.GetSurface();
    if (SDL_BlitSurface(src, &tile_rect, surface_.get(), &dest_rect) != 0) {
      return absl::InvalidArgumentError(
          "Failed to blit surface for Bitmap Tilesheet");
    }

    dest_rect.x++;
    if (dest_rect.x == 15) {
      dest_rect.x = 0;
      dest_rect.y++;
    }
  }

  return absl::OkStatus();
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze
