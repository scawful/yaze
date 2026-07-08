#include "zelda3/dungeon/dimension_service.h"

#include "gtest/gtest.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {

TEST(DimensionServiceTest, SingletonAccess) {
  auto& svc1 = DimensionService::Get();
  auto& svc2 = DimensionService::Get();
  EXPECT_EQ(&svc1, &svc2);
}

TEST(DimensionServiceTest, KnownObjectReturnsDimensions) {
  // Object 0x00 (rightwards 2x2 wall, size 0 -> 32 repeats)
  RoomObject obj(/*id=*/0x00, /*x=*/5, /*y=*/10, /*size=*/0);
  auto result = DimensionService::Get().GetDimensions(obj);
  EXPECT_GT(result.width_tiles, 0);
  EXPECT_GT(result.height_tiles, 0);
  EXPECT_EQ(result.width_pixels(), result.width_tiles * 8);
  EXPECT_EQ(result.height_pixels(), result.height_tiles * 8);
}

TEST(DimensionServiceTest, GetPixelDimensionsMatchesGetDimensions) {
  RoomObject obj(/*id=*/0x60, /*x=*/0, /*y=*/0, /*size=*/3);
  auto result = DimensionService::Get().GetDimensions(obj);
  auto [w_px, h_px] = DimensionService::Get().GetPixelDimensions(obj);
  EXPECT_EQ(w_px, result.width_pixels());
  EXPECT_EQ(h_px, result.height_pixels());
}

TEST(DimensionServiceTest, GetHitTestBoundsIncludesPosition) {
  RoomObject obj(/*id=*/0x00, /*x=*/10, /*y=*/20, /*size=*/1);
  auto [x, y, w, h] = DimensionService::Get().GetHitTestBounds(obj);
  // x/y should include the object position plus any offset
  EXPECT_GE(x, 10);
  EXPECT_GE(y, 20);
  EXPECT_GT(w, 0);
  EXPECT_GT(h, 0);
}

TEST(DimensionServiceTest, GetSelectionBoundsPixelsUsesPixelUnits) {
  RoomObject obj(/*id=*/0x00, /*x=*/5, /*y=*/5, /*size=*/1);
  auto [x_px, y_px, w_px, h_px] =
      DimensionService::Get().GetSelectionBoundsPixels(obj);
  // Pixel values should be multiples of 8 (tile size)
  EXPECT_EQ(x_px % 8, 0);
  EXPECT_EQ(y_px % 8, 0);
  EXPECT_EQ(w_px % 8, 0);
  EXPECT_EQ(h_px % 8, 0);
  EXPECT_GT(w_px, 0);
  EXPECT_GT(h_px, 0);
}

TEST(DimensionServiceTest,
     DiagonalCeilingSelectionBoundsUseAnchorAwareOffsets) {
  RoomObject obj(/*id=*/0xA3, /*x=*/10, /*y=*/12, /*size=*/0);

  auto [hit_x, hit_y, hit_w, hit_h] =
      DimensionService::Get().GetHitTestBounds(obj);
  auto [sel_x_px, sel_y_px, sel_w_px, sel_h_px] =
      DimensionService::Get().GetSelectionBoundsPixels(obj);

  // Bottom-right diagonal ceilings extend up-left from the origin. Hit testing
  // and selection should honor those negative offsets rather than clamping to
  // the object origin.
  EXPECT_LT(hit_x, obj.x_);
  EXPECT_LT(hit_y, obj.y_);
  EXPECT_EQ(sel_x_px, hit_x * 8);
  EXPECT_EQ(sel_y_px, hit_y * 8);
  EXPECT_EQ(sel_w_px, hit_w * 8);
  EXPECT_EQ(sel_h_px, hit_h * 8);
}

TEST(DimensionServiceTest, EmptyGeometryFallsBackToDimensionTableBounds) {
  RoomObject obj(/*id=*/0xFF3, /*x=*/4, /*y=*/6, /*size=*/0);

  auto [hit_x, hit_y, hit_w, hit_h] =
      DimensionService::Get().GetHitTestBounds(obj);
  auto [sel_x, sel_y, sel_w, sel_h] =
      DimensionService::Get().GetSelectionBoundsPixels(obj);

  EXPECT_GT(hit_w, 0);
  EXPECT_GT(hit_h, 0);
  EXPECT_GT(sel_w, 0);
  EXPECT_GT(sel_h, 0);
  EXPECT_EQ(sel_x, hit_x * 8);
  EXPECT_EQ(sel_y, hit_y * 8);
  EXPECT_EQ(sel_w, hit_w * 8);
  EXPECT_EQ(sel_h, hit_h * 8);
}

TEST(DimensionServiceTest, UnmappedObjectStillReturnsSomething) {
  // Object 0xF8 is unmapped in ObjectGeometry
  RoomObject obj(/*id=*/0xF8, /*x=*/0, /*y=*/0, /*size=*/0);
  auto result = DimensionService::Get().GetDimensions(obj);
  EXPECT_GT(result.width_tiles, 0);
  EXPECT_GT(result.height_tiles, 0);
}

TEST(DimensionServiceTest, VerticalObjectHasTallDimensions) {
  // Object 0x60 is a downwards 2x2 wall
  RoomObject obj(/*id=*/0x60, /*x=*/0, /*y=*/0, /*size=*/5);
  auto result = DimensionService::Get().GetDimensions(obj);
  EXPECT_GT(result.height_tiles, result.width_tiles);
}

// ObjectDrawer uses DimensionService::GetSelectionBoundsPixels to decide how
// much of BG1 to mark transparent for pit/mask objects. The invariant these
// tests pin: that call must return the same rectangle (in pixels) as
// GetHitTestBounds (which drives the editor's selection outline), so the
// transparent cutout always lines up with what the user selected.
class MaskGeometryParityTest : public ::testing::TestWithParam<int> {};

TEST_P(MaskGeometryParityTest, SelectionAndHitTestAgreeInPixels) {
  const int id = GetParam();
  RoomObject obj(id, /*x=*/4, /*y=*/6, /*size=*/0);

  const auto [hit_x, hit_y, hit_w, hit_h] =
      DimensionService::Get().GetHitTestBounds(obj);
  const auto [sel_x, sel_y, sel_w, sel_h] =
      DimensionService::Get().GetSelectionBoundsPixels(obj);

  EXPECT_GT(sel_w, 0) << "mask 0x" << std::hex << id << " has zero width";
  EXPECT_GT(sel_h, 0) << "mask 0x" << std::hex << id << " has zero height";
  EXPECT_EQ(sel_x, hit_x * 8);
  EXPECT_EQ(sel_y, hit_y * 8);
  EXPECT_EQ(sel_w, hit_w * 8);
  EXPECT_EQ(sel_h, hit_h * 8);
}

INSTANTIATE_TEST_SUITE_P(
    PitAndCeilingMasks, MaskGeometryParityTest,
    // Mirrors ObjectDrawer::RequiresRectangularBg1Mask. Keep in sync when that
    // list changes — new masks should satisfy the same parity invariant.
    ::testing::Values(0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xC0, 0xC2, 0xC3, 0xC6,
                      0xC8, 0xD7, 0xD8, 0xD9, 0xDA, 0xFE6, 0xFF3));

}  // namespace yaze::zelda3
