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

TEST(DrawRoutineBothBGMetadataTest, RegistryMarksStructuralRoutinesAsBothBG) {
  auto& registry = DrawRoutineRegistry::Get();

  EXPECT_TRUE(registry.RoutineDrawsToBothBGs(
      DrawRoutineIds::kRightwards2x2_1to15or32));  // ceiling
  EXPECT_TRUE(registry.RoutineDrawsToBothBGs(
      DrawRoutineIds::kRightwards2x4_1to15or26));  // layout walls
  EXPECT_TRUE(registry.RoutineDrawsToBothBGs(
      DrawRoutineIds::kDownwards4x2_1to15or26));  // layout walls
  EXPECT_TRUE(
      registry.RoutineDrawsToBothBGs(DrawRoutineIds::kCorner4x4));  // corners
}

TEST(DrawRoutineBothBGMetadataTest, ObjectDrawerUsesRegistryMetadata) {
  EXPECT_TRUE(TestObjectDrawer::RoutineDrawsToBothBGs(
      DrawRoutineIds::kRightwards2x4_1to15or26));
  EXPECT_TRUE(TestObjectDrawer::RoutineDrawsToBothBGs(DrawRoutineIds::kCorner4x4));

  // Sanity: a non-BothBG routine should remain false.
  EXPECT_FALSE(TestObjectDrawer::RoutineDrawsToBothBGs(
      DrawRoutineIds::kRightwards2x4_1to16));
}

}  // namespace zelda3
}  // namespace yaze
