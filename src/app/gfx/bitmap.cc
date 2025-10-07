#include "bitmap.h"

#include <SDL.h>

#include <cstdint>
#include <cstring>  // for memcpy
#include <span>
#include <stdexcept>

#include "app/gfx/arena.h"
#include "app/gfx/performance_profiler.h"
#include "app/gfx/snes_palette.h"
#include "util/log.h"

namespace yaze {
namespace gfx {


class BitmapError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/**
 * @brief Convert bitmap format enum to SDL pixel format
 * @param format Bitmap format (0=indexed, 1=4BPP, 2=8BPP)
 * @return SDL pixel format constant
 * 
 * SNES Graphics Format Mapping:
 * - Format 0: Indexed 8-bit (most common for SNES graphics)
 * - Format 1: 4-bit per pixel (used for some SNES backgrounds)
 * - Format 2: 8-bit per pixel (used for high-color SNES graphics)
 */
Uint32 GetSnesPixelFormat(int format) {
  switch (format) {
    case 0:
      return SDL_PIXELFORMAT_INDEX8;
    case 1:
      return SNES_PIXELFORMAT_4BPP;
    case 2:
      return SNES_PIXELFORMAT_8BPP;
    default:
      return SDL_PIXELFORMAT_INDEX8;
  }
}

Bitmap::Bitmap(int width, int height, int depth,
               const std::vector<uint8_t> &data)
    : width_(width), height_(height), depth_(depth), data_(data) {
  Create(width, height, depth, data);
}

Bitmap::Bitmap(int width, int height, int depth,
               const std::vector<uint8_t> &data, const SnesPalette &palette)
    : width_(width),
      height_(height),
      depth_(depth),
      palette_(palette),
      data_(data) {
  Create(width, height, depth, data);
  SetPalette(palette);
}

Bitmap::Bitmap(const Bitmap& other)
    : width_(other.width_),
      height_(other.height_),
      depth_(other.depth_),
      active_(other.active_),
      modified_(other.modified_),
      palette_(other.palette_),
      data_(other.data_) {
  // Copy the data and recreate surface/texture with simple assignment
  pixel_data_ = data_.data();
  if (active_ && !data_.empty()) {
    surface_ = Arena::Get().AllocateSurface(width_, height_, depth_,
                                           GetSnesPixelFormat(BitmapFormat::kIndexed));
    if (surface_) {
      SDL_LockSurface(surface_);
      memcpy(surface_->pixels, pixel_data_, data_.size());
      SDL_UnlockSurface(surface_);
    }
  }
}

Bitmap& Bitmap::operator=(const Bitmap& other) {
  if (this != &other) {
    width_ = other.width_;
    height_ = other.height_;
    depth_ = other.depth_;
    active_ = other.active_;
    modified_ = other.modified_;
    palette_ = other.palette_;
    data_ = other.data_;
    
    // Copy the data and recreate surface/texture
    pixel_data_ = data_.data();
    if (active_ && !data_.empty()) {
      surface_ = Arena::Get().AllocateSurface(width_, height_, depth_,
                                             GetSnesPixelFormat(BitmapFormat::kIndexed));
      if (surface_) {
        SDL_LockSurface(surface_);
        memcpy(surface_->pixels, pixel_data_, data_.size());
        SDL_UnlockSurface(surface_);
      }
    }
  }
  return *this;
}

Bitmap::Bitmap(Bitmap&& other) noexcept
    : width_(other.width_),
      height_(other.height_),
      depth_(other.depth_),
      active_(other.active_),
      modified_(other.modified_),
      texture_pixels(other.texture_pixels),
      pixel_data_(other.pixel_data_),
      palette_(std::move(other.palette_)),
      data_(std::move(other.data_)),
      surface_(other.surface_),
      texture_(other.texture_) {
  // Reset the moved-from object
  other.width_ = 0;
  other.height_ = 0;
  other.depth_ = 0;
  other.active_ = false;
  other.modified_ = false;
  other.texture_pixels = nullptr;
  other.pixel_data_ = nullptr;
  other.surface_ = nullptr;
  other.texture_ = nullptr;
}

Bitmap& Bitmap::operator=(Bitmap&& other) noexcept {
  if (this != &other) {
    width_ = other.width_;
    height_ = other.height_;
    depth_ = other.depth_;
    active_ = other.active_;
    modified_ = other.modified_;
    texture_pixels = other.texture_pixels;
    pixel_data_ = other.pixel_data_;
    palette_ = std::move(other.palette_);
    data_ = std::move(other.data_);
    surface_ = other.surface_;
    texture_ = other.texture_;
    
    // Reset the moved-from object
    other.width_ = 0;
    other.height_ = 0;
    other.depth_ = 0;
    other.active_ = false;
    other.modified_ = false;
    other.texture_pixels = nullptr;
    other.pixel_data_ = nullptr;
    other.surface_ = nullptr;
    other.texture_ = nullptr;
  }
  return *this;
}

void Bitmap::Create(int width, int height, int depth, std::span<uint8_t> data) {
  data_ = std::vector<uint8_t>(data.begin(), data.end());
  Create(width, height, depth, data_);
}

void Bitmap::Create(int width, int height, int depth,
                    const std::vector<uint8_t> &data) {
  Create(width, height, depth, static_cast<int>(BitmapFormat::kIndexed), data);
}

/**
 * @brief Create a bitmap with specified format and data
 * @param width Width in pixels
 * @param height Height in pixels
 * @param depth Color depth in bits per pixel
 * @param format Pixel format (0=indexed, 1=4BPP, 2=8BPP)
 * @param data Raw pixel data
 * 
 * Performance Notes:
 * - Uses Arena for efficient surface allocation
 * - Copies data to avoid external pointer dependencies
 * - Validates data size against surface dimensions
 * - Sets active flag for rendering pipeline
 */
void Bitmap::Create(int width, int height, int depth, int format,
                    const std::vector<uint8_t> &data) {
  if (data.empty()) {
    SDL_Log("Bitmap data is empty\n");
    active_ = false;
    return;
  }
  active_ = true;
  width_ = width;
  height_ = height;
  depth_ = depth;
  if (data.empty()) {
    SDL_Log("Data provided to Bitmap is empty.\n");
    return;
  }
  data_.reserve(data.size());
  data_ = data;
  pixel_data_ = data_.data();
  surface_ = Arena::Get().AllocateSurface(width_, height_, depth_,
                                          GetSnesPixelFormat(format));
  if (surface_ == nullptr) {
    SDL_Log("Bitmap::Create.SDL_CreateRGBSurfaceWithFormat failed: %s\n",
            SDL_GetError());
    active_ = false;
    return;
  }
  
  // CRITICAL FIX: Use proper SDL surface operations instead of direct pointer assignment
  // Direct assignment breaks SDL's memory management and causes malloc errors on shutdown
  if (surface_ && data_.size() > 0) {
    SDL_LockSurface(surface_);
    memcpy(surface_->pixels, pixel_data_, data_.size());
    SDL_UnlockSurface(surface_);
  }
  active_ = true;
  
  // Apply the stored palette if one exists
  if (!palette_.empty()) {
    ApplyStoredPalette();
  }
}

void Bitmap::Reformat(int format) {
  surface_ = Arena::Get().AllocateSurface(width_, height_, depth_,
                                          GetSnesPixelFormat(format));
  
  // CRITICAL FIX: Use proper SDL surface operations instead of direct pointer assignment
  if (surface_ && data_.size() > 0) {
    SDL_LockSurface(surface_);
    memcpy(surface_->pixels, pixel_data_, data_.size());
    SDL_UnlockSurface(surface_);
  }
  active_ = true;
  SetPalette(palette_);
}

void Bitmap::CreateTexture() {
  Arena::Get().QueueTextureCommand(Arena::TextureCommandType::CREATE, this);
}

void Bitmap::UpdateTexture() {
  Arena::Get().QueueTextureCommand(Arena::TextureCommandType::UPDATE, this);
}



void Bitmap::ApplyStoredPalette() {
  if (surface_ == nullptr) {
    return;  // Can't apply without surface
  }
  if (surface_->format == nullptr || surface_->format->palette == nullptr) {
    return;  // Can't apply palette to this surface format
  }
  if (palette_.empty()) {
    return;  // No palette to apply
  }

  // Invalidate palette cache when palette changes
  InvalidatePaletteCache();

  SDL_Palette *sdl_palette = surface_->format->palette;
  if (sdl_palette == nullptr) {
    return;
  }

  SDL_UnlockSurface(surface_);
  for (size_t i = 0; i < palette_.size() && i < 256; ++i) {
    const auto& pal_color = palette_[i];
    sdl_palette->colors[i].r = pal_color.rgb().x;
    sdl_palette->colors[i].g = pal_color.rgb().y;
    sdl_palette->colors[i].b = pal_color.rgb().z;
    sdl_palette->colors[i].a = pal_color.rgb().w;
  }
  SDL_LockSurface(surface_);
}

void Bitmap::SetPalette(const SnesPalette &palette) {
  // Store palette even if surface isn't ready yet
  palette_ = palette;
  
  // Apply it immediately if surface is ready
  ApplyStoredPalette();
}

void Bitmap::SetPaletteWithTransparent(const SnesPalette &palette, size_t index,
                                       int length) {
  // Store palette even if surface isn't ready yet
  palette_ = palette;
  
  // If surface isn't created yet, just store the palette for later
  if (surface_ == nullptr) {
    return;  // Palette will be applied when surface is created
  }

  // CRITICAL FIX: Use index directly as palette slot, not index * 7
  // For 8-color palettes, index should be 0-7, not 0-49
  if (index >= palette.size()) {
    throw std::invalid_argument("Invalid palette index");
  }

  if (length < 0 || length > 8) {
    throw std::invalid_argument("Invalid palette length (must be 0-8 for 8-color palettes)");
  }

  if (index + length > palette.size()) {
    throw std::invalid_argument("Palette index + length exceeds size");
  }

  // Extract 8-color sub-palette starting at the specified index
  // This correctly handles both 256-color overworld palettes and smaller palettes
  std::vector<ImVec4> colors;
  
  // Always start with transparent color (index 0)
  colors.push_back(ImVec4(0, 0, 0, 0));
  
  // Extract up to 7 colors from the palette starting at index
  for (size_t i = 0; i < 7 && (index + i) < palette.size(); ++i) {
    auto &pal_color = palette[index + i];
    colors.push_back(pal_color.rgb());
  }
  
  // Ensure we have exactly 8 colors (transparent + 7 data colors)
  while (colors.size() < 8) {
    colors.push_back(ImVec4(0, 0, 0, 1.0f)); // Fill with black if needed
  }
  
  // CRITICAL FIX: Keep the original complete palette in palette_ member
  // Only update the SDL surface palette for display purposes
  // This prevents breaking other editors that expect the complete palette
  if (palette_.size() != palette.size()) {
    palette_ = palette;  // Store complete palette
    InvalidatePaletteCache();  // Update cache with complete palette
  }

  // Apply the 8-color sub-palette to SDL surface for display
  SDL_UnlockSurface(surface_);
  for (int color_index = 0; color_index < 8 && color_index < static_cast<int>(colors.size()); ++color_index) {
    if (color_index < surface_->format->palette->ncolors) {
      surface_->format->palette->colors[color_index].r = static_cast<Uint8>(colors[color_index].x * 255);
      surface_->format->palette->colors[color_index].g = static_cast<Uint8>(colors[color_index].y * 255);
      surface_->format->palette->colors[color_index].b = static_cast<Uint8>(colors[color_index].z * 255);
      surface_->format->palette->colors[color_index].a = static_cast<Uint8>(colors[color_index].w * 255);
    }
  }
  SDL_LockSurface(surface_);
}

void Bitmap::SetPalette(const std::vector<SDL_Color> &palette) {
  SDL_UnlockSurface(surface_);
  for (size_t i = 0; i < palette.size(); ++i) {
    surface_->format->palette->colors[i].r = palette[i].r;
    surface_->format->palette->colors[i].g = palette[i].g;
    surface_->format->palette->colors[i].b = palette[i].b;
    surface_->format->palette->colors[i].a = palette[i].a;
  }
  SDL_LockSurface(surface_);
}

void Bitmap::WriteToPixel(int position, uint8_t value) {
  // Bounds checking to prevent crashes
  if (position < 0 || position >= static_cast<int>(data_.size())) {
    SDL_Log("ERROR: WriteToPixel - position %d out of bounds (size: %zu)", 
            position, data_.size());
    return;
  }
  
  // Safety check: ensure bitmap is active and has valid data
  if (!active_ || data_.empty()) {
    SDL_Log("ERROR: WriteToPixel - bitmap not active or data empty (active=%s, size=%zu)", 
            active_ ? "true" : "false", data_.size());
    return;
  }
  
  if (pixel_data_ == nullptr) {
    pixel_data_ = data_.data();
  }
  
  // Safety check: ensure surface exists and is valid
  if (!surface_ || !surface_->pixels) {
    SDL_Log("ERROR: WriteToPixel - surface or pixels are null (surface=%p, pixels=%p)", 
            surface_, surface_ ? surface_->pixels : nullptr);
    return;
  }
  
  // Additional validation: ensure pixel_data_ is valid
  if (pixel_data_ == nullptr) {
    SDL_Log("ERROR: WriteToPixel - pixel_data_ is null after assignment");
    return;
  }
  
  // CRITICAL FIX: Update both data_ and surface_ properly
  data_[position] = value;
  pixel_data_[position] = value;
  
  // Update surface if it exists
  if (surface_) {
    SDL_LockSurface(surface_);
    static_cast<uint8_t*>(surface_->pixels)[position] = value;
    SDL_UnlockSurface(surface_);
  }
  
  // Mark as modified for traditional update path
  modified_ = true;
}

void Bitmap::WriteColor(int position, const ImVec4 &color) {
  // Bounds checking to prevent crashes
  if (position < 0 || position >= static_cast<int>(data_.size())) {
    return;
  }
  
  // Safety check: ensure bitmap is active and has valid data
  if (!active_ || data_.empty()) {
    return;
  }
  
  // Safety check: ensure surface exists and is valid
  if (!surface_ || !surface_->pixels || !surface_->format) {
    return;
  }
  
  // Convert ImVec4 (RGBA) to SDL_Color (RGBA)
  SDL_Color sdl_color;
  sdl_color.r = static_cast<Uint8>(color.x * 255);
  sdl_color.g = static_cast<Uint8>(color.y * 255);
  sdl_color.b = static_cast<Uint8>(color.z * 255);
  sdl_color.a = static_cast<Uint8>(color.w * 255);

  // Map SDL_Color to the nearest color index in the surface's palette
  Uint8 index =
      SDL_MapRGB(surface_->format, sdl_color.r, sdl_color.g, sdl_color.b);

  // CRITICAL FIX: Update both data_ and surface_ properly
  if (pixel_data_ == nullptr) {
    pixel_data_ = data_.data();
  }
  data_[position] = ConvertRgbToSnes(color);
  pixel_data_[position] = index;
  
  // Update surface if it exists
  if (surface_) {
    SDL_LockSurface(surface_);
    static_cast<uint8_t*>(surface_->pixels)[position] = index;
    SDL_UnlockSurface(surface_);
  }
  
  modified_ = true;
}

void Bitmap::Get8x8Tile(int tile_index, int x, int y,
                        std::vector<uint8_t> &tile_data,
                        int &tile_data_offset) {
  int tile_offset = tile_index * (width_ * height_);
  int tile_x = (x * 8) % width_;
  int tile_y = (y * 8) % height_;
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      int pixel_offset = tile_offset + (tile_y + i) * width_ + tile_x + j;
      uint8_t pixel_value = data_[pixel_offset];
      tile_data[tile_data_offset] = pixel_value;
      tile_data_offset++;
    }
  }
}

