#ifndef YAZE_APP_GFX_ARENA_H
#define YAZE_APP_GFX_ARENA_H

#include <array>
#include <cstdint>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "util/sdl_deleter.h"
#include "app/gfx/background_buffer.h"

namespace yaze {
namespace gfx {

/**
 * @brief Resource management arena for efficient graphics memory handling
 * 
 * The Arena class provides centralized management of SDL textures and surfaces
 * for the YAZE ROM hacking editor. It implements several key optimizations:
 * 
 * Key Features:
 * - Singleton pattern for global access across all graphics components
 * - Automatic resource cleanup with RAII-style management
 * - Memory pooling to reduce allocation overhead
 * - Support for 223 graphics sheets (YAZE's full graphics space)
 * - Background buffer management for SNES layer rendering
 * 
 * Performance Optimizations:
 * - Unique_ptr with custom deleters for automatic SDL resource cleanup
 * - Hash map storage for O(1) texture/surface lookup
 * - Batch resource management to minimize SDL calls
 * - Pre-allocated graphics sheet array for fast access
 * 
 * ROM Hacking Specific:
 * - Fixed-size graphics sheet array (223 sheets) matching YAZE's graphics space
 * - Background buffer support for SNES layer 1 and layer 2 rendering
 * - Tile buffer management for 64x64 tile grids
 * - Integration with SNES graphics format requirements
 */
class Arena {
 public:
  static Arena& Get();

  ~Arena();

  // Resource management
  /**
   * @brief Allocate a new SDL texture with automatic cleanup
   * @param renderer SDL renderer for texture creation
   * @param width Texture width in pixels
   * @param height Texture height in pixels
   * @return Pointer to allocated texture (managed by Arena)
   */
  SDL_Texture* AllocateTexture(SDL_Renderer* renderer, int width, int height);
  
  /**
   * @brief Free a texture and remove from Arena management
   * @param texture Texture to free
   */
  void FreeTexture(SDL_Texture* texture);
  
  /**
   * @brief Update texture data from surface (with format conversion)
   * @param texture Target texture to update
   * @param surface Source surface with pixel data
   */
  void UpdateTexture(SDL_Texture* texture, SDL_Surface* surface);

  /**
   * @brief Update texture data from surface for a specific region
   * @param texture Target texture to update
   * @param surface Source surface with pixel data
   * @param rect Region to update (nullptr for entire texture)
   */
  void UpdateTextureRegion(SDL_Texture* texture, SDL_Surface* surface, SDL_Rect* rect = nullptr);

  // Batch operations for improved performance
  /**
   * @brief Queue a texture update for batch processing
   * @param texture Target texture to update
   * @param surface Source surface with pixel data
   * @param rect Region to update (nullptr for entire texture)
   */
  void QueueTextureUpdate(SDL_Texture* texture, SDL_Surface* surface, SDL_Rect* rect = nullptr);

  /**
   * @brief Process all queued texture updates in a single batch
   * @note This reduces SDL calls and improves performance significantly
   */
  void ProcessBatchTextureUpdates();

  /**
   * @brief Clear all queued texture updates
   */
  void ClearBatchQueue();

  /**
   * @brief Allocate a new SDL surface with automatic cleanup
   * @param width Surface width in pixels
   * @param height Surface height in pixels
   * @param depth Color depth in bits per pixel
   * @param format SDL pixel format
   * @return Pointer to allocated surface (managed by Arena)
   */
  SDL_Surface* AllocateSurface(int width, int height, int depth, int format);
  
  /**
   * @brief Free a surface and remove from Arena management
   * @param surface Surface to free
   */
  void FreeSurface(SDL_Surface* surface);
  
  // Explicit cleanup method for controlled shutdown
  void Shutdown();
  
  // Resource tracking for debugging
  size_t GetTextureCount() const { return textures_.size(); }
  size_t GetSurfaceCount() const { return surfaces_.size(); }
  size_t GetPooledTextureCount() const { return texture_pool_.available_textures_.size(); }
  size_t GetPooledSurfaceCount() const { return surface_pool_.available_surfaces_.size(); }

  // Graphics sheet access (223 total sheets in YAZE)
  /**
   * @brief Get reference to all graphics sheets
   * @return Reference to array of 223 Bitmap objects
   */
  std::array<gfx::Bitmap, 223>& gfx_sheets() { return gfx_sheets_; }
  
  /**
   * @brief Get a specific graphics sheet by index
   * @param i Sheet index (0-222)
   * @return Copy of the Bitmap at index i
   */
  auto gfx_sheet(int i) { return gfx_sheets_[i]; }
  
  /**
   * @brief Get mutable reference to a specific graphics sheet
   * @param i Sheet index (0-222)
   * @return Pointer to mutable Bitmap at index i
   */
  auto mutable_gfx_sheet(int i) { return &gfx_sheets_[i]; }
  
  /**
   * @brief Get mutable reference to all graphics sheets
   * @return Pointer to mutable array of 223 Bitmap objects
   */
  auto mutable_gfx_sheets() { return &gfx_sheets_; }

  // Background buffer access for SNES layer rendering
  /**
   * @brief Get reference to background layer 1 buffer
   * @return Reference to BackgroundBuffer for layer 1
   */
  auto& bg1() { return bg1_; }
  
  /**
   * @brief Get reference to background layer 2 buffer
   * @return Reference to BackgroundBuffer for layer 2
   */
  auto& bg2() { return bg2_; }

 private:
  Arena();

  BackgroundBuffer bg1_;
  BackgroundBuffer bg2_;

  static constexpr int kTilesPerRow = 64;
  static constexpr int kTilesPerColumn = 64;
  static constexpr int kTotalTiles = kTilesPerRow * kTilesPerColumn;

  std::array<uint16_t, kTotalTiles> layer1_buffer_;
  std::array<uint16_t, kTotalTiles> layer2_buffer_;

  std::array<gfx::Bitmap, 223> gfx_sheets_;

  std::unordered_map<SDL_Texture*,
                     std::unique_ptr<SDL_Texture, util::SDL_Texture_Deleter>>
      textures_;

  std::unordered_map<SDL_Surface*,
                     std::unique_ptr<SDL_Surface, util::SDL_Surface_Deleter>>
      surfaces_;

  // Resource pooling for efficient memory management
  struct TexturePool {
    std::vector<SDL_Texture*> available_textures_;
    std::unordered_map<SDL_Texture*, std::pair<int, int>> texture_sizes_;
    static constexpr size_t MAX_POOL_SIZE = 100;
  } texture_pool_;

  struct SurfacePool {
    std::vector<SDL_Surface*> available_surfaces_;
    std::unordered_map<SDL_Surface*, std::tuple<int, int, int, int>> surface_info_;
    static constexpr size_t MAX_POOL_SIZE = 100;
  } surface_pool_;

  // Batch operations for improved performance
  struct BatchUpdate {
    SDL_Texture* texture;
    SDL_Surface* surface;
    std::unique_ptr<SDL_Rect> rect;
    
    BatchUpdate(SDL_Texture* t, SDL_Surface* s, SDL_Rect* r = nullptr)
        : texture(t), surface(s), rect(r ? std::make_unique<SDL_Rect>(*r) : nullptr) {}
  };
  
  std::vector<BatchUpdate> batch_update_queue_;
  static constexpr size_t MAX_BATCH_SIZE = 50;

  // Helper methods for resource pooling
  SDL_Texture* CreateNewTexture(SDL_Renderer* renderer, int width, int height);
  SDL_Surface* CreateNewSurface(int width, int height, int depth, int format);
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_ARENA_H
