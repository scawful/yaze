#ifndef YAZE_APP_GFX_ARENA_H
#define YAZE_APP_GFX_ARENA_H

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/background_buffer.h"
#include "util/sdl_deleter.h"

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
    Bitmap* bitmap;  // The bitmap that needs a texture operation
    uint32_t generation;  // Generation at queue time for staleness detection
  };

  void QueueTextureCommand(TextureCommandType type, Bitmap* bitmap);
  void ProcessTextureQueue(IRenderer* renderer);
  void ClearTextureQueue();

  /**
   * @brief Check if there are pending textures to process
   * @return true if texture queue has pending commands
   */
  bool HasPendingTextures() const { return !texture_command_queue_.empty(); }

  /**
   * @brief Process a single texture command for frame-budget-aware loading
   * @param renderer The renderer to use for texture operations
   * @return true if a texture was processed, false if queue was empty
   */
  bool ProcessSingleTexture(IRenderer* renderer);

  // --- Surface Management (unchanged) ---
  SDL_Surface* AllocateSurface(int width, int height, int depth, int format);
  void FreeSurface(SDL_Surface* surface);

  void Shutdown();

  // Resource tracking for debugging
  size_t GetTextureCount() const { return textures_.size(); }
  size_t GetSurfaceCount() const { return surfaces_.size(); }
  size_t GetPooledTextureCount() const {
    return texture_pool_.available_textures_.size();
  }
  size_t GetPooledSurfaceCount() const {
    return surface_pool_.available_surfaces_.size();
  }
  size_t texture_command_queue_size() const {
    return texture_command_queue_.size();
  }

  // Graphics sheet access (223 total sheets in YAZE)
  /**
   * @brief Get reference to all graphics sheets
   * @return Reference to array of 223 Bitmap objects
   */
  std::array<gfx::Bitmap, 223>& gfx_sheets() { return gfx_sheets_; }

  /**
   * @brief Get a specific graphics sheet by index
   * @param i Sheet index (0-222)
   * @return Copy of the Bitmap at index i, or empty Bitmap if out of bounds
   */
  auto gfx_sheet(int i) {
    if (i < 0 || i >= 223) return gfx::Bitmap{};
    return gfx_sheets_[i];
  }

  /**
   * @brief Get mutable reference to a specific graphics sheet
   * @param i Sheet index (0-222)
   * @return Pointer to mutable Bitmap at index i, or nullptr if out of bounds
   */
  auto mutable_gfx_sheet(int i) {
    if (i < 0 || i >= 223) return static_cast<gfx::Bitmap*>(nullptr);
    return &gfx_sheets_[i];
  }

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

  // ========== Palette Change Notification System ==========

  /// Callback type for palette change listeners
  /// @param group_name The palette group that changed (e.g., "ow_main")
  /// @param palette_index The specific palette that changed, or -1 for all
  using PaletteChangeCallback =
      std::function<void(const std::string& group_name, int palette_index)>;

  /**
   * @brief Notify all listeners that a palette has been modified
   * @param group_name The palette group name (e.g., "ow_main", "dungeon_main")
   * @param palette_index Specific palette index, or -1 for entire group
   * @details This triggers bitmap refresh in editors using these palettes
   */
  void NotifyPaletteModified(const std::string& group_name,
                             int palette_index = -1);

  /**
   * @brief Register a callback for palette change notifications
   * @param callback Function to call when palettes change
   * @return Unique ID for this listener (use to unregister)
   */
  int RegisterPaletteListener(PaletteChangeCallback callback);

  /**
   * @brief Unregister a palette change listener
   * @param listener_id The ID returned from RegisterPaletteListener
   */
  void UnregisterPaletteListener(int listener_id);

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
    std::unordered_map<SDL_Surface*, std::tuple<int, int, int, int>>
        surface_info_;
    static constexpr size_t MAX_POOL_SIZE = 100;
  } surface_pool_;

  std::vector<TextureCommand> texture_command_queue_;
  IRenderer* renderer_ = nullptr;

  // Palette change notification system
  std::unordered_map<int, PaletteChangeCallback> palette_listeners_;
  int next_palette_listener_id_ = 1;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_ARENA_H
