#include "util/lru_cache.h"

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace yaze {
namespace util {
namespace {

TEST(LruCacheTest, BasicInsertAndGet) {
  LruCache<int, std::string> cache(3);
  cache.Insert(1, "one");
  cache.Insert(2, "two");
  cache.Insert(3, "three");

  EXPECT_EQ(*cache.Get(1), "one");
  EXPECT_EQ(*cache.Get(2), "two");
  EXPECT_EQ(*cache.Get(3), "three");
  EXPECT_EQ(cache.Size(), 3u);
}

TEST(LruCacheTest, GetReturnsNullptrForMissing) {
  LruCache<int, std::string> cache(3);
  EXPECT_EQ(cache.Get(42), nullptr);
}

TEST(LruCacheTest, EvictsLeastRecentlyUsed) {
  LruCache<int, std::string> cache(2);
  cache.Insert(1, "one");
  cache.Insert(2, "two");
  // Cache is full. Inserting 3 should evict 1 (oldest).
  cache.Insert(3, "three");

  EXPECT_EQ(cache.Get(1), nullptr);
  EXPECT_EQ(*cache.Get(2), "two");
  EXPECT_EQ(*cache.Get(3), "three");
  EXPECT_EQ(cache.Size(), 2u);
}

TEST(LruCacheTest, TouchUpdatesAccessOrder) {
  LruCache<int, std::string> cache(2);
  cache.Insert(1, "one");
  cache.Insert(2, "two");
  // Touch 1 so it becomes most recent. Inserting 3 evicts 2 instead.
  cache.Touch(1);
  cache.Insert(3, "three");

  EXPECT_EQ(*cache.Get(1), "one");
  EXPECT_EQ(cache.Get(2), nullptr);
  EXPECT_EQ(*cache.Get(3), "three");
}

TEST(LruCacheTest, GetTouchesEntry) {
  LruCache<int, std::string> cache(2);
  cache.Insert(1, "one");
  cache.Insert(2, "two");
  // Get(1) should touch it. Inserting 3 evicts 2 instead.
  cache.Get(1);
  cache.Insert(3, "three");

  EXPECT_EQ(*cache.Get(1), "one");
  EXPECT_EQ(cache.Get(2), nullptr);
}

TEST(LruCacheTest, PeekDoesNotTouch) {
  LruCache<int, std::string> cache(2);
  cache.Insert(1, "one");
  cache.Insert(2, "two");
  // Peek(1) should NOT touch it. Inserting 3 evicts 1.
  cache.Peek(1);
  cache.Insert(3, "three");

  EXPECT_EQ(cache.Get(1), nullptr);
  EXPECT_EQ(*cache.Get(2), "two");
  EXPECT_EQ(*cache.Get(3), "three");
}

TEST(LruCacheTest, Erase) {
  LruCache<int, std::string> cache(3);
  cache.Insert(1, "one");
  cache.Insert(2, "two");

  EXPECT_TRUE(cache.Erase(1));
  EXPECT_EQ(cache.Get(1), nullptr);
  EXPECT_EQ(cache.Size(), 1u);
  EXPECT_FALSE(cache.Erase(42));  // Non-existent
}

TEST(LruCacheTest, Rename) {
  LruCache<int, std::string> cache(3);
  cache.Insert(1, "one");
  cache.Insert(2, "two");

  cache.Rename(1, 10);
  EXPECT_EQ(cache.Get(1), nullptr);
  EXPECT_EQ(*cache.Get(10), "one");
  EXPECT_EQ(cache.Size(), 2u);
}

TEST(LruCacheTest, InsertReplacesExisting) {
  LruCache<int, std::string> cache(3);
  cache.Insert(1, "one");
  cache.Insert(1, "ONE");

  EXPECT_EQ(*cache.Get(1), "ONE");
  EXPECT_EQ(cache.Size(), 1u);
}

TEST(LruCacheTest, Clear) {
  LruCache<int, std::string> cache(3);
  cache.Insert(1, "one");
  cache.Insert(2, "two");
  cache.Clear();

  EXPECT_TRUE(cache.Empty());
  EXPECT_EQ(cache.Size(), 0u);
  EXPECT_EQ(cache.Get(1), nullptr);
}

TEST(LruCacheTest, Contains) {
  LruCache<int, std::string> cache(3);
  cache.Insert(1, "one");
  EXPECT_TRUE(cache.Contains(1));
  EXPECT_FALSE(cache.Contains(2));
}

TEST(LruCacheTest, EvictionPredicateSkipsProtected) {
  LruCache<int, std::string> cache(2);
  // Protect key 1 from eviction
  cache.SetEvictionPredicate([](const int& key) { return key != 1; });

  cache.Insert(1, "one");
  cache.Insert(2, "two");
  // Should evict 2 (not 1 which is protected), even though 1 is older
  cache.Insert(3, "three");

  EXPECT_EQ(*cache.Get(1), "one");
  EXPECT_EQ(cache.Get(2), nullptr);
  EXPECT_EQ(*cache.Get(3), "three");
}

TEST(LruCacheTest, AllProtectedAllowsOverCapacity) {
  LruCache<int, std::string> cache(2);
  // Protect everything
  cache.SetEvictionPredicate([](const int&) { return false; });

  cache.Insert(1, "one");
  cache.Insert(2, "two");
  cache.Insert(3, "three");

  // All entries survive because none can be evicted
  EXPECT_EQ(cache.Size(), 3u);
  EXPECT_EQ(*cache.Get(1), "one");
  EXPECT_EQ(*cache.Get(2), "two");
  EXPECT_EQ(*cache.Get(3), "three");
}

TEST(LruCacheTest, ForEachIteratesAll) {
  LruCache<int, std::string> cache(3);
  cache.Insert(1, "one");
  cache.Insert(2, "two");
  cache.Insert(3, "three");

  std::vector<int> keys;
  cache.ForEach([&](int key, const std::string&) { keys.push_back(key); });
  EXPECT_EQ(keys.size(), 3u);
}

TEST(LruCacheTest, WorksWithUniquePtr) {
  LruCache<int, std::unique_ptr<int>> cache(2);
  cache.Insert(1, std::make_unique<int>(100));
  cache.Insert(2, std::make_unique<int>(200));

  auto* val = cache.Get(1);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(**val, 100);

  // Get(1) touched it, so 2 is now oldest. Inserting 3 evicts 2.
  cache.Insert(3, std::make_unique<int>(300));
  EXPECT_NE(cache.Get(1), nullptr);   // 1 was touched, still cached
  EXPECT_EQ(cache.Get(2), nullptr);   // 2 was oldest, evicted
  EXPECT_NE(cache.Get(3), nullptr);   // 3 was just inserted
}

TEST(LruCacheTest, CapacityOfOne) {
  LruCache<int, std::string> cache(1);
  cache.Insert(1, "one");
  cache.Insert(2, "two");

  EXPECT_EQ(cache.Get(1), nullptr);
  EXPECT_EQ(*cache.Get(2), "two");
  EXPECT_EQ(cache.Size(), 1u);
}

TEST(LruCacheTest, SetCapacityShrinksAndEvictsImmediately) {
  LruCache<int, std::string> cache(3);
  cache.Insert(1, "one");
  cache.Insert(2, "two");
  cache.Insert(3, "three");

  cache.SetCapacity(2);

  EXPECT_EQ(cache.Size(), 2u);
  EXPECT_EQ(cache.Get(1), nullptr);
  EXPECT_EQ(*cache.Get(2), "two");
  EXPECT_EQ(*cache.Get(3), "three");
}

}  // namespace
}  // namespace util
}  // namespace yaze