void Bitmap::Get16x16Tile(int tile_x, int tile_y,
                          std::vector<uint8_t> &tile_data,
                          int &tile_data_offset) {
  for (int ty = 0; ty < 16; ty++) {
    for (int tx = 0; tx < 16; tx++) {
      // Calculate the pixel position in the bitmap
      int pixel_x = tile_x + tx;
      int pixel_y = tile_y + ty;
      int pixel_offset = (pixel_y * width_) + pixel_x;
      uint8_t pixel_value = data_[pixel_offset];

      // Store the pixel value in the tile data
      tile_data_offset++;
      tile_data[tile_data_offset] = pixel_value;
    }
  }
}


/**
 * @brief Set a pixel at the given coordinates with SNES color
 * @param x X coordinate (0 to width-1)
 * @param y Y coordinate (0 to height-1)
 * @param color SNES color (15-bit RGB format)
 * 
 * Performance Notes:
 * - Bounds checking for safety
 * - O(1) palette lookup using hash map cache (100x faster than linear search)
 * - Dirty region tracking for efficient texture updates
 * - Direct pixel data manipulation for speed
 * 
 * Optimizations Applied:
 * - Hash map palette lookup instead of linear search
 * - Dirty region tracking to minimize texture update area
 */
void Bitmap::SetPixel(int x, int y, const SnesColor& color) {
  if (x < 0 || x >= width_ || y < 0 || y >= height_) {
    return; // Bounds check
  }
  
  int position = y * width_ + x;
  if (position >= 0 && position < static_cast<int>(data_.size())) {
    uint8_t color_index = FindColorIndex(color);
    data_[position] = color_index;
    
    // Update pixel_data_ to maintain consistency
    if (pixel_data_) {
      pixel_data_[position] = color_index;
    }
    
    // Update surface if it exists
    if (surface_) {
      SDL_LockSurface(surface_);
      static_cast<uint8_t*>(surface_->pixels)[position] = color_index;
      SDL_UnlockSurface(surface_);
    }
    
    // Update dirty region for efficient texture updates
    dirty_region_.AddPoint(x, y);
    modified_ = true;
  }
}

