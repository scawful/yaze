#ifndef YAZE_APP_GFX_BITMAP_H
#define YAZE_APP_GFX_BITMAP_H

#include <SDL.h>

#include <cstdint>
#include <memory>

#include "absl/container/flat_hash_map.h"
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

  Bitmap(int width, int height, int depth, int data_size);
  Bitmap(int width, int height, int depth, const Bytes &data)
      : width_(width), height_(height), depth_(depth), data_(data) {
    CreateTextureFromData();
  }

  // Function to create texture from pixel data
  void CreateTextureFromData() {
    // Safely create the texture from the raw data
    // Assuming a function exists that converts Bytes to the appropriate format
    auto raw_pixel_data = data_;
    surface_ = std::shared_ptr<SDL_Surface>(
        SDL_CreateRGBSurfaceWithFormat(0, width_, height_, depth_,
                                       SDL_PIXELFORMAT_INDEX8),
        SDL_Surface_Deleter());
    surface_->pixels = data_.data();
  }

  [[deprecated]] Bitmap(int width, int height, int depth, uchar *data);
  [[deprecated]] Bitmap(int width, int height, int depth, uchar *data,
                        int data_size);

  void Create(int width, int height, int depth, int data_size);
  void Create(int width, int height, int depth, const Bytes &data);

  absl::Status InitializeFromData(uint32_t width, uint32_t height,
                                  uint32_t depth, const Bytes &data);
  void ReserveData(uint32_t width, uint32_t height, uint32_t depth,
                   uint32_t size);

  [[deprecated]] void Create(int width, int height, int depth, uchar *data);
  [[deprecated]] void Create(int width, int height, int depth, uchar *data,
                             int data_size);

  void CreateTexture(std::shared_ptr<SDL_Renderer> renderer);
  void UpdateTexture(std::shared_ptr<SDL_Renderer> renderer);

  void SaveSurfaceToFile(std::string_view filename);
  void SetSurface(SDL_Surface *surface);

  void ApplyPalette(const SNESPalette &palette);
  void ApplyPalette(const std::vector<SDL_Color> &palette);

  void WriteToPixel(int position, uchar value) {
    this->pixel_data_[position] = value;
  }

  void Cleanup() {
    // Reset texture_
    if (texture_) {
      texture_.reset();
    }

    // Reset surface_ and its pixel data
    if (surface_) {
      surface_->pixels = nullptr;
      surface_.reset();
    }

    // Reset data_
    data_.clear();

    // Reset other members if necessary
    active_ = false;
    pixel_data_ = nullptr;
    width_ = 0;
    height_ = 0;
    depth_ = 0;
    data_size_ = 0;
    palette_.Clear();
  }

  int width() const { return width_; }
  int height() const { return height_; }
  auto size() const { return data_size_; }
  auto data() const { return pixel_data_; }
  auto at(int i) const { return pixel_data_[i]; }
  auto texture() const { return texture_.get(); }
  auto IsActive() const { return active_; }
  auto SetActive(bool active) { active_ = active; }

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

class BitmapManager {
 private:
  std::unordered_map<int, std::shared_ptr<gfx::Bitmap>> bitmap_cache_;

 public:
  std::shared_ptr<gfx::Bitmap> LoadBitmap(int id, const Bytes &data, int width,
                                          int height, int depth) {
    auto bitmap = std::make_shared<gfx::Bitmap>(width, height, depth, data);
    bitmap_cache_[id] = bitmap;
    return bitmap;
  }

  std::shared_ptr<gfx::Bitmap> operator[](int id) {
    auto it = bitmap_cache_.find(id);
    if (it != bitmap_cache_.end()) {
      return it->second;
    }
    return nullptr;
  }

  using value_type = std::pair<const int, std::shared_ptr<gfx::Bitmap>>;
  using iterator =
      std::unordered_map<int, std::shared_ptr<gfx::Bitmap>>::iterator;
  using const_iterator =
      std::unordered_map<int, std::shared_ptr<gfx::Bitmap>>::const_iterator;

  iterator begin() noexcept { return bitmap_cache_.begin(); }
  iterator end() noexcept { return bitmap_cache_.end(); }
  const_iterator begin() const noexcept { return bitmap_cache_.begin(); }
  const_iterator end() const noexcept { return bitmap_cache_.end(); }
  const_iterator cbegin() const noexcept { return bitmap_cache_.cbegin(); }
  const_iterator cend() const noexcept { return bitmap_cache_.cend(); }

  std::shared_ptr<gfx::Bitmap> GetBitmap(int id) {
    auto it = bitmap_cache_.find(id);
    if (it != bitmap_cache_.end()) {
      return it->second;
    }
    return nullptr;  // or handle the error accordingly
  }

  void ClearCache() { bitmap_cache_.clear(); }
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_BITMAP_H