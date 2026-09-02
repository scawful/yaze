#include "zelda3/dungeon/draw_routines/draw_routine_symbology.h"

#include "gtest/gtest.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"

namespace yaze {
namespace zelda3 {
namespace {

TEST(DrawRoutineSymbologyTest, RightwardsRoutineUsesDirectionBadge) {
  const DrawRoutineSymbology symbology =
      GetSymbologyForObject(0x001);
  EXPECT_EQ(symbology.badge, ">");
  EXPECT_EQ(symbology.family, "Rightwards");
  EXPECT_FALSE(symbology.dual_layer);
}

TEST(DrawRoutineSymbologyTest, BothBgRoutineAppendsDualLayerMarker) {
  const DrawRoutineSymbology symbology =
      GetSymbologyForObject(0x015);
  EXPECT_EQ(symbology.badge, "/2");
  EXPECT_EQ(symbology.family, "Diagonal (BothBG)");
  EXPECT_TRUE(symbology.dual_layer);
}

TEST(DrawRoutineSymbologyTest, SuperSquareRoutineUsesLargeFixedBadge) {
  const DrawRoutineSymbology symbology =
      GetSymbologyForObject(0x0C5);
  EXPECT_EQ(symbology.badge, "4");
  EXPECT_EQ(symbology.family, "Large fixed");
  EXPECT_TRUE(symbology.large_fixed);
}

TEST(DrawRoutineSymbologyTest, ChestRoutineUsesChestBadge) {
  const DrawRoutineSymbology symbology =
      GetSymbologyForObject(0xF99);
  EXPECT_EQ(symbology.badge, "C");
  EXPECT_EQ(symbology.family, "Chest");
}

TEST(DrawRoutineSymbologyTest, BombableFloorUsesDedicatedBadge) {
  const DrawRoutineSymbology symbology =
      GetSymbologyForObject(0xFC7);
  EXPECT_EQ(symbology.badge, "B");
  EXPECT_EQ(symbology.family, "Bombable floor");
}

TEST(DrawRoutineSymbologyTest, UnmappedObjectFallsBackToUnknown) {
  const DrawRoutineSymbology symbology =
      GetSymbologyForObject(0x0140);
  EXPECT_EQ(symbology.badge, "?");
  EXPECT_EQ(symbology.family, "Unmapped");
}

}  // namespace
}  // namespace zelda3
}  // namespace yaze
