#ifndef YAZE_APP_GFX_BITMAP_H
#define YAZE_APP_GFX_BITMAP_H

#include <SDL.h>

#include <cstdint>
#include <span>
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

constexpr Uint32 SNES_PIXELFORMAT_4BPP = SDL_DEFINE_PIXELFORMAT(
    /*type=*/SDL_PIXELTYPE_INDEX8, /*order=*/0,
    /*layouts=*/0, /*bits=*/4, /*bytes=*/1);

constexpr Uint32 SNES_PIXELFORMAT_8BPP = SDL_DEFINE_PIXELFORMAT(
    /*type=*/SDL_PIXELTYPE_INDEX8, /*order=*/0,
    /*layouts=*/0, /*bits=*/8, /*bytes=*/1);

enum BitmapFormat {
  kIndexed = 0,
  k4bpp = 1,
  k8bpp = 2,
};


/**
 * @brief Represents a bitmap image optimized for SNES ROM hacking.
 *
 * The `Bitmap` class provides functionality to create, manipulate, and display
 * bitmap images specifically designed for Link to the Past ROM editing. It supports:
 * 
 * Key Features:
 * - SNES-specific pixel formats (4BPP, 8BPP, indexed)
 * - Palette management with transparent color support
 * - Tile extraction (8x8, 16x16) for ROM tile editing
 * - Memory-efficient surface/texture management via Arena
 * - Real-time editing with immediate visual feedback
 * 
 * Performance Optimizations:
 * - Lazy texture creation (textures only created when needed)
 * - Modified flag tracking to avoid unnecessary updates
 * - Arena-based resource pooling to reduce allocation overhead
 * - Direct pixel data manipulation for fast editing operations
 * 
 * ROM Hacking Specific:
 * - SNES color format conversion (15-bit RGB to 8-bit indexed)
 * - Tile-based editing for 8x8 and 16x16 SNES tiles
 * - Palette index management for ROM palette editing
 * - Support for multiple graphics sheets (223 total in YAZE)
 */
class Bitmap {
 public:
  Bitmap() = default;

  /**
   * @brief Create a bitmap with the given dimensions and raw pixel data
   * @param width Width in pixels (typically 128, 256, or 512 for SNES tilesheets)
   * @param height Height in pixels (typically 32, 64, or 128 for SNES tilesheets)
   * @param depth Color depth in bits per pixel (4, 8, or 16 for SNES)
   * @param data Raw pixel data (indexed color values for SNES graphics)
   */
  Bitmap(int width, int height, int depth, const std::vector<uint8_t> &data);

  /**
   * @brief Create a bitmap with the given dimensions, data, and SNES palette
   * @param width Width in pixels
   * @param height Height in pixels  
   * @param depth Color depth in bits per pixel
   * @param data Raw pixel data (indexed color values)
   * @param palette SNES palette for color mapping (15-bit RGB format)
   */
  Bitmap(int width, int height, int depth, const std::vector<uint8_t> &data,
         const SnesPalette &palette);

  /**
   * @brief Copy constructor - creates a deep copy
   */
  Bitmap(const Bitmap& other);

  /**
   * @brief Copy assignment operator
   */
  Bitmap& operator=(const Bitmap& other);

  /**
   * @brief Move constructor
   */
  Bitmap(Bitmap&& other) noexcept;

  /**
   * @brief Move assignment operator
   */
  Bitmap& operator=(Bitmap&& other) noexcept;

  /**
   * @brief Destructor
   */
  ~Bitmap() = default;

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
   * @brief Creates the underlying SDL_Texture to be displayed.
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
   * @brief Set the palette for the bitmap
   */
  void SetPalette(const SnesPalette &palette);

  /**
   * @brief Set the palette with a transparent color
   */
  void SetPaletteWithTransparent(const SnesPalette &palette, size_t index,
                                 int length = 7);

  /**
   * @brief Set the palette using SDL colors
   */
  void SetPalette(const std::vector<SDL_Color> &palette);

  /**
   * @brief Write a value to a pixel at the given position
   */
  void WriteToPixel(int position, uint8_t value);

  /**
   * @brief Write a color to a pixel at the given position
   */
  void WriteColor(int position, const ImVec4 &color);

  /**
   * @brief Set a pixel at the given x,y coordinates with SNES color
   * @param x X coordinate (0 to width-1)
   * @param y Y coordinate (0 to height-1) 
   * @param color SNES color (15-bit RGB format)
   * @note Automatically finds closest palette index and marks bitmap as modified
   */
  void SetPixel(int x, int y, const SnesColor& color);

  /**
   * @brief Resize the bitmap to new dimensions (preserves existing data)
   * @param new_width New width in pixels
   * @param new_height New height in pixels
   * @note Expands with black pixels, crops excess data
   */
  void Resize(int new_width, int new_height);

  /**
   * @brief Extract an 8x8 tile from the bitmap (SNES standard tile size)
   * @param tile_index Index of the tile in the tilesheet
   * @param x X offset within the tile (0-7)
   * @param y Y offset within the tile (0-7)
   * @param tile_data Output buffer for tile pixel data (64 bytes for 8x8)
   * @param tile_data_offset Current offset in tile_data buffer
   * @note Used for ROM tile editing and tile extraction
   */
  void Get8x8Tile(int tile_index, int x, int y, std::vector<uint8_t> &tile_data,
                  int &tile_data_offset);

  /**
   * @brief Extract a 16x16 tile from the bitmap (SNES metatile size)
   * @param tile_x X coordinate of tile in tilesheet
   * @param tile_y Y coordinate of tile in tilesheet
   * @param tile_data Output buffer for tile pixel data (256 bytes for 16x16)
   * @param tile_data_offset Current offset in tile_data buffer
   * @note Used for ROM metatile editing and large tile extraction
   */
  void Get16x16Tile(int tile_x, int tile_y, std::vector<uint8_t> &tile_data,
                    int &tile_data_offset);

  const SnesPalette &palette() const { return palette_; }
  SnesPalette *mutable_palette() { return &palette_; }
  int width() const { return width_; }
  int height() const { return height_; }
  int depth() const { return depth_; }
  auto size() const { return data_.size(); }
  const uint8_t *data() const { return data_.data(); }
  std::vector<uint8_t> &mutable_data() { return data_; }
  SDL_Surface *surface() const { return surface_; }
  SDL_Texture *texture() const { return texture_; }
  const std::vector<uint8_t> &vector() const { return data_; }
  uint8_t at(int i) const { return data_[i]; }
  bool modified() const { return modified_; }
  bool is_active() const { return active_; }
  void set_active(bool active) { active_ = active; }
  void set_data(const std::vector<uint8_t> &data) { data_ = data; }
  void set_modified(bool modified) { modified_ = modified; }


 private:
  int width_ = 0;
  int height_ = 0;
  int depth_ = 0;

  bool active_ = false;
  bool modified_ = false;

  // Pointer to the texture pixels
  void *texture_pixels = nullptr;

  // Pointer to the pixel data
  uint8_t *pixel_data_ = nullptr;

  // Palette for the bitmap
  gfx::SnesPalette palette_;

  // Data for the bitmap
  std::vector<uint8_t> data_;

  // Surface for the bitmap (managed by Arena)
  SDL_Surface *surface_ = nullptr;

  // Texture for the bitmap (managed by Arena)
  SDL_Texture *texture_ = nullptr;
};

// Type alias for a table of bitmaps
using BitmapTable = std::unordered_map<int, gfx::Bitmap>;

/**
 * @brief Get the SDL pixel format for a given bitmap format
 */
Uint32 GetSnesPixelFormat(int format);

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_BITMAP_H
