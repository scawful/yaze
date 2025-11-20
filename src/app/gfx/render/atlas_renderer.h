#ifndef YAZE_APP_GFX_ATLAS_RENDERER_H
#define YAZE_APP_GFX_ATLAS_RENDERER_H

#include <SDL.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/util/bpp_format_manager.h"

namespace yaze {
namespace gfx {

/**
 * @brief Render command for batch rendering
 */
struct RenderCommand {
  int atlas_id;            ///< Atlas ID of bitmap to render
  float x, y;              ///< Screen coordinates
  float scale_x, scale_y;  ///< Scale factors
  float rotation;          ///< Rotation angle in degrees
  SDL_Color tint;          ///< Color tint

  RenderCommand(int id, float x_pos, float y_pos, float sx = 1.0f,
                float sy = 1.0f, float rot = 0.0f,
                SDL_Color color = {255, 255, 255, 255})
      : atlas_id(id),
        x(x_pos),
        y(y_pos),
        scale_x(sx),
        scale_y(sy),
        rotation(rot),
        tint(color) {}
};

/**
 * @brief Atlas usage statistics
 */
struct AtlasStats {
  int total_atlases;
  int total_entries;
  int used_entries;
  size_t total_memory;
  size_t used_memory;
  float utilization_percent;

  AtlasStats()
      : total_atlases(0),
        total_entries(0),
        used_entries(0),
        total_memory(0),
        used_memory(0),
        utilization_percent(0.0f) {}
};

/**
 * @brief Atlas-based rendering system for efficient graphics operations
 *
 * The AtlasRenderer class provides efficient rendering by combining multiple
 * graphics elements into a single texture atlas, reducing draw calls and
 * improving performance for ROM hacking workflows.
 *
 * Key Features:
 * - Single draw call for multiple tiles/graphics
 * - Automatic atlas management and packing
 * - Dynamic atlas resizing and reorganization
 * - UV coordinate mapping for efficient rendering
 * - Memory-efficient texture management
 *
 * Performance Optimizations:
 * - Reduces draw calls from N to 1 for multiple elements
 * - Minimizes GPU state changes
 * - Efficient texture packing algorithm
 * - Automatic atlas defragmentation
 *
 * ROM Hacking Specific:
 * - Optimized for SNES tile rendering (8x8, 16x16)
 * - Support for graphics sheet atlasing
 * - Efficient palette management across atlas
 * - Tile-based UV coordinate system
 */
class AtlasRenderer {
 public:
  static AtlasRenderer& Get();

  /**
   * @brief Initialize the atlas renderer
   * @param renderer The renderer to use for texture operations
   * @param initial_size Initial atlas size (power of 2 recommended)
   */
  void Initialize(IRenderer* renderer, int initial_size = 1024);

  /**
   * @brief Add a bitmap to the atlas
   * @param bitmap Bitmap to add to atlas
   * @return Atlas ID for referencing this bitmap
   */
  int AddBitmap(const Bitmap& bitmap);

  /**
   * @brief Add a bitmap to the atlas with BPP format optimization
   * @param bitmap Bitmap to add to atlas
   * @param target_bpp Target BPP format for optimization
   * @return Atlas ID for referencing this bitmap
   */
  int AddBitmapWithBppOptimization(const Bitmap& bitmap, BppFormat target_bpp);

  /**
   * @brief Remove a bitmap from the atlas
   * @param atlas_id Atlas ID of bitmap to remove
   */
  void RemoveBitmap(int atlas_id);

  /**
   * @brief Update a bitmap in the atlas
   * @param atlas_id Atlas ID of bitmap to update
   * @param bitmap New bitmap data
   */
  void UpdateBitmap(int atlas_id, const Bitmap& bitmap);

  /**
   * @brief Render multiple bitmaps in a single draw call
   * @param render_commands Vector of render commands (atlas_id, x, y, scale)
   */
  void RenderBatch(const std::vector<RenderCommand>& render_commands);

  /**
   * @brief Render multiple bitmaps with BPP-aware batching
   * @param render_commands Vector of render commands
   * @param bpp_groups Map of BPP format to command groups for optimization
   */
  void RenderBatchWithBppOptimization(
      const std::vector<RenderCommand>& render_commands,
      const std::unordered_map<BppFormat, std::vector<int>>& bpp_groups);

  /**
   * @brief Get atlas statistics
   * @return Atlas usage statistics
   */
  AtlasStats GetStats() const;

  /**
   * @brief Defragment the atlas to reclaim space
   */
  void Defragment();

  /**
   * @brief Clear all atlases
   */
  void Clear();

  /**
   * @brief Render a single bitmap using atlas (convenience method)
   * @param atlas_id Atlas ID of bitmap to render
   * @param x X position on screen
   * @param y Y position on screen
   * @param scale_x Horizontal scale factor
   * @param scale_y Vertical scale factor
   */
  void RenderBitmap(int atlas_id, float x, float y, float scale_x = 1.0f,
                    float scale_y = 1.0f);

  /**
   * @brief Get UV coordinates for a bitmap in the atlas
   * @param atlas_id Atlas ID of bitmap
   * @return UV rectangle (0-1 normalized coordinates)
   */
  SDL_Rect GetUVCoordinates(int atlas_id) const;

 private:
  AtlasRenderer() = default;
  ~AtlasRenderer();

  struct AtlasEntry {
    int atlas_id;
    SDL_Rect uv_rect;  // UV coordinates in atlas
    TextureHandle texture;
    bool in_use;
    BppFormat bpp_format;  // BPP format of this entry
    int original_width;
    int original_height;

    AtlasEntry(int id, const SDL_Rect& rect, TextureHandle tex,
               BppFormat bpp = BppFormat::kBpp8, int width = 0, int height = 0)
        : atlas_id(id),
          uv_rect(rect),
          texture(tex),
          in_use(true),
          bpp_format(bpp),
          original_width(width),
          original_height(height) {}
  };

  struct Atlas {
    TextureHandle texture;
    int size;
    std::vector<AtlasEntry> entries;
    std::vector<bool> used_regions;  // Track used regions for packing

    Atlas(int s) : size(s), used_regions(s * s, false) {}
  };

  IRenderer* renderer_;
  std::vector<std::unique_ptr<Atlas>> atlases_;
  std::unordered_map<int, AtlasEntry*> atlas_lookup_;
  int next_atlas_id_;
  int current_atlas_;

  // Helper methods
  bool PackBitmap(Atlas& atlas, const Bitmap& bitmap, SDL_Rect& uv_rect);
  void CreateNewAtlas();
  void RebuildAtlas(Atlas& atlas);
  SDL_Rect FindFreeRegion(Atlas& atlas, int width, int height);
  void MarkRegionUsed(Atlas& atlas, const SDL_Rect& rect, bool used);
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_ATLAS_RENDERER_H
