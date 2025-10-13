#ifndef YAZE_APP_GFX_TEXTURE_ATLAS_H
#define YAZE_APP_GFX_TEXTURE_ATLAS_H

#include <map>
#include <memory>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "absl/status/status.h"

namespace yaze {
namespace gfx {

/**
 * @class TextureAtlas
 * @brief Manages multiple textures packed into a single large texture for performance
 * 
 * Future-proof infrastructure for combining multiple room textures into one atlas.
 * This reduces GPU state changes and improves rendering performance when many rooms are open.
 * 
 * Benefits:
 * - Fewer texture binds per frame
 * - Better memory locality
 * - Reduced VRAM fragmentation
 * - Easier batch rendering
 * 
 * Usage (Future):
 *   TextureAtlas atlas(2048, 2048);
 *   auto region = atlas.AllocateRegion(room_id, 512, 512);
 *   atlas.PackBitmap(room.bg1_buffer().bitmap(), *region);
 *   atlas.DrawRegion(room_id, x, y);
 */
class TextureAtlas {
 public:
  /**
   * @brief Region within the atlas texture
   */
  struct AtlasRegion {
    int x = 0;           // X position in atlas
    int y = 0;           // Y position in atlas
    int width = 0;       // Region width
    int height = 0;      // Region height
    int source_id = -1;  // ID of source (e.g., room_id)
    bool in_use = false; // Whether this region is allocated
  };
  
  /**
   * @brief Construct texture atlas with specified dimensions
   * @param width Atlas width in pixels (typically 2048 or 4096)
   * @param height Atlas height in pixels (typically 2048 or 4096)
   */
  explicit TextureAtlas(int width = 2048, int height = 2048);
  
  /**
   * @brief Allocate a region in the atlas for a source texture
   * @param source_id Identifier for the source (e.g., room_id)
   * @param width Required width in pixels
   * @param height Required height in pixels
   * @return Pointer to allocated region, or nullptr if no space
   * 
   * Uses simple rect packing algorithm. Future: implement more efficient packing.
   */
  AtlasRegion* AllocateRegion(int source_id, int width, int height);
  
  /**
   * @brief Pack a bitmap into an allocated region
   * @param src Source bitmap to pack
   * @param region Region to pack into (must be pre-allocated)
   * @return Status of packing operation
   * 
   * Copies pixel data from source bitmap into atlas at region coordinates.
   */
  absl::Status PackBitmap(const Bitmap& src, const AtlasRegion& region);
  
  /**
   * @brief Draw a region from the atlas to screen coordinates
   * @param source_id Source identifier (e.g., room_id)
   * @param dest_x Destination X coordinate
   * @param dest_y Destination Y coordinate
   * @return Status of drawing operation
   * 
   * Future: Integrate with renderer to draw atlas regions.
   */
  absl::Status DrawRegion(int source_id, int dest_x, int dest_y);
  
  /**
   * @brief Free a region and mark it as available
   * @param source_id Source identifier to free
   */
  void FreeRegion(int source_id);
  
  /**
   * @brief Clear all regions and reset atlas
   */
  void Clear();
  
  /**
   * @brief Get the atlas bitmap (contains all packed textures)
   * @return Reference to atlas bitmap
   */
  Bitmap& GetAtlasBitmap() { return atlas_bitmap_; }
  const Bitmap& GetAtlasBitmap() const { return atlas_bitmap_; }
  
  /**
   * @brief Get region for a specific source
   * @param source_id Source identifier
   * @return Pointer to region, or nullptr if not found
   */
  const AtlasRegion* GetRegion(int source_id) const;
  
  /**
   * @brief Get atlas dimensions
   */
  int width() const { return width_; }
  int height() const { return height_; }
  
  /**
   * @brief Get atlas utilization statistics
   */
  struct AtlasStats {
    int total_regions = 0;
    int used_regions = 0;
    int total_pixels = 0;
    int used_pixels = 0;
    float utilization = 0.0f;  // Percentage of atlas in use
  };
  AtlasStats GetStats() const;

 private:
  int width_;
  int height_;
  Bitmap atlas_bitmap_;  // Large combined bitmap
  
  // Simple linear packing for now (future: more efficient algorithms)
  int next_x_ = 0;
  int next_y_ = 0;
  int row_height_ = 0;  // Current row height for packing
  
  // Map source_id → region
  std::map<int, AtlasRegion> regions_;
  
  // Simple rect packing helper
  bool TryPackRect(int width, int height, int& out_x, int& out_y);
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_TEXTURE_ATLAS_H

