#ifndef YAZE_APP_CORE_PLATFORM_MEMORY_TRACKER_H
#define YAZE_APP_CORE_PLATFORM_MEMORY_TRACKER_H

#include <SDL.h>

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace yaze {
namespace core {

class MemoryTracker {
 public:
  static MemoryTracker& GetInstance() {
    static MemoryTracker instance;
    return instance;
  }

  void TrackAllocation(const void* ptr, size_t size, const char* type) {
    std::lock_guard<std::mutex> lock(mutex_);
    allocations_[ptr] = {size, type};
    total_allocated_ += size;
  }

  void TrackDeallocation(const void* ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = allocations_.find(ptr);
    if (it != allocations_.end()) {
      total_allocated_ -= it->second.size;
      allocations_.erase(it);
    }
  }

  size_t GetTotalAllocated() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_allocated_;
  }

  void DumpAllocations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    SDL_Log("Memory allocations: %zu bytes in %zu allocations",
            total_allocated_, allocations_.size());

    std::unordered_map<std::string, size_t> type_counts;
    for (const auto& pair : allocations_) {
      type_counts[pair.second.type] += pair.second.size;
    }

    for (const auto& pair : type_counts) {
      SDL_Log("  %s: %zu bytes", pair.first.c_str(), pair.second);
    }
  }

  // Check if the memory was freed by another reference
  bool IsFreed(const void* ptr) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return allocations_.find(ptr) == allocations_.end();
  }

 private:
  MemoryTracker() = default;

  struct AllocationInfo {
    size_t size;
    const char* type;
  };

  std::unordered_map<const void*, AllocationInfo> allocations_;
  size_t total_allocated_ = 0;
  mutable std::mutex mutex_;
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_PLATFORM_MEMORY_TRACKER_H
