#include "unified_object_renderer.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <mutex>

#include "absl/strings/str_format.h"
#include "app/gfx/arena.h"

namespace yaze {
namespace zelda3 {

// Graphics Cache Implementation
class UnifiedObjectRenderer::GraphicsCache {
 public:
  GraphicsCache() : max_cache_size_(100), cache_hits_(0), cache_misses_(0) {
    cache_.reserve(223); // Reserve space for all graphics sheets
  }
  
  ~GraphicsCache() = default;
  
  absl::StatusOr<std::shared_ptr<gfx::Bitmap>> GetGraphicsSheet(int sheet_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Validate sheet index
    if (sheet_index < 0 || sheet_index >= 223) {
      return absl::InvalidArgumentError("Invalid graphics sheet index");
    }
    
    // Check cache first
    auto it = cache_.find(sheet_index);
    if (it != cache_.end() && it->second.is_loaded) {
      it->second.last_accessed = std::chrono::steady_clock::now();
      it->second.access_count++;
      cache_hits_++;
      return it->second.sheet;
    }
    
    // Load from Arena
    auto& arena = gfx::Arena::Get();
    auto sheet = arena.gfx_sheet(sheet_index);
    
    if (!sheet.is_active()) {
      cache_misses_++;
      return absl::NotFoundError("Graphics sheet not available");
    }
    
    // Cache the sheet
    GraphicsSheetInfo info;
    info.sheet = std::make_shared<gfx::Bitmap>(sheet);
    info.is_loaded = true;
    info.last_accessed = std::chrono::steady_clock::now();
    info.access_count = 1;
    
    cache_[sheet_index] = info;
    cache_misses_++;
    
    // Evict if cache is full
    if (cache_.size() > max_cache_size_) {
      EvictLeastRecentlyUsed();
    }
    
    return info.sheet;
  }
  
  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
  }
  
  size_t GetCacheSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
  }
  
  size_t GetMemoryUsage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t usage = 0;
    for (const auto& [index, info] : cache_) {
      if (info.sheet) {
        usage += info.sheet->width() * info.sheet->height();
      }
    }
    return usage;
  }
  
  void SetMaxCacheSize(size_t max_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_cache_size_ = max_size;
    while (cache_.size() > max_cache_size_) {
      EvictLeastRecentlyUsed();
    }
  }
  
  size_t GetCacheHits() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_hits_;
  }
  
  size_t GetCacheMisses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_misses_;
  }

 private:
  struct GraphicsSheetInfo {
    std::shared_ptr<gfx::Bitmap> sheet;
    bool is_loaded;
    std::chrono::steady_clock::time_point last_accessed;
    size_t access_count;
  };
  
  std::unordered_map<int, GraphicsSheetInfo> cache_;
  size_t max_cache_size_;
  size_t cache_hits_;
  size_t cache_misses_;
  mutable std::mutex mutex_;
  
  void EvictLeastRecentlyUsed() {
    auto oldest = cache_.end();
    auto oldest_time = std::chrono::steady_clock::now();
    
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
      if (it->second.last_accessed < oldest_time) {
        oldest = it;
        oldest_time = it->second.last_accessed;
      }
    }
    
    if (oldest != cache_.end()) {
      cache_.erase(oldest);
    }
  }
};

// Memory Pool Implementation
class UnifiedObjectRenderer::MemoryPool {
 public:
  MemoryPool() : pool_size_(1024 * 1024), current_offset_(0) {
    pools_.push_back(std::make_unique<uint8_t[]>(pool_size_));
  }
  
  ~MemoryPool() = default;
  
  void* Allocate(size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Align to 8-byte boundary for optimal performance
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

// Performance Monitor Implementation
class UnifiedObjectRenderer::PerformanceMonitor {
 public:
  PerformanceMonitor() = default;
  ~PerformanceMonitor() = default;
  
  void RecordRenderTime(std::chrono::high_resolution_clock::duration duration) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    stats_.total_render_time += ms;
  }
  
  void IncrementObjectCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.objects_rendered++;
  }
  
