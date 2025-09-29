# YAZE Graphics System Optimization Recommendations

## Overview
This document provides comprehensive analysis and optimization recommendations for the YAZE graphics system, specifically targeting improvements for Link to the Past ROM hacking workflows.

## Current Architecture Analysis

### Strengths
1. **Arena-based Resource Management**: Efficient SDL resource pooling
2. **SNES-specific Format Support**: Proper handling of 4BPP/8BPP graphics
3. **Palette Management**: Integrated SNES palette system
4. **Tile-based Editing**: Support for 8x8 and 16x16 tiles

### Performance Bottlenecks Identified

#### 1. Bitmap Class Issues
- **Linear Palette Search**: `SetPixel()` uses O(n) palette lookup
- **Redundant Data Copies**: Multiple copies of pixel data
- **Inefficient Texture Updates**: Full texture updates for single pixel changes
- **Missing Bounds Optimization**: No early exit for out-of-bounds operations

#### 2. Arena Resource Management
- **Hash Map Overhead**: O(1) lookup but memory overhead for small collections
- **No Resource Pooling**: Each allocation creates new SDL resources
- **Missing Batch Operations**: No bulk texture/surface operations

#### 3. Tilemap Performance
- **Lazy Loading Inefficiency**: Tiles created on-demand without batching
- **Memory Fragmentation**: Individual tile bitmaps cause memory fragmentation
- **No Tile Caching Strategy**: No LRU or smart caching for frequently used tiles

## Optimization Recommendations

### 1. Bitmap Class Optimizations

#### A. Palette Lookup Optimization
```cpp
// Current: O(n) linear search
uint8_t color_index = 0;
for (size_t i = 0; i < palette_.size(); i++) {
  if (palette_[i].rgb().x == color.rgb().x && ...) {
    color_index = static_cast<uint8_t>(i);
    break;
  }
}

// Optimized: O(1) hash map lookup
class Bitmap {
private:
  std::unordered_map<uint32_t, uint8_t> color_to_index_cache_;
  
public:
  void InvalidatePaletteCache() {
    color_to_index_cache_.clear();
    for (size_t i = 0; i < palette_.size(); i++) {
      uint32_t color_hash = HashColor(palette_[i].rgb());
      color_to_index_cache_[color_hash] = static_cast<uint8_t>(i);
    }
  }
  
  uint8_t FindColorIndex(const SnesColor& color) {
    uint32_t hash = HashColor(color.rgb());
    auto it = color_to_index_cache_.find(hash);
    return (it != color_to_index_cache_.end()) ? it->second : 0;
  }
};
```

#### B. Dirty Region Tracking
```cpp
class Bitmap {
private:
  struct DirtyRegion {
    int min_x, min_y, max_x, max_y;
    bool is_dirty = false;
  } dirty_region_;
  
public:
  void SetPixel(int x, int y, const SnesColor& color) {
    // ... existing code ...
    
    // Update dirty region instead of marking entire bitmap
    if (!dirty_region_.is_dirty) {
      dirty_region_.min_x = dirty_region_.max_x = x;
      dirty_region_.min_y = dirty_region_.max_y = y;
      dirty_region_.is_dirty = true;
    } else {
      dirty_region_.min_x = std::min(dirty_region_.min_x, x);
      dirty_region_.min_y = std::min(dirty_region_.min_y, y);
      dirty_region_.max_x = std::max(dirty_region_.max_x, x);
      dirty_region_.max_y = std::max(dirty_region_.max_y, y);
    }
  }
  
  void UpdateTexture(SDL_Renderer* renderer) {
    if (!dirty_region_.is_dirty) return;
    
    // Only update the dirty region
    SDL_Rect dirty_rect = {
      dirty_region_.min_x, dirty_region_.min_y,
      dirty_region_.max_x - dirty_region_.min_x + 1,
      dirty_region_.max_y - dirty_region_.min_y + 1
    };
    
    // Update only the dirty region
    Arena::Get().UpdateTextureRegion(texture_, surface_, &dirty_rect);
    dirty_region_.is_dirty = false;
  }
};
```

### 2. Arena Resource Management Improvements

