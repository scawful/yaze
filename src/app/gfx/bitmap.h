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

bool ConvertSurfaceToPNG(SDL_Surface *surface, std::vector<uint8_t> &buffer);
void ConvertPngToSurface(const std::vector<uint8_t> &png_data,
                         SDL_Surface **outSurface);
class Bitmap {
 public:
  Bitmap() = default;

  Bitmap(int width, int height, int depth, int data_size);
  Bitmap(int width, int height, int depth, const Bytes &data)
      : width_(width), height_(height), depth_(depth), data_(data) {
    InitializeFromData(width, height, depth, data);
  }

  void Create(int width, int height, int depth, int data_size);
  void Create(int width, int height, int depth, const Bytes &data);

  void InitializeFromData(uint32_t width, uint32_t height, uint32_t depth,
                          const Bytes &data);

  void CreateTexture(std::shared_ptr<SDL_Renderer> renderer);
  void UpdateTexture(std::shared_ptr<SDL_Renderer> renderer);
  void CreateTexture(SDL_Renderer *renderer);
  void UpdateTexture(SDL_Renderer *renderer, bool use_sdl_update = false);

  void SaveSurfaceToFile(std::string_view filename);
  void SetSurface(SDL_Surface *surface);
  std::vector<uint8_t> GetPngData();
  void LoadFromPngData(const std::vector<uint8_t> &png_data, int width,
                       int height);

  void ApplyPalette(const SnesPalette &palette);
  void ApplyPaletteWithTransparent(const SnesPalette &palette, int index,
                                   int length = 7);
  void ApplyPalette(const std::vector<SDL_Color> &palette);
  void ApplyPaletteFromPaletteGroup(const SnesPalette &palette, int palette_id);

  void WriteToPixel(int position, uchar value) {
    if (pixel_data_ == nullptr) {
      pixel_data_ = data_.data();
    }
    pixel_data_[position] = value;
    modified_ = true;
  }

  void WriteWordToPixel(int position, uint16_t value) {
    if (pixel_data_ == nullptr) {
      pixel_data_ = data_.data();
    }
    pixel_data_[position] = value & 0xFF;
    pixel_data_[position + 1] = (value >> 8) & 0xFF;
    modified_ = true;
  }

  void Get8x8Tile(int tile_index, int x, int y, std::vector<uint8_t> &tile_data,
                  int &tile_data_offset) {
    int tile_offset = tile_index * (width_ * height_);
    int tile_x = (x * 8) % width_;
    int tile_y = (y * 8) % height_;
    for (int i = 0; i < 8; i++) {
      int row_offset = tile_offset + ((tile_y + i) * width_);
      for (int j = 0; j < 8; j++) {
        int pixel_offset = row_offset + (tile_x + j);
        int pixel_value = data_[pixel_offset];
        tile_data[tile_data_offset] = pixel_value;
        tile_data_offset++;
      }
    }
  }

  void Get16x16Tile(int tile_index, int x, int y,
                    std::vector<uint8_t> &tile_data, int &tile_data_offset) {
    int tile_offset = tile_index * (width_ * height_);
    int tile_x = x * 16;
    int tile_y = y * 16;
    for (int i = 0; i < 16; i++) {
      int row_offset = tile_offset + ((i / 8) * (width_ * 8));
      for (int j = 0; j < 16; j++) {
        int pixel_offset =
            row_offset + ((j / 8) * 8) + ((i % 8) * width_) + (j % 8);
        int pixel_value = data_[pixel_offset];
        tile_data[tile_data_offset] = pixel_value;
        tile_data_offset++;
      }
    }
  }

  void Get16x16Tile(int tile_x, int tile_y, std::vector<uint8_t> &tile_data,
                    int &tile_data_offset) {
    // Assuming 'width_' and 'height_' are the dimensions of the bitmap
    // and 'data_' is the bitmap data.
    for (int ty = 0; ty < 16; ty++) {
      for (int tx = 0; tx < 16; tx++) {
        // Calculate the pixel position in the bitmap
        int pixel_x = tile_x + tx;
        int pixel_y = tile_y + ty;
        int pixel_offset = pixel_y * width_ + pixel_x;
        int pixel_value = data_[pixel_offset];

        // Store the pixel value in the tile data
        tile_data[tile_data_offset++] = pixel_value;
      }
    }
  }

  void WriteColor(int position, const ImVec4 &color) {
    // Convert ImVec4 (RGBA) to SDL_Color (RGBA)
    SDL_Color sdl_color;
    sdl_color.r = static_cast<Uint8>(color.x * 255);
    sdl_color.g = static_cast<Uint8>(color.y * 255);
    sdl_color.b = static_cast<Uint8>(color.z * 255);
    sdl_color.a = static_cast<Uint8>(color.w * 255);

    // Map SDL_Color to the nearest color index in the surface's palette
    Uint8 index =
        SDL_MapRGB(surface_->format, sdl_color.r, sdl_color.g, sdl_color.b);

    // Write the color index to the pixel data
    pixel_data_[position] = index;
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

  auto sdl_palette() {
    if (surface_ == nullptr) {
      throw std::runtime_error("Surface is null.");
    }
    return surface_->format->palette;
  }
  auto palette() const { return palette_; }
  auto palette_size() const { return palette_.size(); }

  int width() const { return width_; }
  int height() const { return height_; }
  auto depth() const { return depth_; }
  auto size() const { return data_size_; }
  auto data() const { return data_.data(); }
  auto &mutable_data() { return data_; }
  auto mutable_pixel_data() { return pixel_data_; }
  auto surface() const { return surface_.get(); }
  auto mutable_surface() { return surface_.get(); }
  auto converted_surface() const { return converted_surface_.get(); }
  auto mutable_converted_surface() { return converted_surface_.get(); }
  void set_data(const Bytes &data) { data_ = data; }

  auto vector() const { return data_; }
  auto at(int i) const { return data_[i]; }
  auto texture() const { return texture_.get(); }
  auto modified() const { return modified_; }
  void set_modified(bool modified) { modified_ = modified; }
  auto is_active() const { return active_; }
  auto set_active(bool active) { active_ = active; }

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

  std::vector<uint8_t> png_data_;

  gfx::SnesPalette palette_;
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

  std::shared_ptr<gfx::Bitmap> const &operator[](int id) {
    auto it = bitmap_cache_.find(id);
    if (it != bitmap_cache_.end()) {
      return it->second;
    }
    throw std::runtime_error(
        absl::StrCat("Bitmap with id ", id, " not found."));
  }
  std::shared_ptr<gfx::Bitmap> const &shared_bitmap(int id) {
    auto it = bitmap_cache_.find(id);
    if (it != bitmap_cache_.end()) {
      return it->second;
    }
    throw std::runtime_error(
        absl::StrCat("Bitmap with id ", id, " not found."));
  }
  auto mutable_bitmap(int id) { return bitmap_cache_[id]; }
  void clear_cache() { bitmap_cache_.clear(); }

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
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_BITMAP_H