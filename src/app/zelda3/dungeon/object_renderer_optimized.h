#ifndef YAZE_APP_ZELDA3_DUNGEON_OBJECT_RENDERER_OPTIMIZED_H
#define YAZE_APP_ZELDA3_DUNGEON_OBJECT_RENDERER_OPTIMIZED_H

#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_parser.h"
#include "app/zelda3/dungeon/room_object.h"
#include "object_renderer.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Optimized ObjectRenderer with enhanced safety and performance
 * 
 * This class extends the base ObjectRenderer with:
 * - Comprehensive bounds checking to prevent segmentation faults
 * - Graphics sheet caching for improved performance
 * - Batch rendering optimizations
 * - Memory pool integration
 * - Thread-safe operations
 * - Enhanced error handling and validation
 */
class OptimizedObjectRenderer : public ObjectRenderer {
 public:
  explicit OptimizedObjectRenderer(Rom* rom);
  
  // Enhanced rendering methods with safety checks
  absl::StatusOr<gfx::Bitmap> RenderObjectSafe(
      const RoomObject& object, const gfx::SnesPalette& palette);
  
  absl::StatusOr<gfx::Bitmap> RenderObjectsBatch(
      const std::vector<RoomObject>& objects, const gfx::SnesPalette& palette,
      int width, int height);
  
  // Memory management
  void ClearCache();
  size_t GetCacheSize() const;
  size_t GetMemoryUsage() const;
  
  // Performance monitoring
  struct PerformanceStats {
    size_t cache_hits = 0;
    size_t cache_misses = 0;
    size_t tiles_rendered = 0;
    size_t objects_rendered = 0;
    std::chrono::milliseconds total_render_time{0};
  };
  
  PerformanceStats GetPerformanceStats() const;
  void ResetPerformanceStats();

 private:
  // Enhanced safety methods
  absl::Status ValidateObject(const RoomObject& object);
  absl::Status ValidatePalette(const gfx::SnesPalette& palette);
  absl::Status ValidateBitmap(const gfx::Bitmap& bitmap);
  absl::Status ValidateCoordinates(int x, int y, int width, int height);
  
  // Optimized rendering methods
  absl::Status RenderTileSafe(const gfx::Tile16& tile, gfx::Bitmap& bitmap, 
                             int x, int y, const gfx::SnesPalette& palette);
  absl::Status RenderTileBatch(const std::vector<TileRenderInfo>& tiles,
                              gfx::Bitmap& bitmap, const gfx::SnesPalette& palette);
  
  // Graphics sheet management
  struct GraphicsSheetInfo {
    std::shared_ptr<gfx::Bitmap> sheet;
    bool is_loaded;
    std::chrono::steady_clock::time_point last_accessed;
    size_t access_count;
  };
  
  absl::StatusOr<std::shared_ptr<gfx::Bitmap>> GetGraphicsSheetSafe(int sheet_index);
  void CacheGraphicsSheet(int sheet_index, std::shared_ptr<gfx::Bitmap> sheet);
  void EvictLeastRecentlyUsed();
  
  // Memory pool for efficient allocations
  class MemoryPool {
   public:
    MemoryPool();
    ~MemoryPool() = default;
    
    void* Allocate(size_t size);
    void Reset();
    size_t GetMemoryUsage() const;
    
   private:
    std::vector<std::unique_ptr<uint8_t[]>> pools_;
    size_t pool_size_;
    size_t current_offset_;
    mutable std::mutex mutex_;
  };
  
  struct TileRenderInfo {
    const gfx::Tile16* tile;
    int x, y;
    int sheet_index;
  };
  
  // Member variables
  std::unordered_map<int, GraphicsSheetInfo> graphics_cache_;
  std::unique_ptr<MemoryPool> memory_pool_;
  mutable std::mutex cache_mutex_;
  mutable PerformanceStats performance_stats_;
  mutable std::mutex stats_mutex_;
  
  // Configuration
  static constexpr size_t kMaxCacheSize = 100; // Maximum cached graphics sheets
  static constexpr size_t kMemoryPoolSize = 1024 * 1024; // 1MB per pool
  static constexpr int kMaxBitmapSize = 2048; // Maximum bitmap dimensions
};

/**
 * @brief Factory function to create optimized renderer
 */
std::unique_ptr<OptimizedObjectRenderer> CreateOptimizedObjectRenderer(Rom* rom);

/**
 * @brief Utility functions for object rendering optimization
 */
namespace ObjectRenderingUtils {

/**
 * @brief Validate object data before rendering
 */
absl::Status ValidateObjectData(const RoomObject& object, Rom* rom);

/**
 * @brief Optimize object list for batch rendering
 */
std::vector<RoomObject> OptimizeObjectList(const std::vector<RoomObject>& objects);

/**
 * @brief Calculate optimal bitmap size for object list
 */
std::pair<int, int> CalculateOptimalBitmapSize(const std::vector<RoomObject>& objects);

/**
 * @brief Create optimized palette for object rendering
 */
gfx::SnesPalette CreateOptimizedPalette(const std::vector<RoomObject>& objects, Rom* rom);

/**
 * @brief Memory usage estimation for rendering operations
 */
size_t EstimateMemoryUsage(const std::vector<RoomObject>& objects, int bitmap_width, int bitmap_height);

}  // namespace ObjectRenderingUtils

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_OBJECT_RENDERER_OPTIMIZED_H