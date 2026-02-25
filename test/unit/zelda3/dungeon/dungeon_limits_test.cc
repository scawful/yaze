#include "zelda3/dungeon/dungeon_limits.h"

#include <map>

#include <gtest/gtest.h>

namespace yaze::zelda3 {
namespace {

TEST(DungeonLimitsTest, ReturnsCanonicalMaxForCoreLimits) {
  EXPECT_EQ(GetDungeonLimitMax(DungeonLimit::kTileObjects),
            static_cast<int>(kMaxTileObjects));
  EXPECT_EQ(GetDungeonLimitMax(DungeonLimit::kSprites),
            static_cast<int>(kMaxTotalSprites));
  EXPECT_EQ(GetDungeonLimitMax(DungeonLimit::kDoors),
            static_cast<int>(kMaxDoors));
  EXPECT_EQ(GetDungeonLimitMax(DungeonLimit::kChests),
            static_cast<int>(kMaxChests));
  EXPECT_EQ(GetDungeonLimitMax(DungeonLimit::kBg3Objects),
            static_cast<int>(kMaxBg3Objects));
}

TEST(DungeonLimitsTest, ReturnsNoHardLimitForExtendedCategories) {
  EXPECT_EQ(GetDungeonLimitMax(DungeonLimit::Overlords), kNoHardLimit);
  EXPECT_EQ(GetDungeonLimitMax(DungeonLimit::SpecialDoors), kNoHardLimit);
  EXPECT_EQ(GetDungeonLimitMax(DungeonLimit::StairsTransition), kNoHardLimit);
  EXPECT_EQ(GetDungeonLimitMax(DungeonLimit::Blocks), kNoHardLimit);
}

TEST(DungeonLimitsTest, HasExceededLimitsIgnoresNoHardLimitCategories) {
  std::map<DungeonLimit, int> counts = {
      {DungeonLimit::Overlords, 99},
      {DungeonLimit::Blocks, 123},
      {DungeonLimit::kTileObjects, static_cast<int>(kMaxTileObjects)}};

  EXPECT_FALSE(HasExceededLimits(counts));
}

TEST(DungeonLimitsTest, HasExceededLimitsFlagsCoreLimitOverflow) {
  std::map<DungeonLimit, int> counts = {
      {DungeonLimit::kDoors, static_cast<int>(kMaxDoors) + 1}};

  EXPECT_TRUE(HasExceededLimits(counts));
}

TEST(DungeonLimitsTest, GetExceededLimitsReportsOnlyHardLimitedCategories) {
  std::map<DungeonLimit, int> counts = {
      {DungeonLimit::Overlords, 500},
      {DungeonLimit::kSprites, static_cast<int>(kMaxTotalSprites) + 1}};

  const auto exceeded = GetExceededLimits(counts);
  ASSERT_EQ(exceeded.size(), 1u);
  EXPECT_EQ(exceeded[0].limit, DungeonLimit::kSprites);
  EXPECT_STREQ(exceeded[0].label, "Sprites");
}

}  // namespace
}  // namespace yaze::zelda3
