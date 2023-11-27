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
    InitializeFromData(width, height, depth, data);
  }

  // Function to create texture from pixel data
  void CreateTextureFromData() {
    active_ = true;
    surface_ = std::shared_ptr<SDL_Surface>(
        SDL_CreateRGBSurfaceWithFormat(0, width_, height_, depth_,
                                       SDL_PIXELFORMAT_INDEX8),
        SDL_Surface_Deleter());
    surface_->pixels = data_.data();
    data_size_ = data_.size();
  }

  void Create(int width, int height, int depth, int data_size);
  void Create(int width, int height, int depth, const Bytes &data);

  void InitializeFromData(uint32_t width, uint32_t height, uint32_t depth,
                          const Bytes &data);
  void ReserveData(uint32_t width, uint32_t height, uint32_t depth,
                   uint32_t size);

  void CreateTexture(std::shared_ptr<SDL_Renderer> renderer);
  void UpdateTexture(std::shared_ptr<SDL_Renderer> renderer);
  void CreateTexture(SDL_Renderer *renderer);
  void UpdateTexture(SDL_Renderer *renderer);

  void SaveSurfaceToFile(std::string_view filename);
  void SetSurface(SDL_Surface *surface);

  void ApplyPalette(const SNESPalette &palette);
  void ApplyPaletteWithTransparent(const SNESPalette &palette, int index);
  void ApplyPalette(const std::vector<SDL_Color> &palette);

  void WriteToPixel(int position, uchar value) {
    if (pixel_data_ == nullptr) {
      pixel_data_ = data_.data();
    }
    this->pixel_data_[position] = value;
    modified_ = true;
  }

  void WriteWordToPixel(int position, uint16_t value) {
    if (pixel_data_ == nullptr) {
      pixel_data_ = data_.data();
    }
    this->pixel_data_[position] = value & 0xFF;
    this->pixel_data_[position + 1] = (value >> 8) & 0xFF;
    modified_ = true;
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
  auto depth() const { return depth_; }
  auto size() const { return data_size_; }
  auto data() const { return data_.data(); }
  auto mutable_data() { return data_; }
  auto mutable_pixel_data() { return pixel_data_; }

  auto vector() const { return data_; }
  auto at(int i) const { return data_[i]; }
  auto texture() const { return texture_.get(); }
  auto modified() const { return modified_; }
  void set_modified(bool modified) { modified_ = modified; }
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
  bool modified_ = false;
  void *texture_pixels = nullptr;
  uchar *pixel_data_;
  Bytes data_;
  gfx::SNESPalette palette_;
  std::shared_ptr<SDL_Texture> texture_ = nullptr;
  std::shared_ptr<SDL_Surface> surface_ = nullptr;
  std::shared_ptr<SDL_Surface> converted_surface_ = nullptr;
};

using BitmapTable = std::unordered_map<int, gfx::Bitmap>;

class BitmapManager {
 private:
  std::unordered_map<int, std::shared_ptr<gfx::Bitmap>> bitmap_cache_;

 public:
  void LoadBitmap(int id, const Bytes &data, int width, int height, int depth) {
    bitmap_cache_[id] =
        std::make_shared<gfx::Bitmap>(width, height, depth, data);
  }

  std::shared_ptr<gfx::Bitmap> const &CopyBitmap(const gfx::Bitmap &bitmap,
                                                 int id) {
    auto new_bitmap = std::make_shared<gfx::Bitmap>(
        bitmap.width(), bitmap.height(), bitmap.depth(), bitmap.vector());
    bitmap_cache_[id] = new_bitmap;
    return new_bitmap;
  }

  std::shared_ptr<gfx::Bitmap> const &operator[](int id) {
    auto it = bitmap_cache_.find(id);
    if (it != bitmap_cache_.end()) {
      return it->second;
    }
    return nullptr;
  }

  auto mutable_bitmap(int id) { return bitmap_cache_[id]; }

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

  std::shared_ptr<gfx::Bitmap> const &GetBitmap(int id) {
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