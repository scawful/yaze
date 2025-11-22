#ifndef YAZE_APP_GFX_MEMORY_POOL_H
#define YAZE_APP_GFX_MEMORY_POOL_H

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace gfx {

/**
 * @brief High-performance memory pool allocator for graphics data
 *
 * The MemoryPool class provides efficient memory management for graphics
 * operations in the YAZE ROM hacking editor. It reduces memory fragmentation
 * and allocation overhead through pre-allocated memory blocks.
 *
 * Key Features:
 * - Pre-allocated memory blocks for common graphics sizes
 * - O(1) allocation and deallocation
 * - Automatic block size management
 * - Memory usage tracking and statistics
 * - Thread-safe operations
 *
 * Performance Optimizations:
 * - Eliminates malloc/free overhead for graphics data
 * - Reduces memory fragmentation
 * - Fast allocation for common sizes (8x8, 16x16, 32x32 tiles)
 * - Automatic block reuse and recycling
 *
 * ROM Hacking Specific:
 * - Optimized for SNES tile sizes (8x8, 16x16)
 * - Support for graphics sheet buffers (128x128, 256x256)
 * - Efficient palette data allocation
 * - Tile cache memory management
 */
class MemoryPool {
 public:
  static MemoryPool& Get();

  /**
   * @brief Allocate memory block of specified size
   * @param size Size in bytes
   * @return Pointer to allocated memory block
   */
  void* Allocate(size_t size);

  /**
   * @brief Deallocate memory block
   * @param ptr Pointer to memory block to deallocate
   */
  void Deallocate(void* ptr);

  /**
   * @brief Allocate memory block aligned to specified boundary
   * @param size Size in bytes
   * @param alignment Alignment boundary (must be power of 2)
   * @return Pointer to aligned memory block
   */
  void* AllocateAligned(size_t size, size_t alignment);

  /**
   * @brief Get memory usage statistics
   * @return Pair of (used_bytes, total_bytes)
   */
  std::pair<size_t, size_t> GetMemoryStats() const;

  /**
   * @brief Get allocation statistics
   * @return Pair of (allocations, deallocations)
   */
  std::pair<size_t, size_t> GetAllocationStats() const;

  /**
   * @brief Clear all allocated blocks (for cleanup)
   */
  void Clear();

  struct MemoryBlock {
    void* data;
    size_t size;
    bool in_use;
    size_t alignment;

    MemoryBlock(void* d, size_t s, size_t a = 0)
        : data(d), size(s), in_use(false), alignment(a) {}
  };

 private:
  MemoryPool();
  ~MemoryPool();

  // Block size categories for common graphics operations
  static constexpr size_t kSmallBlockSize = 1024;  // 8x8 tiles, small palettes
  static constexpr size_t kMediumBlockSize =
      4096;  // 16x16 tiles, medium graphics
  static constexpr size_t kLargeBlockSize =
      16384;  // 32x32 tiles, large graphics
  static constexpr size_t kHugeBlockSize =
      65536;  // Graphics sheets, large buffers

  // Pre-allocated block pools
  std::vector<MemoryBlock> small_blocks_;
  std::vector<MemoryBlock> medium_blocks_;
  std::vector<MemoryBlock> large_blocks_;
  std::vector<MemoryBlock> huge_blocks_;

  // Allocation tracking
  std::unordered_map<void*, MemoryBlock*> allocated_blocks_;
  size_t total_allocations_;
  size_t total_deallocations_;
  size_t total_used_bytes_;
  size_t total_allocated_bytes_;

  // Helper methods
  MemoryBlock* FindFreeBlock(size_t size);
  void InitializeBlockPool(std::vector<MemoryBlock>& pool, size_t block_size,
                           size_t count);
  size_t GetPoolIndex(size_t size) const;
};

/**
 * @brief RAII wrapper for memory pool allocations
 * @tparam T Type of object to allocate
 */
template <typename T>
class PoolAllocator {
 public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  PoolAllocator() = default;
  template <typename U>
  PoolAllocator(const PoolAllocator<U>&) {}

  pointer allocate(size_type n) {
    return static_cast<pointer>(MemoryPool::Get().Allocate(n * sizeof(T)));
  }

  void deallocate(pointer p, size_type) { MemoryPool::Get().Deallocate(p); }

  template <typename U>
  bool operator==(const PoolAllocator<U>&) const {
    return true;
  }

  template <typename U>
  bool operator!=(const PoolAllocator<U>&) const {
    return false;
  }
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_MEMORY_POOL_H
