# Overworld::Load Performance Analysis and Optimization Plan

## Current Performance Profile

Based on the performance report, `Overworld::Load` takes **2887.91ms (2.9 seconds)**, making it the primary bottleneck in ROM loading.

## Detailed Analysis of Overworld::Load

### Current Implementation Breakdown

```cpp
absl::Status Overworld::Load(Rom* rom) {
  // 1. Tile Assembly (CPU-bound)
  RETURN_IF_ERROR(AssembleMap32Tiles());     // ~200-400ms
  RETURN_IF_ERROR(AssembleMap16Tiles());     // ~100-200ms
  
  // 2. Decompression (CPU-bound, memory-intensive)
  DecompressAllMapTiles();                   // ~1500-2000ms (MAJOR BOTTLENECK)
  
  // 3. Map Object Creation (fast)
  for (int map_index = 0; map_index < kNumOverworldMaps; ++map_index)
    overworld_maps_.emplace_back(map_index, rom_);
  
  // 4. Map Parent Assignment (fast)
  for (int map_index = 0; map_index < kNumOverworldMaps; ++map_index) {
    map_parent_[map_index] = overworld_maps_[map_index].parent();
  }
  
  // 5. Map Size Assignment (fast)
  if (asm_version >= 3) {
    AssignMapSizes(overworld_maps_);
  } else {
    FetchLargeMaps();
  }
  
  // 6. Data Loading (moderate)
  LoadTileTypes();                           // ~50-100ms
  RETURN_IF_ERROR(LoadEntrances());          // ~100-200ms
  RETURN_IF_ERROR(LoadHoles());              // ~50ms
  RETURN_IF_ERROR(LoadExits());              // ~100-200ms
  RETURN_IF_ERROR(LoadItems());              // ~100-200ms
  RETURN_IF_ERROR(LoadOverworldMaps());      // ~200-500ms (already parallelized)
  RETURN_IF_ERROR(LoadSprites());            // ~200-400ms
}
```

## Major Bottlenecks Identified

### 1. **DecompressAllMapTiles() - PRIMARY BOTTLENECK (~1.5-2.0 seconds)**

**Current Implementation Issues:**
- Sequential processing of 160 overworld maps
- Each map calls `HyruleMagicDecompress()` twice (high/low pointers)
- 320 decompression operations total
- Each decompression involves complex algorithm with nested loops

**Performance Impact:**
```cpp
for (int i = 0; i < kNumOverworldMaps; i++) {  // 160 iterations
  // Two expensive decompression calls per map
  auto bytes = gfx::HyruleMagicDecompress(rom()->data() + p2, &size1, 1);   // ~5-10ms each
  auto bytes2 = gfx::HyruleMagicDecompress(rom()->data() + p1, &size2, 1);  // ~5-10ms each
  OrganizeMapTiles(bytes, bytes2, i, sx, sy, ttpos);  // ~2-5ms each
}
```

### 2. **AssembleMap32Tiles() - SECONDARY BOTTLENECK (~200-400ms)**

**Current Implementation Issues:**
- Sequential processing of tile32 data
- Multiple ROM reads per tile
- Complex tile assembly logic

### 3. **AssembleMap16Tiles() - MODERATE BOTTLENECK (~100-200ms)**

**Current Implementation Issues:**
- Sequential processing of tile16 data
- Multiple ROM reads per tile
- Tile info processing

## Optimization Strategies

### 1. **Parallelize Decompression Operations**

**Strategy:** Process multiple maps concurrently during decompression

```cpp
absl::Status DecompressAllMapTilesParallel() {
  constexpr int kMaxConcurrency = std::thread::hardware_concurrency();
  constexpr int kMapsPerBatch = kNumOverworldMaps / kMaxConcurrency;
  
  std::vector<std::future<void>> futures;
  
  for (int batch = 0; batch < kMaxConcurrency; ++batch) {
    auto task = [this, batch, kMapsPerBatch]() {
      int start = batch * kMapsPerBatch;
      int end = std::min(start + kMapsPerBatch, kNumOverworldMaps);
      
      for (int i = start; i < end; ++i) {
        // Process map i decompression
        ProcessMapDecompression(i);
      }
    };
    futures.emplace_back(std::async(std::launch::async, task));
  }
  
  // Wait for all batches to complete
  for (auto& future : futures) {
    future.wait();
  }
  
  return absl::OkStatus();
}
```