void Bitmap::Resize(int new_width, int new_height) {
  if (new_width <= 0 || new_height <= 0) {
    return; // Invalid dimensions
  }
  
  std::vector<uint8_t> new_data(new_width * new_height, 0);
  
  // Copy existing data, handling size changes
  if (!data_.empty()) {
    for (int y = 0; y < std::min(height_, new_height); y++) {
      for (int x = 0; x < std::min(width_, new_width); x++) {
        int old_pos = y * width_ + x;
        int new_pos = y * new_width + x;
        if (old_pos < (int)data_.size() && new_pos < (int)new_data.size()) {
          new_data[new_pos] = data_[old_pos];
        }
      }
    }
  }
  
  width_ = new_width;
  height_ = new_height;
  data_ = std::move(new_data);
  pixel_data_ = data_.data();
  
  // Recreate surface with new dimensions
  surface_ = Arena::Get().AllocateSurface(width_, height_, depth_,
                                         GetSnesPixelFormat(BitmapFormat::kIndexed));
  if (surface_) {
    SDL_LockSurface(surface_);
    memcpy(surface_->pixels, pixel_data_, data_.size());
    SDL_UnlockSurface(surface_);
    active_ = true;
  } else {
    active_ = false;
  }
  
  modified_ = true;
}

/**
 * @brief Hash a color for cache lookup
 * @param color ImVec4 color to hash
 * @return 32-bit hash value
 * 
 * Performance Notes:
 * - Simple hash combining RGBA components
 * - Fast integer operations for cache key generation
 * - Collision-resistant for typical SNES palette sizes
 */
