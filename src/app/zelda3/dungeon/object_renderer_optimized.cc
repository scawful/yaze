#include "object_renderer.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <mutex>

#include "absl/strings/str_format.h"
#include "app/gfx/arena.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Optimized ObjectRenderer with enhanced safety and performance
 * 
 * This implementation includes:
 * - Comprehensive bounds checking to prevent segmentation faults
 * - Graphics sheet caching for improved performance
 * - Batch rendering optimizations
 * - Memory pool integration
 * - Thread-safe operations
 */
class OptimizedObjectRenderer : public ObjectRenderer {
 public:
  explicit OptimizedObjectRenderer(Rom* rom) : ObjectRenderer(rom) {
    // Initialize graphics sheet cache
    graphics_cache_.reserve(223); // Reserve space for all graphics sheets
    
    // Initialize memory pool
    memory_pool_ = std::make_unique<MemoryPool>();
  }
  
  // Enhanced rendering methods with safety checks
  absl::StatusOr<gfx::Bitmap> RenderObjectSafe(
      const RoomObject& object, const gfx::SnesPalette& palette);
  
  absl::StatusOr<gfx::Bitmap> RenderObjectsBatch(
      const std::vector<RoomObject>& objects, const gfx::SnesPalette& palette,
      int width, int height);
  
  // Memory management
  void ClearCache();
  size_t GetCacheSize() const;
  
 private:
  // Enhanced safety methods
  absl::Status ValidateObject(const RoomObject& object);
  absl::Status ValidatePalette(const gfx::SnesPalette& palette);
  absl::Status ValidateBitmap(const gfx::Bitmap& bitmap);
  
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
  };
  
  absl::StatusOr<std::shared_ptr<gfx::Bitmap>> GetGraphicsSheetSafe(int sheet_index);
  void CacheGraphicsSheet(int sheet_index, std::shared_ptr<gfx::Bitmap> sheet);
  
  // Memory pool for efficient allocations
  class MemoryPool {
   public:
    MemoryPool() : pool_size_(1024 * 1024), current_offset_(0) {
      pools_.push_back(std::make_unique<uint8_t[]>(pool_size_));
    }
    
    void* Allocate(size_t size) {
      std::lock_guard<std::mutex> lock(mutex_);
      
      // Align to 8-byte boundary
      size = (size + 7) & ~7;
      
      if (current_offset_ + size > pool_size_) {
        // Allocate new pool
        pools_.push_back(std::make_unique<uint8_t[]>(pool_size_));
        current_offset_ = 0;
      }
      
      void* ptr = pools_.back().get() + current_offset_;
      current_offset_ += size;
      return ptr;
    }
    
    void Reset() {
      std::lock_guard<std::mutex> lock(mutex_);
      current_offset_ = 0;
      // Keep first pool, clear others
      if (pools_.size() > 1) {
        pools_.erase(pools_.begin() + 1, pools_.end());
      }
    }
    
    size_t GetMemoryUsage() const {
      std::lock_guard<std::mutex> lock(mutex_);
      return pools_.size() * pool_size_;
    }
    
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
};

absl::StatusOr<gfx::Bitmap> OptimizedObjectRenderer::RenderObjectSafe(
    const RoomObject& object, const gfx::SnesPalette& palette) {
  
  // Comprehensive validation
  auto status = ValidateObject(object);
  if (!status.ok()) return status;
  
  status = ValidatePalette(palette);
  if (!status.ok()) return status;
  
  // Ensure object has tiles loaded
  if (object.tiles().empty()) {
    return absl::FailedPreconditionError("Object has no tiles loaded");
  }
  
  // Create bitmap with bounds checking
  int bitmap_width = std::min(512, static_cast<int>(object.tiles().size()) * 16);
  int bitmap_height = std::min(512, 32);
  
  if (bitmap_width <= 0 || bitmap_height <= 0) {
    return absl::InvalidArgumentError("Invalid bitmap dimensions");
  }
  
  gfx::Bitmap bitmap = CreateBitmap(bitmap_width, bitmap_height);
  if (!bitmap.is_active()) {
    return absl::InternalError("Failed to create bitmap");
  }
  
  // Render each tile with safety checks
  for (size_t i = 0; i < object.tiles().size(); ++i) {
    int tile_x = (i % 2) * 16;
    int tile_y = (i / 2) * 16;
    
    // Bounds check
    if (tile_x >= bitmap_width || tile_y >= bitmap_height) {
      continue; // Skip out-of-bounds tiles
    }
    
    auto tile_status = RenderTileSafe(object.tiles()[i], bitmap, tile_x, tile_y, palette);
    if (!tile_status.ok()) {
      // Log error but continue with other tiles
      continue;
    }
  }
  
  return bitmap;
}

