#ifndef YAZE_APP_GFX_BITMAP_H
#define YAZE_APP_GFX_BITMAP_H

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

class Bitmap {
 public:
  Bitmap() = default;
  Bitmap(int width, int height, int depth, uchar *data);
  Bitmap(int width, int height, int depth, int data_size);

  void Create(int width, int height, int depth, uchar *data);
  void Create(int width, int height, int depth, int data_size);

  void CreateTexture(std::shared_ptr<SDL_Renderer> renderer);

  void ApplyPalette(SNESPalette &palette);

  absl::StatusOr<std::vector<Bitmap>> CreateTiles();
  absl::Status CreateFromTiles(const std::vector<Bitmap> &tiles);
  
  absl::Status WritePixel(int pos, uchar pixel);

  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }
  auto GetData() const { return pixel_data_; }
  auto GetTexture() const { return texture_.get(); }
  auto GetSurface() const { return surface_.get(); }

 private:
  struct sdl_deleter {
    void operator()(SDL_Texture *p) const { if (p) { SDL_DestroyTexture(p); p = nullptr; } }
    void operator()(SDL_Surface *p) const { if (p) { SDL_FreeSurface(p); p = nullptr;} }
  };

  int width_ = 0;
  int height_ = 0;
  int depth_ = 0;
  int data_size_ = 0;
  bool freed_ = false;
  uchar *pixel_data_;
  std::shared_ptr<SDL_Texture> texture_;
  std::shared_ptr<SDL_Surface> surface_;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_BITMAP_H