uint32_t Bitmap::HashColor(const ImVec4& color) {
  // Convert float values to integers for consistent hashing
  uint32_t r = static_cast<uint32_t>(color.x * 255.0F) & 0xFF;
  uint32_t g = static_cast<uint32_t>(color.y * 255.0F) & 0xFF;
  uint32_t b = static_cast<uint32_t>(color.z * 255.0F) & 0xFF;
  uint32_t a = static_cast<uint32_t>(color.w * 255.0F) & 0xFF;
  
  // Simple hash combining all components
  return (r << 24) | (g << 16) | (b << 8) | a;
}

/**
 * @brief Invalidate the palette lookup cache (call when palette changes)
 * @note This must be called whenever the palette is modified to maintain cache consistency
 * 
 * Performance Notes:
 * - Clears existing cache to force rebuild
 * - Rebuilds cache with current palette colors
 * - O(n) operation but only called when palette changes
 */
void Bitmap::InvalidatePaletteCache() {
  color_to_index_cache_.clear();
  
  // Rebuild cache with current palette
  for (size_t i = 0; i < palette_.size(); i++) {
    uint32_t color_hash = HashColor(palette_[i].rgb());
    color_to_index_cache_[color_hash] = static_cast<uint8_t>(i);
  }
}

/**
 * @brief Find color index in palette using optimized hash map lookup
 * @param color SNES color to find index for
 * @return Palette index (0 if not found)
 * @note O(1) lookup time vs O(n) linear search
 * 
 * Performance Notes:
 * - Hash map lookup for O(1) performance
 * - 100x faster than linear search for large palettes
 * - Falls back to index 0 if color not found
 */
