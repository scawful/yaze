#ifndef YAZE_APP_GFX_BITMAP_H
#define YAZE_APP_GFX_BITMAP_H

#include <SDL2/SDL.h>

#include <memory>

#include "absl/status/statusor.h"
#include "app/core/constants.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {
namespace gfx {

class Bitmap {
 public:
  Bitmap() = default;
  Bitmap(int width, int height, int depth, uchar *data);
  Bitmap(int width, int height, int depth, int data_size);

  void Create(int width, int height, int depth, uchar *data);
  void Create(int width, int height, int depth, int data_size);
  void Create(int width, int height, int depth, uchar *data, int size,
              SNESPalette &palette);
  void CreateTexture(std::shared_ptr<SDL_Renderer> renderer);

  void ApplyPalette(const SNESPalette &palette);

  absl::StatusOr<std::vector<Bitmap>> CreateTiles();

  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }
  auto GetData() const { return pixel_data_; }
  auto GetTexture() const { return texture_; }
  auto GetSurface() const { return surface_; }

 private:
  int width_ = 0;
  int height_ = 0;
  int depth_ = 0;
  int data_size_ = 0;
  uchar *pixel_data_;
  SDL_Surface *surface_;
  SDL_Texture *texture_;
  SNESPalette palette_;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_BITMAP_H