  void IncrementTileCount(size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.tiles_rendered += count;
  }
  
  void IncrementMemoryAllocation() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.memory_allocations++;
  }
  
  void IncrementGraphicsSheetLoad() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.graphics_sheet_loads++;
  }
  
  void UpdateCacheStats(size_t hits, size_t misses) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.cache_hits = hits;
    stats_.cache_misses = misses;
  }
  
  UnifiedObjectRenderer::PerformanceStats GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
  }
  
  void Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = UnifiedObjectRenderer::PerformanceStats{};
  }

 private:
  UnifiedObjectRenderer::PerformanceStats stats_;
  mutable std::mutex mutex_;
};

// Enhanced Object Parser Implementation
class UnifiedObjectRenderer::ObjectParser {
 public:
  explicit ObjectParser(Rom* rom) : rom_(rom) {
    if (rom_ == nullptr) {
      throw std::invalid_argument("ROM cannot be null");
    }
    InitializeObjectTables();
  }
  
  ~ObjectParser() = default;
  
  absl::StatusOr<std::vector<gfx::Tile16>> ParseObject(int16_t object_id) {
    // Comprehensive validation
    auto status = ValidateObjectID(object_id);
    if (!status.ok()) return status;
    
    // Determine subtype and parse accordingly
    int subtype = GetObjectSubtype(object_id);
    switch (subtype) {
      case 1: return ParseSubtype1(object_id);
      case 2: return ParseSubtype2(object_id);
      case 3: return ParseSubtype3(object_id);
      default: return absl::InvalidArgumentError("Invalid object subtype");
    }
  }

 private:
  Rom* rom_;
  
  void InitializeObjectTables() {
    // Initialize object table constants based on ROM analysis
    // These values are derived from the Link to the Past ROM structure
  }
  
  absl::Status ValidateObjectID(int16_t object_id) {
    if (object_id < 0 || object_id > 0x3FF) {
      return absl::InvalidArgumentError("Object ID out of range");
    }
    return absl::OkStatus();
  }
  
  bool ValidateROMAddress(int address, size_t size) {
    return address >= 0 && (address + size) <= static_cast<int>(rom_->size());
  }
  
  int GetObjectSubtype(int16_t object_id) {
    if (object_id < 0x100) return 1;
    if (object_id < 0x200) return 2;
    return 3;
  }
  
  absl::StatusOr<std::vector<gfx::Tile16>> ParseSubtype1(int16_t object_id) {
    int index = object_id & 0xFF;
    int tile_ptr = kRoomObjectSubtype1 + (index * 2);
    
    // Enhanced bounds checking
    if (!ValidateROMAddress(tile_ptr, 2)) {
      return absl::OutOfRangeError("Tile pointer out of range");
    }
    
    // Read tile data pointer
    uint8_t low = rom_->data()[tile_ptr];
    uint8_t high = rom_->data()[tile_ptr + 1];
    int tile_data_ptr = kRoomObjectTileAddress + ((high << 8) | low);
    
    // Validate tile data address
    if (!ValidateROMAddress(tile_data_ptr, 64)) {
      return absl::OutOfRangeError("Tile data address out of range");
    }
    
    return ReadTileData(tile_data_ptr, 8); // 8 tiles for subtype 1
  }
  
  absl::StatusOr<std::vector<gfx::Tile16>> ParseSubtype2(int16_t object_id) {
    int index = (object_id & 0xFF) - 0x100;
    int tile_ptr = kRoomObjectSubtype2 + (index * 2);
    
    if (!ValidateROMAddress(tile_ptr, 2)) {
      return absl::OutOfRangeError("Tile pointer out of range");
    }
    
    uint8_t low = rom_->data()[tile_ptr];
    uint8_t high = rom_->data()[tile_ptr + 1];
    int tile_data_ptr = kRoomObjectTileAddress + ((high << 8) | low);
    
    if (!ValidateROMAddress(tile_data_ptr, 128)) {
      return absl::OutOfRangeError("Tile data address out of range");
    }
    
    return ReadTileData(tile_data_ptr, 16); // 16 tiles for subtype 2
  }
  
