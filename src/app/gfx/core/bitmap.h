#ifndef YAZE_APP_GFX_BITMAP_H
#define YAZE_APP_GFX_BITMAP_H

#include <SDL.h>

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "app/gfx/backend/irenderer.h"
#include "app/gfx/types/snes_palette.h"

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
  void CreateTexture();

  /**
   * @brief Updates the underlying SDL_Texture when it already exists.
   */
  void UpdateTexture();

  /**
   * @brief Queue texture update for batch processing (improved performance)
   * @param renderer SDL renderer for texture operations
   * @note Use this for better performance when multiple textures need updating
   */
  void QueueTextureUpdate(IRenderer *renderer);

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
   * @brief Apply palette using metadata-driven strategy
   * Chooses between SetPalette and SetPaletteWithTransparent based on metadata
   */
  void ApplyPaletteByMetadata(const SnesPalette& palette, int sub_palette_index = 0);

  /**
   * @brief Apply the stored palette to the surface (internal helper)
   */
  void ApplyStoredPalette();
  
  /**
   * @brief Update SDL surface with current pixel data from data_ vector
   * Call this after modifying pixel data via mutable_data()
   */
  void UpdateSurfacePixels();

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
   * @brief Invalidate the palette lookup cache (call when palette changes)
   * @note This must be called whenever the palette is modified to maintain cache consistency
   */
  void InvalidatePaletteCache();

  /**
   * @brief Find color index in palette using optimized hash map lookup
   * @param color SNES color to find index for
   * @return Palette index (0 if not found)
   * @note O(1) lookup time vs O(n) linear search
   */
  uint8_t FindColorIndex(const SnesColor& color);

  /**
   * @brief Validate that bitmap data and surface pixels are synchronized
   * @return true if synchronized, false if there are issues
   * @note This method helps debug surface synchronization problems
   */
  bool ValidateDataSurfaceSync();

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

  /**
   * @brief Metadata for tracking bitmap source format and palette requirements
   */
  struct BitmapMetadata {
    int source_bpp = 8;  // Original bits per pixel (3, 4, 8)
    int palette_format = 0;  // 0=full palette, 1=sub-palette with transparent
    std::string source_type;  // "graphics_sheet", "tilemap", "screen_buffer", "mode7"
    int palette_colors = 256;  // Expected palette size
    
    BitmapMetadata() = default;
    BitmapMetadata(int bpp, int format, const std::string& type, int colors = 256)
        : source_bpp(bpp), palette_format(format), source_type(type), palette_colors(colors) {}
  };

  const SnesPalette &palette() const { return palette_; }
  SnesPalette *mutable_palette() { return &palette_; }
  BitmapMetadata& metadata() { return metadata_; }
  const BitmapMetadata& metadata() const { return metadata_; }
  
  int width() const { return width_; }
  int height() const { return height_; }
  int depth() const { return depth_; }
  auto size() const { return data_.size(); }
  const uint8_t *data() const { return data_.data(); }
  std::vector<uint8_t> &mutable_data() { return data_; }
  SDL_Surface *surface() const { return surface_; }
  TextureHandle texture() const { return texture_; }
  const std::vector<uint8_t> &vector() const { return data_; }
  uint8_t at(int i) const { return data_[i]; }
  bool modified() const { return modified_; }
  bool is_active() const { return active_; }
  void set_active(bool active) { active_ = active; }
  void set_data(const std::vector<uint8_t> &data);
  void set_modified(bool modified) { modified_ = modified; }
  void set_texture(TextureHandle texture) { texture_ = texture; }


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

  // Metadata for tracking source format and palette requirements
  BitmapMetadata metadata_;

  // Data for the bitmap
  std::vector<uint8_t> data_;

  // Surface for the bitmap (managed by Arena)
  SDL_Surface *surface_ = nullptr;

  // Texture for the bitmap (managed by Arena)
  TextureHandle texture_ = nullptr;

  // Optimized palette lookup cache for O(1) color index lookups
  std::unordered_map<uint32_t, uint8_t> color_to_index_cache_;

  // Dirty region tracking for efficient texture updates
  struct DirtyRegion {
    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    bool is_dirty = false;
    
    void Reset() {
      min_x = min_y = max_x = max_y = 0;
      is_dirty = false;
    }
    
    void AddPoint(int x, int y) {
      if (!is_dirty) {
        min_x = max_x = x;
        min_y = max_y = y;
        is_dirty = true;
      } else {
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
      }
    }
  } dirty_region_;

  /**
   * @brief Hash a color for cache lookup
   * @param color ImVec4 color to hash
   * @return 32-bit hash value
   */
  static uint32_t HashColor(const ImVec4& color);
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