**Expected Improvement:** 60-80% reduction in decompression time (2.0s → 0.4-0.8s)

### 2. **Optimize ROM Access Patterns**

**Strategy:** Batch ROM reads and cache frequently accessed data

```cpp
// Cache ROM data in memory to reduce I/O overhead
class RomDataCache {
 private:
  std::unordered_map<uint32_t, std::vector<uint8_t>> cache_;
  const Rom* rom_;
  
 public:
  const std::vector<uint8_t>& GetData(uint32_t offset, size_t size) {
    auto it = cache_.find(offset);
    if (it == cache_.end()) {
      auto data = rom_->ReadBytes(offset, size);
      cache_[offset] = std::move(data);
      return cache_[offset];
    }
    return it->second;
  }
};
```

**Expected Improvement:** 10-20% reduction in ROM access time

### 3. **Implement Lazy Map Loading**

**Strategy:** Only load maps that are immediately needed

```cpp
absl::Status Overworld::LoadEssentialMaps() {
  // Only load first few maps initially
  constexpr int kInitialMapCount = 8;
  
  RETURN_IF_ERROR(AssembleMap32Tiles());
  RETURN_IF_ERROR(AssembleMap16Tiles());
  
  // Load only essential maps
  DecompressEssentialMaps(kInitialMapCount);
  
  // Load remaining maps in background
  StartBackgroundMapLoading();
  
  return absl::OkStatus();
}
```

**Expected Improvement:** 70-80% reduction in initial loading time (2.9s → 0.6-0.9s)

### 4. **Optimize HyruleMagicDecompress**

**Strategy:** Profile and optimize the decompression algorithm

**Current Algorithm Complexity:**
- Nested loops with O(n²) complexity in worst case
- Multiple memory allocations and reallocations
- String matching operations

**Potential Optimizations:**
- Pre-allocate buffers to avoid reallocations
- Optimize string matching with better algorithms
- Use SIMD instructions for bulk operations
- Cache decompression results for identical data

**Expected Improvement:** 20-40% reduction in decompression time

### 5. **Memory Pool Optimization**

**Strategy:** Use memory pools for frequent allocations

```cpp
class DecompressionMemoryPool {
 private:
  std::vector<std::unique_ptr<uint8_t[]>> buffers_;
  size_t buffer_size_;
  
 public:
  uint8_t* AllocateBuffer(size_t size) {
    // Reuse existing buffers or allocate new ones
    if (size <= buffer_size_) {
      // Return existing buffer
    } else {
      // Allocate new buffer
    }
  }
  
  void ReleaseBuffer(uint8_t* buffer) {
    // Return buffer to pool
  }
};
```

## Implementation Priority

### Phase 1: High Impact, Low Risk (Immediate)
1. **Parallelize DecompressAllMapTiles** - Biggest performance gain
2. **Implement lazy loading for non-essential maps**
3. **Add performance monitoring to identify remaining bottlenecks**

### Phase 2: Medium Impact, Medium Risk (Next)
1. **Optimize ROM access patterns**
2. **Implement memory pooling for decompression**
3. **Profile and optimize HyruleMagicDecompress**

### Phase 3: Lower Impact, Higher Risk (Future)
1. **Rewrite decompression algorithm with SIMD**
2. **Implement advanced caching strategies**
3. **Consider alternative data formats for faster loading**

## Expected Performance Improvements

### Conservative Estimates
- **Current:** 2887ms total loading time
- **After Phase 1:** 800-1200ms (60-70% improvement)
- **After Phase 2:** 500-800ms (70-80% improvement)
- **After Phase 3:** 300-500ms (80-85% improvement)

### Aggressive Estimates
- **Current:** 2887ms total loading time
- **After Phase 1:** 600-900ms (70-80% improvement)
- **After Phase 2:** 300-500ms (80-85% improvement)
- **After Phase 3:** 200-400ms (85-90% improvement)

## Conclusion

The primary optimization opportunity is in `DecompressAllMapTiles()`, which represents the majority of the loading time. By implementing parallel processing and lazy loading, we can achieve significant performance improvements while maintaining code reliability.

The optimizations should focus on:
1. **Parallelization** of CPU-bound operations
2. **Lazy loading** of non-essential data
3. **Memory optimization** to reduce allocation overhead
4. **ROM access optimization** to reduce I/O bottlenecks

These changes will dramatically improve the user experience during ROM loading while maintaining the same functionality and data integrity.