#### A. Resource Pooling
```cpp
class Arena {
private:
  struct TexturePool {
    std::vector<SDL_Texture*> available_textures_;
    std::unordered_map<SDL_Texture*, std::pair<int, int>> texture_sizes_;
  } texture_pool_;
  
  struct SurfacePool {
    std::vector<SDL_Surface*> available_surfaces_;
    std::unordered_map<SDL_Surface*, std::tuple<int, int, int, int>> surface_info_;
  } surface_pool_;
  
public:
  SDL_Texture* AllocateTexture(SDL_Renderer* renderer, int width, int height) {
    // Try to reuse existing texture of same size
    for (auto it = texture_pool_.available_textures_.begin(); 
         it != texture_pool_.available_textures_.end(); ++it) {
      auto& size = texture_pool_.texture_sizes_[*it];
      if (size.first == width && size.second == height) {
        SDL_Texture* texture = *it;
        texture_pool_.available_textures_.erase(it);
        return texture;
      }
    }
    
    // Create new texture if none available
    return CreateNewTexture(renderer, width, height);
  }
  
  void FreeTexture(SDL_Texture* texture) {
    // Return to pool instead of destroying
    texture_pool_.available_textures_.push_back(texture);
  }
};
```

#### B. Batch Operations
```cpp
class Arena {
public:
  struct BatchUpdate {
    std::vector<std::pair<SDL_Texture*, SDL_Surface*>> updates_;
    
    void AddUpdate(SDL_Texture* texture, SDL_Surface* surface) {
      updates_.emplace_back(texture, surface);
    }
    
    void Execute() {
      // Batch all texture updates for efficiency
      for (auto& update : updates_) {
        UpdateTexture(update.first, update.second);
      }
      updates_.clear();
    }
  };
  
  BatchUpdate CreateBatch() { return BatchUpdate{}; }
};
```

### 3. Tilemap Performance Enhancements

#### A. Smart Tile Caching
```cpp
class Tilemap {
private:
  struct TileCache {
    static constexpr size_t MAX_CACHE_SIZE = 1024;
    std::unordered_map<int, Bitmap> cache_;
    std::list<int> access_order_;
    
    Bitmap* GetTile(int tile_id) {
      auto it = cache_.find(tile_id);
      if (it != cache_.end()) {
        // Move to front of access order
        access_order_.remove(tile_id);
        access_order_.push_front(tile_id);
        return &it->second;
      }
      return nullptr;
    }
    
    void CacheTile(int tile_id, Bitmap&& bitmap) {
      if (cache_.size() >= MAX_CACHE_SIZE) {
        // Remove least recently used tile
        int lru_tile = access_order_.back();
        access_order_.pop_back();
        cache_.erase(lru_tile);
      }
      
      cache_[tile_id] = std::move(bitmap);
      access_order_.push_front(tile_id);
    }
  } tile_cache_;
  
public:
  void RenderTile(int tile_id) {
    Bitmap* cached_tile = tile_cache_.GetTile(tile_id);
    if (cached_tile) {
      core::Renderer::Get().UpdateBitmap(cached_tile);
      return;
    }
    
    // Create new tile and cache it
    Bitmap new_tile = CreateTileFromAtlas(tile_id);
    tile_cache_.CacheTile(tile_id, std::move(new_tile));
    core::Renderer::Get().RenderBitmap(&tile_cache_.cache_[tile_id]);
  }
};
```

#### B. Atlas-based Rendering
```cpp
class Tilemap {
public:
  void RenderTilemap(const std::vector<int>& tile_ids, 
                     const std::vector<SDL_Rect>& positions) {
    // Batch render multiple tiles from atlas
    std::vector<SDL_Rect> src_rects;
    std::vector<SDL_Rect> dst_rects;
    
    for (size_t i = 0; i < tile_ids.size(); ++i) {
      SDL_Rect src_rect = GetTileRect(tile_ids[i]);
      src_rects.push_back(src_rect);
      dst_rects.push_back(positions[i]);
    }
    
    // Single draw call for all tiles
    core::Renderer::Get().RenderAtlas(atlas.texture(), src_rects, dst_rects);
  }
};
```

### 4. Editor-Specific Optimizations

#### A. Graphics Editor Improvements
```cpp
class GraphicsEditor {
private:
  struct EditingState {
    bool is_drawing = false;
    std::vector<PixelChange> undo_stack_;
    std::vector<PixelChange> redo_stack_;
    DirtyRegion current_edit_region_;
  } editing_state_;
  
public:
  void StartDrawing() {
    editing_state_.is_drawing = true;
    editing_state_.current_edit_region_.Reset();
  }
  
  void EndDrawing() {
    if (editing_state_.is_drawing) {
      // Batch update only the edited region
      UpdateDirtyRegion(editing_state_.current_edit_region_);
      editing_state_.is_drawing = false;
    }
  }
  
  void SetPixel(int x, int y, const SnesColor& color) {
    // Record change for undo/redo
    editing_state_.undo_stack_.emplace_back(x, y, GetPixel(x, y), color);
    
    // Update pixel
    current_bitmap_->SetPixel(x, y, color);
    
    // Update edit region
    editing_state_.current_edit_region_.AddPoint(x, y);
  }
};
```

