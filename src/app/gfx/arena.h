#ifndef YAZE_APP_GFX_ARENA_H
#define YAZE_APP_GFX_ARENA_H

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "util/sdl_deleter.h"
#include "app/gfx/background_buffer.h"
#include "app/gfx/bitmap.h"

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

  void Initialize(IRenderer* renderer);
  ~Arena();

  // --- New Deferred Command System ---
  enum class TextureCommandType { CREATE, UPDATE, DESTROY };
  struct TextureCommand {
    TextureCommandType type;
    Bitmap* bitmap; // The bitmap that needs a texture operation
  };

  void QueueTextureCommand(TextureCommandType type, Bitmap* bitmap);
  void ProcessTextureQueue(IRenderer* renderer);

  // --- Surface Management (unchanged) ---
  SDL_Surface* AllocateSurface(int width, int height, int depth, int format);
  void FreeSurface(SDL_Surface* surface);
  
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
  
  /**
   * @brief Notify Arena that a graphics sheet has been modified
   * @param sheet_index Index of the modified sheet (0-222)
   * @details This ensures textures are updated across all editors
   */
  void NotifySheetModified(int sheet_index);

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

  std::unordered_map<TextureHandle,
                     std::unique_ptr<SDL_Texture, util::SDL_Texture_Deleter>>
      textures_;

  std::unordered_map<SDL_Surface*,
                     std::unique_ptr<SDL_Surface, util::SDL_Surface_Deleter>>
      surfaces_;

  // Resource pooling for efficient memory management
  struct TexturePool {
    std::vector<TextureHandle> available_textures_;
    std::unordered_map<TextureHandle, std::pair<int, int>> texture_sizes_;
    static constexpr size_t MAX_POOL_SIZE = 100;
  } texture_pool_;

  struct SurfacePool {
    std::vector<SDL_Surface*> available_surfaces_;
    std::unordered_map<SDL_Surface*, std::tuple<int, int, int, int>> surface_info_;
    static constexpr size_t MAX_POOL_SIZE = 100;
  } surface_pool_;

  std::vector<TextureCommand> texture_command_queue_;
  IRenderer* renderer_ = nullptr;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_ARENA_H