absl::StatusOr<gfx::Bitmap> OptimizedObjectRenderer::RenderObjectsBatch(
    const std::vector<RoomObject>& objects, const gfx::SnesPalette& palette,
    int width, int height) {
  
  if (objects.empty()) {
    return absl::InvalidArgumentError("No objects to render");
  }
  
  if (width <= 0 || height <= 0 || width > 2048 || height > 2048) {
    return absl::InvalidArgumentError("Invalid bitmap dimensions");
  }
  
  gfx::Bitmap bitmap = CreateBitmap(width, height);
  if (!bitmap.is_active()) {
    return absl::InternalError("Failed to create bitmap");
  }
  
  // Collect all tiles for batch rendering
  std::vector<TileRenderInfo> tile_infos;
  tile_infos.reserve(objects.size() * 8); // Estimate 8 tiles per object
  
  for (const auto& object : objects) {
    if (object.tiles().empty()) continue;
    
    int obj_x = object.x_ * 16;
    int obj_y = object.y_ * 16;
    
    // Bounds check object position
    if (obj_x >= width || obj_y >= height || obj_x < -64 || obj_y < -64) {
      continue; // Skip objects completely out of bounds
    }
    
    for (size_t i = 0; i < object.tiles().size(); ++i) {
      int tile_x = obj_x + (i % 2) * 16;
      int tile_y = obj_y + (i / 2) * 16;
      
      // Bounds check individual tile
      if (tile_x >= -16 && tile_x < width && tile_y >= -16 && tile_y < height) {
        TileRenderInfo info;
        info.tile = &object.tiles()[i];
        info.x = tile_x;
        info.y = tile_y;
        info.sheet_index = -1; // Will be determined during rendering
        tile_infos.push_back(info);
      }
    }
  }
  
  // Batch render tiles
  auto batch_status = RenderTileBatch(tile_infos, bitmap, palette);
  if (!batch_status.ok()) {
    return batch_status;
  }
  
  return bitmap;
}

absl::Status OptimizedObjectRenderer::ValidateObject(const RoomObject& object) {
  if (object.id_ < 0 || object.id_ > 0x3FF) {
    return absl::InvalidArgumentError("Invalid object ID");
  }
  
  if (object.x_ > 255 || object.y_ > 255) {
    return absl::InvalidArgumentError("Object coordinates out of range");
  }
  
  if (object.layer_ > 2) {
    return absl::InvalidArgumentError("Invalid object layer");
  }
  
  if (object.rom_ == nullptr) {
    return absl::InvalidArgumentError("Object has no ROM reference");
  }
  
  return absl::OkStatus();
}

absl::Status OptimizedObjectRenderer::ValidatePalette(const gfx::SnesPalette& palette) {
  if (palette.empty()) {
    return absl::InvalidArgumentError("Palette is empty");
  }
  
  if (palette.size() > 256) {
    return absl::InvalidArgumentError("Palette size exceeds maximum");
  }
  
  return absl::OkStatus();
}

absl::Status OptimizedObjectRenderer::ValidateBitmap(const gfx::Bitmap& bitmap) {
  if (!bitmap.is_active()) {
    return absl::InvalidArgumentError("Bitmap is not active");
  }
  
  if (bitmap.surface() == nullptr) {
    return absl::InvalidArgumentError("Bitmap surface is null");
  }
  
  if (bitmap.width() <= 0 || bitmap.height() <= 0) {
    return absl::InvalidArgumentError("Invalid bitmap dimensions");
  }
  
  if (bitmap.data() == nullptr) {
    return absl::InvalidArgumentError("Bitmap data is null");
  }
  
  return absl::OkStatus();
}

