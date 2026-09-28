// Pins GetMappedObjectIds(), the object inventory the Object Coverage panel
// lists. It must agree exactly with GetRoutineIdForObject(), which is what the
// renderer uses, or the panel would list objects the renderer cannot draw
// (or hide ones it can).

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

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

}  // namespace
}  // namespace yaze::zelda3
