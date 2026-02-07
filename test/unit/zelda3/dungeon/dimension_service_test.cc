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

}  // namespace yaze::zelda3
