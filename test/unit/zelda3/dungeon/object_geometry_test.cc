#include "zelda3/dungeon/geometry/object_geometry.h"

#include "gtest/gtest.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {

TEST(ObjectGeometryTest, Rightwards2x2UsesThirtyTwoWhenSizeZero) {
  RoomObject obj(/*id=*/0x00, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByRoutineId(/*routine_id=*/0, obj);
  ASSERT_TRUE(bounds.ok());
  EXPECT_EQ(bounds->min_x_tiles, 0);
  EXPECT_EQ(bounds->min_y_tiles, 0);
  EXPECT_EQ(bounds->width_tiles, 64);   // 32 repeats × 2 tiles wide
  EXPECT_EQ(bounds->height_tiles, 2);   // 2 tiles tall
  EXPECT_EQ(bounds->width_pixels(), 512);
  EXPECT_EQ(bounds->height_pixels(), 16);
}

TEST(ObjectGeometryTest, Downwards2x2UsesThirtyTwoWhenSizeZero) {
  RoomObject obj(/*id=*/0x60, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByRoutineId(/*routine_id=*/7, obj);
  ASSERT_TRUE(bounds.ok());
  EXPECT_EQ(bounds->min_x_tiles, 0);
  EXPECT_EQ(bounds->min_y_tiles, 0);
  EXPECT_EQ(bounds->width_tiles, 2);    // 2 tiles wide
  EXPECT_EQ(bounds->height_tiles, 64);  // 32 repeats × 2 tiles tall
}

TEST(ObjectGeometryTest, DiagonalAcuteExtendsUpward) {
  RoomObject obj(/*id=*/0x09, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByRoutineId(/*routine_id=*/5, obj);
  ASSERT_TRUE(bounds.ok());
  EXPECT_EQ(bounds->width_tiles, 7);    // count = size + 7
  EXPECT_EQ(bounds->height_tiles, 11);  // count + 4 rows
  EXPECT_EQ(bounds->min_x_tiles, 0);
  EXPECT_EQ(bounds->min_y_tiles, -6);   // routine walks upward from origin
}

}  // namespace yaze::zelda3
