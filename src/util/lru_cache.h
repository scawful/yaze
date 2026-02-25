#ifndef YAZE_UTIL_LRU_CACHE_H_
#define YAZE_UTIL_LRU_CACHE_H_

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <utility>

namespace yaze {
namespace util {

// A generic LRU (Least Recently Used) cache with configurable capacity
// and an optional eviction predicate.
//
// Usage:
//   LruCache<int, std::unique_ptr<Widget>> cache(20);
//   cache.Insert(42, std::make_unique<Widget>());
//   Widget* w = cache.Get(42);        // touches entry (moves to most recent)
//   cache.Touch(42);                  // explicit LRU refresh
//   cache.Erase(42);                  // manual removal
//
// Eviction:
//   When Insert would exceed capacity, the least recently used entry is
//   evicted. You can supply a predicate to skip entries that should not
//   be evicted (e.g., entries that are currently active/pinned):
//
//     cache.SetEvictionPredicate([&](const Key& k) {
//       return !IsActive(k);  // only evict inactive entries
//     });
//
template <typename Key, typename Value>
class LruCache {
 public:
  using EvictionPredicate = std::function<bool(const Key&)>;

  explicit LruCache(size_t capacity) : capacity_(capacity) {}

  // Insert or replace an entry. Evicts the LRU entry if over capacity.
  // Returns a pointer to the stored value.
  Value* Insert(const Key& key, Value value) {
    Erase(key);  // Remove existing entry if present
    entries_[key] = std::move(value);
    lru_.push_back(key);
    EvictIfNeeded();
    return &entries_[key];
  }

  // Get a pointer to the value, or nullptr if not found.
  // Touches the entry (moves to most recent).
  Value* Get(const Key& key) {
    auto it = entries_.find(key);
    if (it == entries_.end()) return nullptr;
    Touch(key);
    return &it->second;
  }

  // Get a pointer to the value without touching LRU order.
  Value* Peek(const Key& key) {
    auto it = entries_.find(key);
    if (it == entries_.end()) return nullptr;
    return &it->second;
  }

  // Check if a key exists in the cache.
  bool Contains(const Key& key) const {
    return entries_.find(key) != entries_.end();
  }

  // Move entry to most-recent position in LRU order.
  void Touch(const Key& key) {
    RemoveFromLru(key);
    lru_.push_back(key);
  }

  // Remove an entry from the cache.
  bool Erase(const Key& key) {
    RemoveFromLru(key);
    return entries_.erase(key) > 0;
  }

  // Transfer ownership of a key's value from old_key to new_key.
  // If old_key doesn't exist, does nothing. If new_key already exists,
  // it is replaced.
  void Rename(const Key& old_key, const Key& new_key) {
    auto it = entries_.find(old_key);
    if (it == entries_.end()) return;
    Value val = std::move(it->second);
    entries_.erase(it);
    RemoveFromLru(old_key);
    Erase(new_key);  // Remove new_key if it exists
    entries_[new_key] = std::move(val);
    lru_.push_back(new_key);
  }

  void Clear() {
    entries_.clear();
    lru_.clear();
  }

  size_t Size() const { return entries_.size(); }
  size_t Capacity() const { return capacity_; }
  bool Empty() const { return entries_.empty(); }

  void SetCapacity(size_t capacity) {
    capacity_ = capacity;
    EvictIfNeeded();
  }

  // Set a predicate that determines whether an entry CAN be evicted.
  // If the predicate returns false, the entry is skipped during eviction.
  void SetEvictionPredicate(EvictionPredicate pred) {
    eviction_predicate_ = std::move(pred);
  }

  // Iterate over all entries (key-value pairs).
  template <typename Fn>
  void ForEach(Fn&& fn) {
    for (auto& [key, value] : entries_) {
      fn(key, value);
    }
  }

  template <typename Fn>
  void ForEach(Fn&& fn) const {
    for (const auto& [key, value] : entries_) {
      fn(key, value);
    }
  }

  // Access the underlying map (for cases needing direct iteration).
  const std::map<Key, Value>& entries() const { return entries_; }
  std::map<Key, Value>& mutable_entries() { return entries_; }

  // Access the LRU order (oldest at front, newest at back).
  const std::deque<Key>& lru_order() const { return lru_; }

 private:
  void RemoveFromLru(const Key& key) {
    lru_.erase(std::remove(lru_.begin(), lru_.end(), key), lru_.end());
  }

  void EvictIfNeeded() {
    while (entries_.size() > capacity_) {
      bool evicted = false;
      for (auto it = lru_.begin(); it != lru_.end(); ++it) {
        // Check eviction predicate if set
        if (eviction_predicate_ && !eviction_predicate_(*it)) {
          continue;  // Skip protected entries
        }
        entries_.erase(*it);
        lru_.erase(it);
        evicted = true;
        break;
      }
      if (!evicted) break;  // All entries are protected; allow over-capacity
    }
  }

  size_t capacity_;
  std::map<Key, Value> entries_;
  std::deque<Key> lru_;
  EvictionPredicate eviction_predicate_;
};

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_LRU_CACHE_H_