#### B. Palette Editor Optimizations
```cpp
class PaletteEditor {
private:
  struct PaletteCache {
    std::unordered_map<uint32_t, ImVec4> snes_to_rgba_cache_;
    std::unordered_map<uint32_t, uint16_t> rgba_to_snes_cache_;
    
    void Invalidate() {
      snes_to_rgba_cache_.clear();
      rgba_to_snes_cache_.clear();
    }
  } palette_cache_;
  
public:
  ImVec4 ConvertSnesToRgba(uint16_t snes_color) {
    uint32_t key = snes_color;
    auto it = palette_cache_.snes_to_rgba_cache_.find(key);
    if (it != palette_cache_.snes_to_rgba_cache_.end()) {
      return it->second;
    }
    
    ImVec4 rgba = ConvertSnesColorToImVec4(SnesColor(snes_color));
    palette_cache_.snes_to_rgba_cache_[key] = rgba;
    return rgba;
  }
};
```

### 5. Memory Management Improvements

#### A. Custom Allocator for Graphics Data
```cpp
class GraphicsAllocator {
private:
  static constexpr size_t POOL_SIZE = 16 * 1024 * 1024; // 16MB
  char* pool_;
  size_t offset_;
  
public:
  GraphicsAllocator() : pool_(new char[POOL_SIZE]), offset_(0) {}
  
  void* Allocate(size_t size) {
    if (offset_ + size > POOL_SIZE) {
      return nullptr; // Pool exhausted
    }
    
    void* ptr = pool_ + offset_;
    offset_ += size;
    return ptr;
  }
  
  void Reset() { offset_ = 0; }
};
```

#### B. Smart Pointer Management
```cpp
template<typename T>
class GraphicsPtr {
private:
  T* ptr_;
  std::function<void(T*)> deleter_;
  
public:
  GraphicsPtr(T* ptr, std::function<void(T*)> deleter) 
    : ptr_(ptr), deleter_(deleter) {}
  
  ~GraphicsPtr() {
    if (ptr_ && deleter_) {
      deleter_(ptr_);
    }
  }
  
  T* get() const { return ptr_; }
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }
};
```

## Implementation Priority

### Phase 1 (High Impact, Low Risk)
1. **Palette Lookup Optimization**: Hash map for O(1) color lookups
2. **Dirty Region Tracking**: Only update changed areas
3. **Resource Pooling**: Reuse SDL textures and surfaces

### Phase 2 (Medium Impact, Medium Risk)
1. **Tile Caching System**: LRU cache for frequently used tiles
2. **Batch Operations**: Group texture updates
3. **Memory Pool Allocator**: Custom allocator for graphics data

### Phase 3 (High Impact, High Risk)
1. **Atlas-based Rendering**: Single draw calls for multiple tiles
2. **Multi-threaded Updates**: Background texture processing
3. **GPU-based Operations**: Move some operations to GPU

## Performance Metrics

### Target Improvements
- **Palette Lookup**: 100x faster (O(n) → O(1))
- **Texture Updates**: 10x faster (dirty regions)
- **Memory Usage**: 30% reduction (resource pooling)
- **Frame Rate**: 2x improvement (batch operations)

### Measurement Tools
```cpp
class PerformanceProfiler {
public:
  void StartTimer(const std::string& operation) {
    timers_[operation] = std::chrono::high_resolution_clock::now();
  }
  
  void EndTimer(const std::string& operation) {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - timers_[operation]).count();
    
    operation_times_[operation].push_back(duration);
  }
  
  void Report() {
    for (auto& [operation, times] : operation_times_) {
      double avg_time = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
      SDL_Log("Operation %s: %.2f μs average", operation.c_str(), avg_time);
    }
  }
};
```

## Conclusion

These optimizations will significantly improve the performance and responsiveness of the YAZE graphics system, particularly for ROM hacking workflows that involve frequent pixel manipulation, palette editing, and tile-based graphics editing. The phased approach ensures minimal risk while delivering substantial performance improvements.