  absl::StatusOr<std::vector<gfx::Tile16>> ParseSubtype3(int16_t object_id) {
    int index = (object_id & 0xFF) - 0x200;
    int tile_ptr = kRoomObjectSubtype3 + (index * 2);
    
    if (!ValidateROMAddress(tile_ptr, 2)) {
      return absl::OutOfRangeError("Tile pointer out of range");
    }
    
    uint8_t low = rom_->data()[tile_ptr];
    uint8_t high = rom_->data()[tile_ptr + 1];
    int tile_data_ptr = kRoomObjectTileAddress + ((high << 8) | low);
    
    if (!ValidateROMAddress(tile_data_ptr, 256)) {
      return absl::OutOfRangeError("Tile data address out of range");
    }
    
    return ReadTileData(tile_data_ptr, 32); // 32 tiles for subtype 3
  }
  
  absl::StatusOr<std::vector<gfx::Tile16>> ReadTileData(int address, int tile_count) {
    std::vector<gfx::Tile16> tiles;
    tiles.reserve(tile_count);
    
    for (int i = 0; i < tile_count; i++) {
      int tile_address = address + (i * 8);
      
      if (!ValidateROMAddress(tile_address, 8)) {
        // Create placeholder tile for invalid data
        tiles.emplace_back(gfx::TileInfo{}, gfx::TileInfo{}, gfx::TileInfo{}, gfx::TileInfo{});
        continue;
      }
      
      // Read 4 tile infos (8 bytes total)
      uint16_t w0 = rom_->data()[tile_address] | (rom_->data()[tile_address + 1] << 8);
      uint16_t w1 = rom_->data()[tile_address + 2] | (rom_->data()[tile_address + 3] << 8);
      uint16_t w2 = rom_->data()[tile_address + 4] | (rom_->data()[tile_address + 5] << 8);
      uint16_t w3 = rom_->data()[tile_address + 6] | (rom_->data()[tile_address + 7] << 8);
      
      tiles.emplace_back(gfx::WordToTileInfo(w0), gfx::WordToTileInfo(w1),
                         gfx::WordToTileInfo(w2), gfx::WordToTileInfo(w3));
    }
    
    return tiles;
  }
};

// Main UnifiedObjectRenderer Implementation
UnifiedObjectRenderer::UnifiedObjectRenderer(Rom* rom) 
    : rom_(rom)
    , graphics_cache_(std::make_unique<GraphicsCache>())
    , memory_pool_(std::make_unique<MemoryPool>())
    , performance_monitor_(std::make_unique<PerformanceMonitor>())
    , parser_(std::make_unique<ObjectParser>(rom)) {
}

absl::StatusOr<gfx::Bitmap> UnifiedObjectRenderer::RenderObject(const RoomObject& object, const gfx::SnesPalette& palette) {
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Validate inputs
  auto status = ValidateInputs(object, palette);
  if (!status.ok()) return status;
  
  // Ensure object has tiles loaded
  if (object.tiles().empty()) {
    return absl::FailedPreconditionError("Object has no tiles loaded");
  }
  
  // Create bitmap
  int bitmap_width = std::min(512, static_cast<int>(object.tiles().size()) * 16);
  int bitmap_height = std::min(512, 32);
  
  auto bitmap_result = CreateBitmap(bitmap_width, bitmap_height);
  if (!bitmap_result.ok()) return bitmap_result;
  
  auto bitmap = std::move(bitmap_result.value());
  
  // Render tiles
  for (size_t i = 0; i < object.tiles().size(); ++i) {
    int tile_x = (i % 2) * 16;
    int tile_y = (i / 2) * 16;
    
    auto tile_status = RenderTileToBitmap(object.tiles()[i], bitmap, tile_x, tile_y, palette);
    if (!tile_status.ok()) {
      // Continue with other tiles
      continue;
    }
  }
  
  // Update performance stats
  auto end_time = std::chrono::high_resolution_clock::now();
  if (performance_monitoring_enabled_) {
    performance_monitor_->RecordRenderTime(end_time - start_time);
    performance_monitor_->IncrementObjectCount();
    performance_monitor_->IncrementTileCount(object.tiles().size());
  }
  
  return bitmap;
}