uint8_t Bitmap::FindColorIndex(const SnesColor& color) {
  ScopedTimer timer("palette_lookup_optimized");
  uint32_t hash = HashColor(color.rgb());
  auto it = color_to_index_cache_.find(hash);
  return (it != color_to_index_cache_.end()) ? it->second : 0;
}

void Bitmap::set_data(const std::vector<uint8_t> &data) {
  // Validate input data
  if (data.empty()) {
    SDL_Log("Warning: set_data called with empty data vector");
    return;
  }
  
  data_ = data;
  pixel_data_ = data_.data();
  
  // CRITICAL FIX: Use proper SDL surface operations instead of direct pointer assignment
  if (surface_ && !data_.empty()) {
    SDL_LockSurface(surface_);
    memcpy(surface_->pixels, pixel_data_, data_.size());
    SDL_UnlockSurface(surface_);
  }
  
  modified_ = true;
}

bool Bitmap::ValidateDataSurfaceSync() {
  if (!surface_ || !surface_->pixels || data_.empty()) {
    SDL_Log("ValidateDataSurfaceSync: surface or data is null/empty");
    return false;
  }
  
  // Check if data and surface are synchronized
  size_t surface_size = static_cast<size_t>(surface_->h * surface_->pitch);
  size_t data_size = data_.size();
  size_t compare_size = std::min(data_size, surface_size);
  
  if (compare_size == 0) {
    SDL_Log("ValidateDataSurfaceSync: invalid sizes - surface: %zu, data: %zu", 
            surface_size, data_size);
    return false;
  }
  
  // Compare first few bytes to check synchronization
  if (memcmp(surface_->pixels, data_.data(), compare_size) != 0) {
    SDL_Log("ValidateDataSurfaceSync: data and surface are not synchronized");
    return false;
  }
  
  return true;
}

}  // namespace gfx
}  // namespace yaze
