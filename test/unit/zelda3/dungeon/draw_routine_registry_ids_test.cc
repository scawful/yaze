// Pins GetMappedObjectIds(), the object inventory the Object Coverage panel
// lists. It must agree exactly with GetRoutineIdForObject(), which is what the
// renderer uses, or the panel would list objects the renderer cannot draw
// (or hide ones it can).

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <set>
#include <thread>
#include <vector>

#include "core/features.h"
#include "gtest/gtest.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"

namespace yaze::zelda3 {
namespace {

std::vector<int16_t> MappedIds() {
  auto& registry = DrawRoutineRegistry::Get();
  registry.Initialize();
  return registry.GetMappedObjectIds();
}

TEST(DrawRoutineRegistryIdsTest, IdsAreSortedAndUnique) {
  const std::vector<int16_t> ids = MappedIds();
  ASSERT_FALSE(ids.empty());
  EXPECT_TRUE(std::is_sorted(ids.begin(), ids.end()));
  EXPECT_EQ(std::set<int16_t>(ids.begin(), ids.end()).size(), ids.size());
}

// Every object ID the three subtypes can encode is either listed with a
// routine or unlisted and unmapped; nothing falls between.
TEST(DrawRoutineRegistryIdsTest, ListMatchesPerObjectLookup) {
  const std::vector<int16_t> ids = MappedIds();
  const std::set<int16_t> listed(ids.begin(), ids.end());
  const auto& registry = DrawRoutineRegistry::Get();
  for (int object_id = 0; object_id <= 0xFFF; ++object_id) {
    const auto id = static_cast<int16_t>(object_id);
    const bool mapped = registry.GetRoutineIdForObject(id) >= 0;
    EXPECT_EQ(listed.count(id) == 1, mapped)
        << "object 0x" << std::hex << object_id;
  }
}

// Spot checks from each subtype, so an empty or single-range list fails.
TEST(DrawRoutineRegistryIdsTest, CoversAllThreeSubtypes) {
  const std::vector<int16_t> ids = MappedIds();
  const std::set<int16_t> listed(ids.begin(), ids.end());
  EXPECT_EQ(listed.count(0x000), 1u);  // Subtype 1
  EXPECT_EQ(listed.count(0x04C), 1u);  // Subtype 1 bar
  EXPECT_EQ(listed.count(0x100), 1u);  // Subtype 2 corner
  EXPECT_EQ(listed.count(0x12D), 1u);  // Subtype 2 stairs
  EXPECT_EQ(listed.count(0xFD6), 1u);  // Subtype 3 bar corner
}

TEST(DrawRoutineRegistryIdsTest, RefreshDoesNotMutateConcurrentReadTables) {
  auto& registry = DrawRoutineRegistry::Get();
  const auto inventory = registry.GetMappedObjectIds();
  const auto* routine = registry.GetRoutineInfo(20);
  ASSERT_NE(routine, nullptr);
  const bool previous = core::FeatureFlags::get().kEnableCustomObjects;
  std::atomic<bool> stop{false};
  std::atomic<bool> mismatch{false};
  std::atomic<int> reads{0};
  std::atomic<int> started{0};
  auto reader = [&] {
    started.fetch_add(1, std::memory_order_release);
    while (!stop.load(std::memory_order_acquire)) {
      const int custom = registry.GetRoutineIdForObject(0x31);
      if (registry.GetRoutineIdForObject(0x00) != 0 ||
          registry.GetRoutineIdForObject(0x21) != 20 ||
          (custom != DrawRoutineIds::kNothing &&
           custom != DrawRoutineIds::kCustomObject) ||
          registry.GetRoutineInfo(20) != routine ||
          registry.GetMappedObjectIds() != inventory) {
        mismatch.store(true, std::memory_order_release);
      }
      reads.fetch_add(1, std::memory_order_relaxed);
    }
  };
  std::thread first(reader);
  std::thread second(reader);
  while (started.load(std::memory_order_acquire) != 2 ||
         reads.load(std::memory_order_relaxed) < 2) {
    std::this_thread::yield();
  }
  for (int i = 0; i < 1000; ++i) {
    core::FeatureFlags::get().kEnableCustomObjects = (i & 1) != 0;
    registry.RefreshFeatureFlagMappings();
    if ((i & 7) == 0)
      std::this_thread::yield();
  }
  stop.store(true, std::memory_order_release);
  first.join();
  second.join();
  core::FeatureFlags::get().kEnableCustomObjects = previous;
  registry.RefreshFeatureFlagMappings();
  EXPECT_GT(reads.load(), 0);
  EXPECT_FALSE(mismatch.load());
}

}  // namespace
}  // namespace yaze::zelda3