absl::StatusOr<gfx::Bitmap> UnifiedObjectRenderer::RenderObjects(const std::vector<RoomObject>& objects, const gfx::SnesPalette& palette) {
  if (objects.empty()) {
    return absl::InvalidArgumentError("No objects to render");
  }
  
  // Validate inputs
  auto status = ValidateInputs(objects, palette);
  if (!status.ok()) return status;
  
  // Calculate optimal bitmap size
  auto [width, height] = CalculateOptimalBitmapSize(objects);
  
  auto bitmap_result = CreateBitmap(width, height);
  if (!bitmap_result.ok()) return bitmap_result;
  
  auto bitmap = std::move(bitmap_result.value());
  
  // Collect all tiles for batch rendering
  std::vector<TileRenderInfo> tile_infos;
  tile_infos.reserve(objects.size() * 8);
  
  for (const auto& object : objects) {
    if (object.tiles().empty()) continue;
    
    int obj_x = object.x_ * 16;
    int obj_y = object.y_ * 16;
    
    for (size_t i = 0; i < object.tiles().size(); ++i) {
      int tile_x = obj_x + (i % 2) * 16;
      int tile_y = obj_y + (i / 2) * 16;
      
      if (tile_x >= -16 && tile_x < width && tile_y >= -16 && tile_y < height) {
        TileRenderInfo info;
        info.tile = &object.tiles()[i];
        info.x = tile_x;
        info.y = tile_y;
        info.sheet_index = -1;
        tile_infos.push_back(info);
      }
    }
  }
  
  // Batch render tiles
  auto batch_status = BatchRenderTiles(tile_infos, bitmap, palette);
  if (!batch_status.ok()) return batch_status;
  
  // Update performance stats
  if (performance_monitoring_enabled_) {
    performance_monitor_->IncrementObjectCount();
    performance_monitor_->IncrementTileCount(tile_infos.size());
  }
  
  return bitmap;
}

absl::StatusOr<gfx::Bitmap> UnifiedObjectRenderer::RenderRoom(const Room& room, const gfx::SnesPalette& palette) {
  // Combine room layout objects with room objects
  std::vector<RoomObject> all_objects;
  
  // Add room layout objects
  const auto& layout_objects = room.GetLayout().GetObjects();
  for (const auto& layout_obj : layout_objects) {
    // Convert layout object to room object (simplified)
    RoomObject room_obj(layout_obj.id(), layout_obj.x(), layout_obj.y(), 0x12, layout_obj.layer());
    room_obj.set_rom(rom_);
    room_obj.EnsureTilesLoaded();
    all_objects.push_back(room_obj);
  }
  
  // Add regular room objects
  for (const auto& obj : room.tile_objects_) {
    all_objects.push_back(obj);
  }
  
  return RenderObjects(all_objects, palette);
}

void UnifiedObjectRenderer::ClearCache() {
  graphics_cache_->Clear();
  memory_pool_->Reset();
  if (performance_monitoring_enabled_) {
    performance_monitor_->Reset();
  }
}

size_t UnifiedObjectRenderer::GetMemoryUsage() const {
  return memory_pool_->GetMemoryUsage() + graphics_cache_->GetMemoryUsage();
}

UnifiedObjectRenderer::PerformanceStats UnifiedObjectRenderer::GetPerformanceStats() const {
  auto stats = performance_monitor_->GetStats();
  stats.cache_hits = graphics_cache_->GetCacheHits();
  stats.cache_misses = graphics_cache_->GetCacheMisses();
  return stats;
}

void UnifiedObjectRenderer::ResetPerformanceStats() {
  if (performance_monitoring_enabled_) {
    performance_monitor_->Reset();
  }
}

void UnifiedObjectRenderer::SetCacheSize(size_t max_cache_size) {
  max_cache_size_ = max_cache_size;
  graphics_cache_->SetMaxCacheSize(max_cache_size);
}

