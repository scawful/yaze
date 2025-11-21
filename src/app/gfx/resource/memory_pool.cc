#include "app/gfx/resource/memory_pool.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace yaze {
namespace gfx {

using MemoryBlock = MemoryPool::MemoryBlock;

MemoryPool& MemoryPool::Get() {
  static MemoryPool instance;
  return instance;
}

MemoryPool::MemoryPool()
    : total_allocations_(0),
      total_deallocations_(0),
      total_used_bytes_(0),
      total_allocated_bytes_(0) {
  // Initialize block pools with common graphics sizes
  InitializeBlockPool(small_blocks_, kSmallBlockSize,
                      100);  // 100KB for small tiles
  InitializeBlockPool(medium_blocks_, kMediumBlockSize,
                      50);  // 200KB for medium tiles
  InitializeBlockPool(large_blocks_, kLargeBlockSize,
                      20);  // 320KB for large tiles
  InitializeBlockPool(huge_blocks_, kHugeBlockSize,
                      10);  // 640KB for graphics sheets

  total_allocated_bytes_ = (100 * kSmallBlockSize) + (50 * kMediumBlockSize) +
                           (20 * kLargeBlockSize) + (10 * kHugeBlockSize);
}

MemoryPool::~MemoryPool() {
  Clear();
}

void* MemoryPool::Allocate(size_t size) {
  total_allocations_++;

  MemoryBlock* block = FindFreeBlock(size);
  if (!block) {
    // Fallback to system malloc if no pool block available
    void* data = std::malloc(size);
    if (data) {
      total_used_bytes_ += size;
      allocated_blocks_[data] = nullptr;  // Mark as system allocated
    }
    return data;
  }

  block->in_use = true;
  total_used_bytes_ += block->size;
  allocated_blocks_[block->data] = block;

  return block->data;
}

void MemoryPool::Deallocate(void* ptr) {
  if (!ptr)
    return;

  total_deallocations_++;

  auto it = allocated_blocks_.find(ptr);
  if (it == allocated_blocks_.end()) {
    // System allocated, use free
    std::free(ptr);
    return;
  }

  MemoryBlock* block = it->second;
  if (block) {
    block->in_use = false;
    total_used_bytes_ -= block->size;
  }

  allocated_blocks_.erase(it);
}

void* MemoryPool::AllocateAligned(size_t size, size_t alignment) {
  // For simplicity, allocate extra space and align manually
  // In a production system, you'd want more sophisticated alignment handling
  size_t aligned_size = size + alignment - 1;
  void* ptr = Allocate(aligned_size);

  if (ptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned_addr);
  }

  return nullptr;
}

std::pair<size_t, size_t> MemoryPool::GetMemoryStats() const {
  return {total_used_bytes_, total_allocated_bytes_};
}

std::pair<size_t, size_t> MemoryPool::GetAllocationStats() const {
  return {total_allocations_, total_deallocations_};
}

void MemoryPool::Clear() {
  // Reset all blocks to unused state
  for (auto& block : small_blocks_) {
    block.in_use = false;
  }
  for (auto& block : medium_blocks_) {
    block.in_use = false;
  }
  for (auto& block : large_blocks_) {
    block.in_use = false;
  }
  for (auto& block : huge_blocks_) {
    block.in_use = false;
  }

  allocated_blocks_.clear();
  total_used_bytes_ = 0;
}

MemoryBlock* MemoryPool::FindFreeBlock(size_t size) {
  // Determine which pool to use based on size
  size_t pool_index = GetPoolIndex(size);

  std::vector<MemoryBlock>* pools[] = {&small_blocks_, &medium_blocks_,
                                       &large_blocks_, &huge_blocks_};

  if (pool_index >= 4) {
    return nullptr;  // Size too large for any pool
  }

  auto& pool = *pools[pool_index];

  // Find first unused block
  auto it =
      std::find_if(pool.begin(), pool.end(),
                   [](const MemoryBlock& block) { return !block.in_use; });

  return (it != pool.end()) ? &(*it) : nullptr;
}

void MemoryPool::InitializeBlockPool(std::vector<MemoryBlock>& pool,
                                     size_t block_size, size_t count) {
  pool.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    void* data = std::malloc(block_size);
    if (data) {
      pool.emplace_back(data, block_size);
    }
  }
}

size_t MemoryPool::GetPoolIndex(size_t size) const {
  if (size <= kSmallBlockSize)
    return 0;
  if (size <= kMediumBlockSize)
    return 1;
  if (size <= kLargeBlockSize)
    return 2;
  if (size <= kHugeBlockSize)
    return 3;
  return 4;  // Too large for any pool
}

}  // namespace gfx
}  // namespace yaze
