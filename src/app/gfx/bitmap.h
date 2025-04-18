#ifndef YAZE_APP_GFX_BITMAP_H
#define YAZE_APP_GFX_BITMAP_H

#include <SDL.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "app/gfx/snes_palette.h"

namespace yaze {

/**
 * @namespace yaze::gfx
 * @brief Contains classes for handling graphical data.
 */
namespace gfx {

// Pixel format constants
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
  // Constructors
  Bitmap() = default;

  /**
   * @brief Create a bitmap with the given dimensions and data
   */
  Bitmap(int width, int height, int depth, const std::vector<uint8_t> &data);

  /**
   * @brief Create a bitmap with the given dimensions, data, and palette
   */
  Bitmap(int width, int height, int depth, const std::vector<uint8_t> &data,
         const SnesPalette &palette);

  /**
   * @brief Initialize the bitmap with the given dimensions and data
   */
  void Initialize(int width, int height, int depth, std::span<uint8_t> &data);

  /**
   * @brief Create a bitmap with the given dimensions and data
   */
  void Create(int width, int height, int depth, std::span<uint8_t> data);

  /**
   * @brief Create a bitmap with the given dimensions and data
   */
  void Create(int width, int height, int depth,
              const std::vector<uint8_t> &data);

  /**
   * @brief Create a bitmap with the given dimensions, format, and data
   */
  void Create(int width, int height, int depth, int format,
              const std::vector<uint8_t> &data);

  /**
   * @brief Reformat the bitmap to use a different pixel format
   */
  void Reformat(int format);

  /**
   * @brief Save the bitmap surface to a file
   */
  void SaveSurfaceToFile(std::string_view filename);

  // Texture management
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
   * @brief Updates the texture data from the surface
   */
  void UpdateTextureData();

  /**
   * @brief Clean up unused textures after a timeout
   */
  void CleanupUnusedTexture(uint64_t current_time, uint64_t timeout);

  // Palette management
  /**
   * @brief Set the palette for the bitmap
   */
  void SetPalette(const SnesPalette &palette);

  /**
   * @brief Set the palette with a transparent color
   */
  void SetPaletteWithTransparent(const SnesPalette &palette, size_t index,
                                 int length = 7);

  /**
   * @brief Set the palette from a palette group
   */
  void SetPaletteFromPaletteGroup(const SnesPalette &palette, int palette_id);

  /**
   * @brief Set the palette using SDL colors
   */
  void SetPalette(const std::vector<SDL_Color> &palette);

  // Pixel operations
  /**
   * @brief Write a value to a pixel at the given position
   */
  void WriteToPixel(int position, uint8_t value);

  /**
   * @brief Write a color to a pixel at the given position
   */
  void WriteColor(int position, const ImVec4 &color);

  // Tile operations
  /**
   * @brief Extract an 8x8 tile from the bitmap
   */
  void Get8x8Tile(int tile_index, int x, int y, std::vector<uint8_t> &tile_data,
                  int &tile_data_offset);

  /**
   * @brief Extract a 16x16 tile from the bitmap
   */
  void Get16x16Tile(int tile_x, int tile_y, std::vector<uint8_t> &tile_data,
                    int &tile_data_offset);

  /**
   * @brief Clean up the bitmap resources
   */
  void Cleanup();

  /**
   * @brief Clear the bitmap data
   */
  void Clear();

  const SnesPalette &palette() const { return palette_; }
  SnesPalette *mutable_palette() { return &palette_; }
  int width() const { return width_; }
  int height() const { return height_; }
  int depth() const { return depth_; }
  int size() const { return data_size_; }
  const uint8_t *data() const { return data_.data(); }
  std::vector<uint8_t> &mutable_data() { return data_; }
  SDL_Surface *surface() const { return surface_.get(); }
  SDL_Texture *texture() const { return texture_.get(); }
  const std::vector<uint8_t> &vector() const { return data_; }
  uint8_t at(int i) const { return data_[i]; }
  bool modified() const { return modified_; }
  bool is_active() const { return active_; }
  void set_active(bool active) { active_ = active; }
  void set_data(const std::vector<uint8_t> &data) { data_ = data; }
  void set_modified(bool modified) { modified_ = modified; }

#if YAZE_LIB_PNG == 1
  /**
   * @brief Get the bitmap data as PNG
   */
  std::vector<uint8_t> GetPngData();
#endif

 private:
  int width_ = 0;
  int height_ = 0;
  int depth_ = 0;
  int data_size_ = 0;

  bool active_ = false;
  bool modified_ = false;

  // Track if this texture is currently in use
  bool texture_in_use_ = false;

  // Track the last time this texture was used
  uint64_t last_used_time_ = 0;

  // Pointer to the texture pixels
  void *texture_pixels = nullptr;

  // Pointer to the pixel data
  uint8_t *pixel_data_ = nullptr;

  // Palette for the bitmap
  gfx::SnesPalette palette_;

  // Data for the bitmap
  std::vector<uint8_t> data_;

  // Surface for the bitmap
  std::shared_ptr<SDL_Surface> surface_ = nullptr;

  // Texture for the bitmap
  std::shared_ptr<SDL_Texture> texture_ = nullptr;
};

// Type alias for a table of bitmaps
using BitmapTable = std::unordered_map<int, gfx::Bitmap>;

// Utility functions that operate on Bitmap objects
/**
 * @brief Extract 8x8 tiles from a source bitmap
 */
std::vector<gfx::Bitmap> ExtractTile8Bitmaps(const gfx::Bitmap &source_bmp,
                                             const gfx::SnesPalette &palette,
                                             uint8_t palette_index);

/**
 * @brief Get the SDL pixel format for a given bitmap format
 */
Uint32 GetSnesPixelFormat(int format);

/**
 * @brief Allocate an SDL surface with the given dimensions and format
 */
SDL_Surface *AllocateSurface(int width, int height, int depth, Uint32 format);

/**
 * @brief Allocate an SDL texture with the given dimensions and format
 */
SDL_Texture *AllocateTexture(SDL_Renderer *renderer, Uint32 format, int access,
                             int width, int height);

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_BITMAP_H
