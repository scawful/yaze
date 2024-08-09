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
#include "app/core/utils/sdl_deleter.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {

/**
 * @namespace yaze::app::gfx
 * @brief Contains classes for handling graphical data.
 */
namespace gfx {

// Same as SDL_PIXELFORMAT_INDEX8 for reference
constexpr Uint32 SNES_PIXELFORMAT_INDEXED =
    SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX8, 0, 0, 8, 1);

constexpr Uint32 SNES_PIXELFORMAT_2BPP = SDL_DEFINE_PIXELFORMAT(
    /*type=*/SDL_PIXELTYPE_INDEX8, /*order=*/0,
    /*layouts=*/0, /*bits=*/2, /*bytes=*/1);

constexpr Uint32 SNES_PIXELFORMAT_4BPP = SDL_DEFINE_PIXELFORMAT(
    /*type=*/SDL_PIXELTYPE_INDEX8, /*order=*/0,
    /*layouts=*/0, /*bits=*/4, /*bytes=*/1);

constexpr Uint32 SNES_PIXELFORMAT_8BPP = SDL_DEFINE_PIXELFORMAT(
    /*type=*/SDL_PIXELTYPE_INDEX8, /*order=*/0,
    /*layouts=*/0, /*bits=*/8, /*bytes=*/1);

enum BitmapFormat {
  kIndexed = 0,
  k2bpp = 1,
  k4bpp = 2,
  k8bpp = 3,
};

/**
 * @brief Convert SDL_Surface to PNG image data.
 */
bool ConvertSurfaceToPNG(SDL_Surface *surface, std::vector<uint8_t> &buffer);

/**
 * @brief Convert PNG image data to SDL_Surface.
 */
void ConvertPngToSurface(const std::vector<uint8_t> &png_data,
                         SDL_Surface **outSurface);

/**
 * @brief Represents a bitmap image.
 *
 * The `Bitmap` class provides functionality to create, manipulate, and display
 * bitmap images. It supports various operations such as creating a bitmap
 * object, creating and updating textures, applying palettes, and accessing
 * pixel data.
 */
class Bitmap {
 public:
  Bitmap() = default;

  Bitmap(int width, int height, int depth, int data_size);
  Bitmap(int width, int height, int depth, const Bytes &data)
      : width_(width), height_(height), depth_(depth), data_(data) {
    Create(width, height, depth, data);
  }
  Bitmap(int width, int height, int depth, const Bytes &data,
         const SnesPalette &palette)
      : width_(width),
        height_(height),
        depth_(depth),
        data_(data),
        palette_(palette) {
    Create(width, height, depth, data);
    if (!ApplyPalette(palette).ok()) {
      std::cerr << "Error applying palette in bitmap constructor." << std::endl;
    }
  }

  /**
   * @brief Creates a bitmap object with the provided graphical data.
   */
  void Create(int width, int height, int depth, const Bytes &data);
  void Create(int width, int height, int depth, int format, const Bytes &data);

  void Reformat(int format);

  /**
   * @brief Creates the underlying SDL_Texture to be displayed.
   *
   * Converts the surface from a RGB to ARGB format.
   * Uses SDL_TEXTUREACCESS_STREAMING to allow for live updates.
   */
  void CreateTexture(std::shared_ptr<SDL_Renderer> renderer);

  /**
   * @brief Updates the underlying SDL_Texture when it already exists.
   */
  void UpdateTexture(std::shared_ptr<SDL_Renderer> renderer);
  void CreateTexture(SDL_Renderer *renderer);
  void UpdateTexture(SDL_Renderer *renderer, bool use_sdl_update = false);

  void SaveSurfaceToFile(std::string_view filename);
  void SetSurface(SDL_Surface *surface);
  std::vector<uint8_t> GetPngData();
  void LoadFromPngData(const std::vector<uint8_t> &png_data, int width,
                       int height);

  /**
   * @brief Copy color data from the SnesPalette into the SDL_Palette
   */
  absl::Status ApplyPalette(const SnesPalette &palette);
  absl::Status ApplyPaletteWithTransparent(const SnesPalette &palette,
                                           int index, int length = 7);
  void ApplyPalette(const std::vector<SDL_Color> &palette);
  absl::Status ApplyPaletteFromPaletteGroup(const SnesPalette &palette,
                                            int palette_id);

  void Get8x8Tile(int tile_index, int x, int y, std::vector<uint8_t> &tile_data,
                  int &tile_data_offset);

  void Get16x16Tile(int tile_index, int x, int y,
                    std::vector<uint8_t> &tile_data, int &tile_data_offset);
  void Get16x16Tile(int tile_x, int tile_y, std::vector<uint8_t> &tile_data,
                    int &tile_data_offset);

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

  void WriteColor(int position, const ImVec4 &color);

  void Cleanup() {
    active_ = false;
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
  auto mutable_palette() { return &palette_; }
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

  auto vector() const { return data_; }
  auto at(int i) const { return data_[i]; }
  auto texture() const { return texture_.get(); }
  auto modified() const { return modified_; }
  auto is_active() const { return active_; }
  void set_active(bool active) { active_ = active; }
  void set_data(const Bytes &data) { data_ = data; }
  void set_modified(bool modified) { modified_ = modified; }

 private:
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

/**
 * @brief Hash map container of shared pointers to Bitmaps.
 * \deprecated Moved to fixed array or vector for storing groups of bitmaps.
 */
class BitmapManager {
 private:
  std::unordered_map<int, gfx::Bitmap> bitmap_cache_;

 public:
  void LoadBitmap(int id, const Bytes &data, int width, int height, int depth) {
    bitmap_cache_[id].Create(width, height, depth, data);
  }

  gfx::Bitmap &operator[](int id) {
    auto it = bitmap_cache_.find(id);
    if (it != bitmap_cache_.end()) {
      return it->second;
    }
    return bitmap_cache_.begin()->second;
  }
  gfx::Bitmap &shared_bitmap(int id) {
    auto it = bitmap_cache_.find(id);
    if (it != bitmap_cache_.end()) {
      return it->second;
    }
    throw std::runtime_error(
        absl::StrCat("Bitmap with id ", id, " not found."));
  }
  auto mutable_bitmap(int id) { return &bitmap_cache_[id]; }
  void clear_cache() { bitmap_cache_.clear(); }
  auto size() const { return bitmap_cache_.size(); }
  auto at(int id) const { return bitmap_cache_.at(id); }

  using value_type = std::pair<const int, gfx::Bitmap>;
  using iterator = std::unordered_map<int, gfx::Bitmap>::iterator;
  using const_iterator = std::unordered_map<int, gfx::Bitmap>::const_iterator;

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
