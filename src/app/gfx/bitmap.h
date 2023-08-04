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
  Bitmap(int width, int height, int depth, uchar *data, int data_size);
  Bitmap(int width, int height, int depth, Bytes data);

  void Create(int width, int height, int depth, uchar *data);
  void Create(int width, int height, int depth, int data_size);
  void Create(int width, int height, int depth, uchar *data, int data_size);
  void Create(int width, int height, int depth, Bytes data);

  void Apply(Bytes data);

  void CreateTexture(std::shared_ptr<SDL_Renderer> renderer);
  void UpdateTexture(std::shared_ptr<SDL_Renderer> renderer);

  void ApplyPalette(const SNESPalette &palette);

  void WriteToPixel(int position, uchar value) {
    this->pixel_data_[position] = value;
  }

  int width() const { return width_; }
  int height() const { return height_; }
  auto size() const { return data_size_; }
  auto data() const { return pixel_data_; }
  auto at(int i) const { return pixel_data_[i]; }
  auto texture() const { return texture_.get(); }
  auto IsActive() const { return active_; }

 private:
  struct SDL_Texture_Deleter {
    void operator()(SDL_Texture *p) const {
      if (p != nullptr) {
        SDL_DestroyTexture(p);
        p = nullptr;
      }
    }
  };

  struct SDL_Surface_Deleter {
    void operator()(SDL_Surface *p) const {
      if (p != nullptr) {
        p->pixels = nullptr;
        SDL_FreeSurface(p);
        p = nullptr;
      }
    }
  };

  int width_ = 0;
  int height_ = 0;
  int depth_ = 0;
  int data_size_ = 0;
  bool freed_ = false;
  bool active_ = false;
  uchar *pixel_data_;
  Bytes data_;
  gfx::SNESPalette palette_;
  std::shared_ptr<SDL_Texture> texture_ = nullptr;
  std::shared_ptr<SDL_Surface> surface_ = nullptr;
};

using BitmapTable = std::unordered_map<int, gfx::Bitmap>;

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_BITMAP_H