void UnifiedObjectRenderer::EnablePerformanceMonitoring(bool enable) {
  performance_monitoring_enabled_ = enable;
}

// Private method implementations
absl::Status UnifiedObjectRenderer::ValidateInputs(const RoomObject& object, const gfx::SnesPalette& palette) {
  if (object.id_ < 0 || object.id_ > 0x3FF) {
    return absl::InvalidArgumentError("Invalid object ID");
  }
  
  if (object.x_ > 255 || object.y_ > 255) {
    return absl::InvalidArgumentError("Object coordinates out of range");
  }
  
  if (palette.empty()) {
    return absl::InvalidArgumentError("Palette is empty");
  }
  
  return absl::OkStatus();
}

absl::Status UnifiedObjectRenderer::ValidateInputs(const std::vector<RoomObject>& objects, const gfx::SnesPalette& palette) {
  if (objects.empty()) {
    return absl::InvalidArgumentError("No objects to render");
  }
  
  if (palette.empty()) {
    return absl::InvalidArgumentError("Palette is empty");
  }
  
  for (const auto& object : objects) {
    auto status = ValidateInputs(object, palette);
    if (!status.ok()) return status;
  }
  
  return absl::OkStatus();
}

absl::StatusOr<gfx::Bitmap> UnifiedObjectRenderer::CreateBitmap(int width, int height) {
  if (width <= 0 || height <= 0 || width > 2048 || height > 2048) {
    return absl::InvalidArgumentError("Invalid bitmap dimensions");
  }
  
  gfx::Bitmap bitmap(width, height);
  if (!bitmap.is_active()) {
    return absl::InternalError("Failed to create bitmap");
  }
  
  if (performance_monitoring_enabled_) {
    performance_monitor_->IncrementMemoryAllocation();
  }
  
  return bitmap;
}

absl::Status UnifiedObjectRenderer::RenderTileToBitmap(const gfx::Tile16& tile, gfx::Bitmap& bitmap, int x, int y, const gfx::SnesPalette& palette) {
  // Render the 4 sub-tiles of the Tile16
  std::array<gfx::TileInfo, 4> sub_tiles = {
    tile.tile0_, tile.tile1_, tile.tile2_, tile.tile3_
  };

  for (int i = 0; i < 4; ++i) {
    const auto& tile_info = sub_tiles[i];
    int sub_x = x + (i % 2) * 8;
    int sub_y = y + (i / 2) * 8;
    
    // Bounds check
    if (sub_x < 0 || sub_y < 0 || sub_x >= bitmap.width() || sub_y >= bitmap.height()) {
      continue;
    }
    
    // Get graphics sheet
    int sheet_index = tile_info.id_ / 256;
    auto sheet_result = graphics_cache_->GetGraphicsSheet(sheet_index);
    if (!sheet_result.ok()) {
      // Use fallback pattern
      RenderTilePattern(bitmap, sub_x, sub_y, tile_info, palette);
      continue;
    }
    
    auto graphics_sheet = sheet_result.value();
    if (!graphics_sheet || !graphics_sheet->is_active()) {
      RenderTilePattern(bitmap, sub_x, sub_y, tile_info, palette);
      continue;
    }
    
    // Render 8x8 tile from graphics sheet
    Render8x8Tile(bitmap, graphics_sheet.get(), tile_info, sub_x, sub_y, palette);
  }
  
  return absl::OkStatus();
}

