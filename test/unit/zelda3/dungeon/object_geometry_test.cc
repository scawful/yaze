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

TEST(ObjectGeometryTest, MeasureByObjectIdKnownObject) {
  // Object 0x00 -> routine 0 (Rightwards2x2_1to15or32)
  RoomObject obj(/*id=*/0x00, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
  ASSERT_TRUE(bounds.ok());
  EXPECT_EQ(bounds->width_tiles, 64);   // 32 repeats × 2 tiles wide
  EXPECT_EQ(bounds->height_tiles, 2);
}

TEST(ObjectGeometryTest, MeasureByObjectIdUnmappedReturnsError) {
  RoomObject obj(/*id=*/0xF8, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
  EXPECT_FALSE(bounds.ok());
}

TEST(ObjectGeometryTest, MeasureByObjectIdCachePopulated) {
  ObjectGeometry::Get().ClearCache();
  RoomObject obj(/*id=*/0x60, /*x=*/0, /*y=*/0, /*size=*/3);

  // First call: measures
  auto bounds1 = ObjectGeometry::Get().MeasureByObjectId(obj);
  ASSERT_TRUE(bounds1.ok());

  // Second call: should return cached result (same values)
  auto bounds2 = ObjectGeometry::Get().MeasureByObjectId(obj);
  ASSERT_TRUE(bounds2.ok());
  EXPECT_EQ(bounds1->width_tiles, bounds2->width_tiles);
  EXPECT_EQ(bounds1->height_tiles, bounds2->height_tiles);
}

TEST(ObjectGeometryTest, ClearCacheWorks) {
  ObjectGeometry::Get().ClearCache();
  RoomObject obj(/*id=*/0x00, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
  ASSERT_TRUE(bounds.ok());

  ObjectGeometry::Get().ClearCache();
  // Should still work after clearing
  auto bounds2 = ObjectGeometry::Get().MeasureByObjectId(obj);
  ASSERT_TRUE(bounds2.ok());
  EXPECT_EQ(bounds->width_tiles, bounds2->width_tiles);
}

}  // namespace yaze::zelda3
