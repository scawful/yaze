#include "gtest/gtest.h"

#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/object_drawer.h"

namespace yaze {
namespace zelda3 {

namespace {
// ObjectDrawer's BothBG helper is intentionally non-public; expose it for tests
// via a thin derived type.
struct TestObjectDrawer : public ObjectDrawer {
  using ObjectDrawer::RoutineDrawsToBothBGs;
};
}  // namespace

TEST(DrawRoutineBothBGMetadataTest, RegistryMarksUsdasmBothBGWritersAsBothBG) {
  auto& registry = DrawRoutineRegistry::Get();

  // Ground truth: usdasm bank_01.asm routines that explicitly write to both
  // $7E2000 and $7E4000 tilemaps.
  EXPECT_TRUE(registry.RoutineDrawsToBothBGs(
      DrawRoutineIds::kRightwards2x4_1to16));  // $01:8B0D (objects 0x03-0x04)
  EXPECT_TRUE(registry.RoutineDrawsToBothBGs(
      DrawRoutineIds::kDownwards4x2_1to16_BothBG));  // $01:8AA4 (0x63-0x64)
  EXPECT_TRUE(registry.RoutineDrawsToBothBGs(
      DrawRoutineIds::kDiagonalAcute_1to16_BothBG));  // $01:8C6A
  EXPECT_TRUE(registry.RoutineDrawsToBothBGs(
      DrawRoutineIds::kCorner4x4_BothBG));  // $01:9813 (Type2 0x108-0x10F)
}

TEST(DrawRoutineBothBGMetadataTest, RegistryDoesNotOverMarkPointerBasedRoutines) {
  auto& registry = DrawRoutineRegistry::Get();

  // These route through the current tilemap pointer set (single-layer).
  EXPECT_FALSE(registry.RoutineDrawsToBothBGs(
      DrawRoutineIds::kRightwards2x2_1to15or32));  // $01:8B89
  EXPECT_FALSE(registry.RoutineDrawsToBothBGs(
      DrawRoutineIds::kRightwards2x4_1to15or26));  // $01:8A92
  EXPECT_FALSE(registry.RoutineDrawsToBothBGs(
      DrawRoutineIds::kDownwards4x2_1to15or26));  // $01:8A89

  // USDASM naming quirk: $01:8C37 uses pointers (single-layer), despite suffix.
  EXPECT_FALSE(registry.RoutineDrawsToBothBGs(
      DrawRoutineIds::kRightwards2x4_1to16_BothBG));
}

TEST(DrawRoutineBothBGMetadataTest, ObjectDrawerUsesRegistryMetadata) {
  EXPECT_TRUE(TestObjectDrawer::RoutineDrawsToBothBGs(
      DrawRoutineIds::kRightwards2x4_1to16));
  EXPECT_TRUE(TestObjectDrawer::RoutineDrawsToBothBGs(
      DrawRoutineIds::kCorner4x4_BothBG));
  EXPECT_FALSE(TestObjectDrawer::RoutineDrawsToBothBGs(
      DrawRoutineIds::kRightwards2x4_1to16_BothBG));
}

}  // namespace zelda3
}  // namespace yaze