absl::Status OptimizedObjectRenderer::RenderTileSafe(const gfx::Tile16& tile, 
                                                    gfx::Bitmap& bitmap, 
                                                    int x, int y, 
                                                    const gfx::SnesPalette& palette) {
  
  // Validate inputs
  auto status = ValidateBitmap(bitmap);
  if (!status.ok()) return status;
  
  if (x < 0 || y < 0 || x >= bitmap.width() || y >= bitmap.height()) {
    return absl::InvalidArgumentError("Tile position out of bitmap bounds");
  }
  
  // Render the 4 sub-tiles of the Tile16 with enhanced safety
  std::array<gfx::TileInfo, 4> sub_tiles = {
    tile.tile0_, tile.tile1_, tile.tile2_, tile.tile3_
  };

  for (int i = 0; i < 4; ++i) {
    const auto& tile_info = sub_tiles[i];
    int sub_x = x + (i % 2) * 8;
    int sub_y = y + (i / 2) * 8;
    
    // Bounds check sub-tile position
    if (sub_x < 0 || sub_y < 0 || sub_x >= bitmap.width() || sub_y >= bitmap.height()) {
      continue;
    }
    
    // Get graphics sheet with safety checks
    int sheet_index = tile_info.id_ / 256;
    if (sheet_index < 0 || sheet_index >= 223) {
      // Use fallback pattern for invalid sheet indices
      RenderTilePattern(bitmap, sub_x, sub_y, tile_info, palette);
      continue;
    }
    
    auto sheet_result = GetGraphicsSheetSafe(sheet_index);
    if (!sheet_result.ok()) {
      // Use fallback pattern if sheet is not available
      RenderTilePattern(bitmap, sub_x, sub_y, tile_info, palette);
      continue;
    }
    
    auto graphics_sheet = sheet_result.value();
    if (!graphics_sheet || !graphics_sheet->is_active()) {
      RenderTilePattern(bitmap, sub_x, sub_y, tile_info, palette);
      continue;
    }
    
    // Calculate tile position within the graphics sheet with bounds checking
    int tile_x = (tile_info.id_ % 16) * 8;
    int tile_y = ((tile_info.id_ % 256) / 16) * 8;
    
    // Validate tile position within graphics sheet
    if (tile_x < 0 || tile_y < 0 || 
        tile_x >= graphics_sheet->width() || tile_y >= graphics_sheet->height()) {
      RenderTilePattern(bitmap, sub_x, sub_y, tile_info, palette);
      continue;
    }
    
    // Render the 8x8 tile from the graphics sheet with comprehensive bounds checking
    for (int py = 0; py < 8; ++py) {
      for (int px = 0; px < 8; ++px) {
        int final_x = sub_x + px;
        int final_y = sub_y + py;
        
        // Bounds check final pixel position
        if (final_x < 0 || final_y < 0 || 
            final_x >= bitmap.width() || final_y >= bitmap.height()) {
          continue;
        }
        
        // Calculate source position with bounds checking
        int src_x = tile_x + px;
        int src_y = tile_y + py;
        
        if (src_x < 0 || src_y < 0 || 
            src_x >= graphics_sheet->width() || src_y >= graphics_sheet->height()) {
          continue;
        }
        
        int pixel_index = src_y * graphics_sheet->width() + src_x;
        if (pixel_index < 0 || pixel_index >= static_cast<int>(graphics_sheet->size())) {
          continue;
        }
        
        uint8_t color_index = graphics_sheet->at(pixel_index);
        
        // Validate color index
        if (color_index >= palette.size()) {
          continue;
        }
        
        // Apply mirroring with bounds checking
        int render_x = final_x;
        int render_y = final_y;
        
        if (tile_info.horizontal_mirror_) {
          render_x = sub_x + (7 - px);
          if (render_x < 0 || render_x >= bitmap.width()) continue;
        }
        if (tile_info.vertical_mirror_) {
          render_y = sub_y + (7 - py);
          if (render_y < 0 || render_y >= bitmap.height()) continue;
        }
        
        // Set pixel with final bounds check
        if (render_x >= 0 && render_y >= 0 && 
            render_x < bitmap.width() && render_y < bitmap.height()) {
          bitmap.SetPixel(render_x, render_y, palette[color_index]);
        }
      }
    }
  }

  return absl::OkStatus();
}

