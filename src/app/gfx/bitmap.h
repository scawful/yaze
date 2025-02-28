#ifndef YAZE_APP_GFX_BITMAP_H
#define YAZE_APP_GFX_BITMAP_H

#include <SDL.h>

#include <span>
#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "app/core/platform/sdl_deleter.h"
#include "app/gfx/snes_palette.h"
#include "util/macro.h"

namespace yaze {

/**
 * @namespace yaze::gfx
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

#if YAZE_LIB_PNG == 1
/**
 * @brief Convert SDL_Surface to PNG image data.
 */
bool ConvertSurfaceToPng(SDL_Surface *surface, std::vector<uint8_t> &buffer);

/**
 * @brief Convert PNG image data to SDL_Surface.
 */
void ConvertPngToSurface(const std::vector<uint8_t> &png_data,
                         SDL_Surface **outSurface);
#endif

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
  Bitmap(int width, int height, int depth, const std::vector<uint8_t> &data)
      : width_(width), height_(height), depth_(depth), data_(data) {
    Create(width, height, depth, data);
  }
  Bitmap(int width, int height, int depth, const std::vector<uint8_t> &data,
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

#if YAZE_LIB_PNG == 1
  std::vector<uint8_t> GetPngData();
#endif

  void SaveSurfaceToFile(std::string_view filename);

  void Initialize(int width, int height, int depth, std::span<uint8_t> &data);

  void Create(int width, int height, int depth, int data_size) {
    Create(width, height, depth, std::vector<uint8_t>(data_size, 0));
  }
  void Create(int width, int height, int depth, std::span<uint8_t> data);
  void Create(int width, int height, int depth,
              const std::vector<uint8_t> &data);
  void Create(int width, int height, int depth, int format,
              const std::vector<uint8_t> &data);

  void Reformat(int format);

  /**
   * @brief Creates the underlying SDL_Texture to be displayed.
   *
   * Converts the surface from a RGB to ARGB format.
   * Uses SDL_TEXTUREACCESS_STREAMING to allow for live updates.
   */
  void CreateTexture(SDL_Renderer *renderer);

  /**
   * @brief Updates the underlying SDL_Texture when it already exists.
   */
  void UpdateTexture(SDL_Renderer *renderer);

  /**
   * @brief Copy color data from the SnesPalette into the SDL_Palette
   */
  absl::Status ApplyPalette(const SnesPalette &palette);
  absl::Status ApplyPaletteWithTransparent(const SnesPalette &palette,
                                           size_t index, int length = 7);
  void ApplyPalette(const std::vector<SDL_Color> &palette);
  absl::Status ApplyPaletteFromPaletteGroup(const SnesPalette &palette,
                                            int palette_id);

  void Get8x8Tile(int tile_index, int x, int y, std::vector<uint8_t> &tile_data,
                  int &tile_data_offset);

  void Get16x16Tile(int tile_x, int tile_y, std::vector<uint8_t> &tile_data,
                    int &tile_data_offset);

  void WriteToPixel(int position, uint8_t value) {
    if (pixel_data_ == nullptr) {
      pixel_data_ = data_.data();
    }
    pixel_data_[position] = value;
    modified_ = true;
  }

  void WriteColor(int position, const ImVec4 &color);

  void Cleanup() {
    active_ = false;
    width_ = 0;
    height_ = 0;
    depth_ = 0;
    data_size_ = 0;
    palette_.clear();
  }

  auto palette() const { return palette_; }
  auto mutable_palette() { return &palette_; }

  int width() const { return width_; }
  int height() const { return height_; }
  auto depth() const { return depth_; }
  auto size() const { return data_size_; }
  auto data() const { return data_.data(); }
  auto &mutable_data() { return data_; }
  auto surface() const { return surface_.get(); }

  auto vector() const { return data_; }
  auto at(int i) const { return data_[i]; }
  auto texture() const { return texture_.get(); }
  auto modified() const { return modified_; }
  auto is_active() const { return active_; }
  void set_active(bool active) { active_ = active; }
  void set_data(const std::vector<uint8_t> &data) { data_ = data; }
  void set_modified(bool modified) { modified_ = modified; }

 private:
  int width_ = 0;
  int height_ = 0;
  int depth_ = 0;
  int data_size_ = 0;

  bool active_ = false;
  bool modified_ = false;
  void *texture_pixels = nullptr;

  uint8_t *pixel_data_ = nullptr;
  std::vector<uint8_t> data_;

  gfx::SnesPalette palette_;
  std::shared_ptr<SDL_Texture> texture_ = nullptr;
  std::shared_ptr<SDL_Surface> surface_ = nullptr;
};

using BitmapTable = std::unordered_map<int, gfx::Bitmap>;

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_BITMAP_H