void UnifiedObjectRenderer::Render8x8Tile(gfx::Bitmap& bitmap, gfx::Bitmap* graphics_sheet, const gfx::TileInfo& tile_info, int x, int y, const gfx::SnesPalette& palette) {
  int tile_x = (tile_info.id_ % 16) * 8;
  int tile_y = ((tile_info.id_ % 256) / 16) * 8;
  
  for (int py = 0; py < 8; ++py) {
    for (int px = 0; px < 8; ++px) {
      int final_x = x + px;
      int final_y = y + py;
      
      if (final_x < 0 || final_y < 0 || final_x >= bitmap.width() || final_y >= bitmap.height()) {
        continue;
      }
      
      int src_x = tile_x + px;
      int src_y = tile_y + py;
      
      if (src_x < 0 || src_y < 0 || src_x >= graphics_sheet->width() || src_y >= graphics_sheet->height()) {
        continue;
      }
      
      int pixel_index = src_y * graphics_sheet->width() + src_x;
      if (pixel_index < 0 || pixel_index >= static_cast<int>(graphics_sheet->size())) {
        continue;
      }
      
      uint8_t color_index = graphics_sheet->at(pixel_index);
      
      if (color_index >= palette.size()) {
        continue;
      }
      
      // Apply mirroring
      int render_x = final_x;
      int render_y = final_y;
      
      if (tile_info.horizontal_mirror_) {
        render_x = x + (7 - px);
        if (render_x < 0 || render_x >= bitmap.width()) continue;
      }
      if (tile_info.vertical_mirror_) {
        render_y = y + (7 - py);
        if (render_y < 0 || render_y >= bitmap.height()) continue;
      }
      
      if (render_x >= 0 && render_y >= 0 && render_x < bitmap.width() && render_y < bitmap.height()) {
        bitmap.SetPixel(render_x, render_y, palette[color_index]);
      }
    }
  }
}

void UnifiedObjectRenderer::RenderTilePattern(gfx::Bitmap& bitmap, int x, int y, const gfx::TileInfo& tile_info, const gfx::SnesPalette& palette) {
  // Render a simple pattern for missing tiles
  uint8_t color = (tile_info.id_ % 16) + 1;
  if (color >= palette.size()) color = 1;
  
  for (int py = 0; py < 8; ++py) {
    for (int px = 0; px < 8; ++px) {
      int final_x = x + px;
      int final_y = y + py;
      
      if (final_x >= 0 && final_y >= 0 && final_x < bitmap.width() && final_y < bitmap.height()) {
        bitmap.SetPixel(final_x, final_y, palette[color]);
      }
    }
  }
}

absl::Status UnifiedObjectRenderer::BatchRenderTiles(const std::vector<TileRenderInfo>& tiles, gfx::Bitmap& bitmap, const gfx::SnesPalette& palette) {
  // Group tiles by graphics sheet for efficiency
  std::unordered_map<int, std::vector<TileRenderInfo>> sheet_tiles;
  
  for (const auto& tile_info : tiles) {
    if (tile_info.tile == nullptr) continue;
    
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
    auto sheet_result = graphics_cache_->GetGraphicsSheet(sheet_index);
    if (!sheet_result.ok()) {
      continue;
    }
    
    auto graphics_sheet = sheet_result.value();
    if (!graphics_sheet || !graphics_sheet->is_active()) {
      continue;
    }
    
    // Render all tiles from this sheet
    for (const auto& tile_info : sheet_tiles_list) {
      auto status = RenderTileToBitmap(*tile_info.tile, bitmap, tile_info.x, tile_info.y, palette);
      if (!status.ok()) {
        continue;
      }
    }
  }
  
  return absl::OkStatus();
}

std::pair<int, int> UnifiedObjectRenderer::CalculateOptimalBitmapSize(const std::vector<RoomObject>& objects) {
  if (objects.empty()) {
    return {256, 256};
  }
  
  int max_x = 0, max_y = 0;
  
  for (const auto& obj : objects) {
    int obj_max_x = obj.x_ * 16 + 16;
    int obj_max_y = obj.y_ * 16 + 16;
    
    max_x = std::max(max_x, obj_max_x);
    max_y = std::max(max_y, obj_max_y);
  }
  
  // Round up to nearest power of 2
  int width = 1;
  int height = 1;
  
  while (width < max_x) width <<= 1;
  while (height < max_y) height <<= 1;
  
  // Cap at maximum size
  width = std::min(width, 2048);
  height = std::min(height, 2048);
  
  return {width, height};
}