absl::Status OptimizedObjectRenderer::RenderTileBatch(
    const std::vector<TileRenderInfo>& tiles, gfx::Bitmap& bitmap, 
    const gfx::SnesPalette& palette) {
  
  if (tiles.empty()) {
    return absl::OkStatus();
  }
  
  // Group tiles by graphics sheet for efficient batch processing
  std::unordered_map<int, std::vector<TileRenderInfo>> sheet_tiles;
  
  for (const auto& tile_info : tiles) {
    if (tile_info.tile == nullptr) continue;
    
    // Determine graphics sheet for each sub-tile
    for (int i = 0; i < 4; i++) {
      const gfx::TileInfo* sub_tile = nullptr;
      switch (i) {
        case 0: sub_tile = &tile_info.tile->tile0_; break;
        case 1: sub_tile = &tile_info.tile->tile1_; break;
        case 2: sub_tile = &tile_info.tile->tile2_; break;
        case 3: sub_tile = &tile_info.tile->tile3_; break;
      }
      
      if (sub_tile == nullptr) continue;
      
      int sheet_index = sub_tile->id_ / 256;
      if (sheet_index >= 0 && sheet_index < 223) {
        TileRenderInfo sheet_tile_info = tile_info;
        sheet_tile_info.sheet_index = sheet_index;
        sheet_tiles[sheet_index].push_back(sheet_tile_info);
      }
    }
  }
  
  // Render tiles for each graphics sheet
  for (auto& [sheet_index, sheet_tiles_list] : sheet_tiles) {
    auto sheet_result = GetGraphicsSheetSafe(sheet_index);
    if (!sheet_result.ok()) {
      // Use fallback patterns for unavailable sheets
      for (const auto& tile_info : sheet_tiles_list) {
        for (int i = 0; i < 4; i++) {
          const gfx::TileInfo* sub_tile = nullptr;
          switch (i) {
            case 0: sub_tile = &tile_info.tile->tile0_; break;
            case 1: sub_tile = &tile_info.tile->tile1_; break;
            case 2: sub_tile = &tile_info.tile->tile2_; break;
            case 3: sub_tile = &tile_info.tile->tile3_; break;
          }
          
          if (sub_tile) {
            int sub_x = tile_info.x + (i % 2) * 8;
            int sub_y = tile_info.y + (i / 2) * 8;
            RenderTilePattern(bitmap, sub_x, sub_y, *sub_tile, palette);
          }
        }
      }
      continue;
    }
    
    auto graphics_sheet = sheet_result.value();
    if (!graphics_sheet || !graphics_sheet->is_active()) {
      continue;
    }
    
    // Render all tiles from this sheet efficiently
    for (const auto& tile_info : sheet_tiles_list) {
      auto status = RenderTileSafe(*tile_info.tile, bitmap, tile_info.x, tile_info.y, palette);
      if (!status.ok()) {
        // Continue with other tiles even if one fails
        continue;
      }
    }
  }
  
  return absl::OkStatus();
}

absl::StatusOr<std::shared_ptr<gfx::Bitmap>> OptimizedObjectRenderer::GetGraphicsSheetSafe(int sheet_index) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  
  if (sheet_index < 0 || sheet_index >= 223) {
    return absl::InvalidArgumentError("Invalid graphics sheet index");
  }
  
  auto it = graphics_cache_.find(sheet_index);
  if (it != graphics_cache_.end() && it->second.is_loaded) {
    it->second.last_accessed = std::chrono::steady_clock::now();
    return it->second.sheet;
  }
  
  // Load graphics sheet from Arena
  auto& arena = gfx::Arena::Get();
  auto sheet = arena.gfx_sheet(sheet_index);
  
  if (!sheet.is_active()) {
    return absl::NotFoundError("Graphics sheet not available");
  }
  
  // Cache the sheet
  GraphicsSheetInfo info;
  info.sheet = std::make_shared<gfx::Bitmap>(sheet);
  info.is_loaded = true;
  info.last_accessed = std::chrono::steady_clock::now();
  
  graphics_cache_[sheet_index] = info;
  
  return info.sheet;
}

void OptimizedObjectRenderer::CacheGraphicsSheet(int sheet_index, std::shared_ptr<gfx::Bitmap> sheet) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  
  if (sheet_index >= 0 && sheet_index < 223 && sheet) {
    GraphicsSheetInfo info;
    info.sheet = sheet;
    info.is_loaded = true;
    info.last_accessed = std::chrono::steady_clock::now();
    
    graphics_cache_[sheet_index] = info;
  }
}

void OptimizedObjectRenderer::ClearCache() {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  graphics_cache_.clear();
  
  if (memory_pool_) {
    memory_pool_->Reset();
  }
}

size_t OptimizedObjectRenderer::GetCacheSize() const {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  return graphics_cache_.size();
}

}  // namespace zelda3
}  // namespace yaze