bool UnifiedObjectRenderer::IsObjectInBounds(const RoomObject& object, int bitmap_width, int bitmap_height) {
  int obj_x = object.x_ * 16;
  int obj_y = object.y_ * 16;
  
  return obj_x >= 0 && obj_y >= 0 && 
         obj_x < bitmap_width && obj_y < bitmap_height;
}

// Factory function
std::unique_ptr<UnifiedObjectRenderer> CreateUnifiedObjectRenderer(Rom* rom) {
  return std::make_unique<UnifiedObjectRenderer>(rom);
}

// Utility functions
namespace ObjectRenderingUtils {

absl::Status ValidateObjectData(const RoomObject& object, Rom* rom) {
  if (rom == nullptr) {
    return absl::InvalidArgumentError("ROM is null");
  }
  
  if (object.id_ < 0 || object.id_ > 0x3FF) {
    return absl::InvalidArgumentError("Invalid object ID");
  }
  
  if (object.x_ > 255 || object.y_ > 255) {
    return absl::InvalidArgumentError("Object coordinates out of range");
  }
  
  return absl::OkStatus();
}

std::vector<RoomObject> OptimizeObjectList(const std::vector<RoomObject>& objects) {
  // Sort objects by graphics sheet for better cache efficiency
  std::vector<RoomObject> optimized = objects;
  
  std::sort(optimized.begin(), optimized.end(), [](const RoomObject& a, const RoomObject& b) {
    // Sort by object ID (which correlates with graphics sheet usage)
    return a.id_ < b.id_;
  });
  
  return optimized;
}

std::pair<int, int> CalculateOptimalBitmapSize(const std::vector<RoomObject>& objects) {
  if (objects.empty()) {
    return {256, 256}; // Default size
  }
  
  int max_x = 0, max_y = 0;
  
  for (const auto& obj : objects) {
    int obj_max_x = obj.x_ * 16 + 16; // Object width
    int obj_max_y = obj.y_ * 16 + 16; // Object height
    
    max_x = std::max(max_x, obj_max_x);
    max_y = std::max(max_y, obj_max_y);
  }
  
  // Round up to nearest power of 2 for efficiency
  int width = 1;
  int height = 1;
  
  while (width < max_x) width <<= 1;
  while (height < max_y) height <<= 1;
  
  // Cap at maximum size
  width = std::min(width, 2048);
  height = std::min(height, 2048);
  
  return {width, height};
}

gfx::SnesPalette CreateOptimizedPalette(const std::vector<RoomObject>& objects, Rom* rom) {
  // Create a palette optimized for the objects being rendered
  // This is a simplified implementation - in practice, you'd analyze
  // the actual tile data to create an optimal palette
  
  gfx::SnesPalette palette;
  
  // Add basic colors that are commonly used
  for (int i = 0; i < 16; i++) {
    int intensity = i * 16;
    palette.AddColor(gfx::SnesColor(intensity, intensity, intensity));
  }
  
  return palette;
}

size_t EstimateMemoryUsage(const std::vector<RoomObject>& objects, int bitmap_width, int bitmap_height) {
  size_t bitmap_memory = bitmap_width * bitmap_height; // 1 byte per pixel
  
  size_t object_memory = objects.size() * sizeof(RoomObject);
  
  size_t tile_memory = 0;
  for (const auto& obj : objects) {
    tile_memory += obj.tiles().size() * sizeof(gfx::Tile16);
  }
  
  return bitmap_memory + object_memory + tile_memory;
}

bool IsObjectInBounds(const RoomObject& object, int bitmap_width, int bitmap_height) {
  int obj_x = object.x_ * 16;
  int obj_y = object.y_ * 16;
  
  return obj_x >= 0 && obj_y >= 0 && 
         obj_x < bitmap_width && obj_y < bitmap_height;
}

int GetObjectSubtype(int16_t object_id) {
  if (object_id < 0x100) return 1;
  if (object_id < 0x200) return 2;
  return 3;
}

bool IsValidObjectID(int16_t object_id) {
  return object_id >= 0 && object_id <= 0x3FF;
}

}  // namespace ObjectRenderingUtils

}  // namespace zelda3
}  // namespace yaze