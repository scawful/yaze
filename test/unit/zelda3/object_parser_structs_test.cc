#include "zelda3/dungeon/object_parser.h"

#include "gtest/gtest.h"

namespace yaze {
namespace test {

class ObjectParserStructsTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(ObjectParserStructsTest, ObjectRoutineInfoDefaultConstructor) {
  zelda3::ObjectRoutineInfo info;

  EXPECT_EQ(info.routine_ptr, 0);
  EXPECT_EQ(info.tile_ptr, 0);
  EXPECT_EQ(info.tile_count, 0);
  EXPECT_FALSE(info.is_repeatable);
  EXPECT_FALSE(info.is_orientation_dependent);
}

TEST_F(ObjectParserStructsTest, ObjectSubtypeInfoDefaultConstructor) {
  zelda3::ObjectSubtypeInfo info;

  EXPECT_EQ(info.subtype, 0);
  EXPECT_EQ(info.subtype_ptr, 0);
  EXPECT_EQ(info.routine_ptr, 0);
  EXPECT_EQ(info.max_tile_count, 0);
}

TEST_F(ObjectParserStructsTest, ObjectSizeInfoDefaultConstructor) {
  zelda3::ObjectSizeInfo info;

  EXPECT_EQ(info.width_tiles, 0);
  EXPECT_EQ(info.height_tiles, 0);
  EXPECT_TRUE(info.is_horizontal);
  EXPECT_FALSE(info.is_repeatable);
  EXPECT_EQ(info.repeat_count, 1);
}

TEST_F(ObjectParserStructsTest, ObjectRoutineInfoAssignment) {
  zelda3::ObjectRoutineInfo info;

  info.routine_ptr = 0x12345;
  info.tile_ptr = 0x67890;
  info.tile_count = 8;
  info.is_repeatable = true;
  info.is_orientation_dependent = true;

  EXPECT_EQ(info.routine_ptr, 0x12345);
  EXPECT_EQ(info.tile_ptr, 0x67890);
  EXPECT_EQ(info.tile_count, 8);
  EXPECT_TRUE(info.is_repeatable);
  EXPECT_TRUE(info.is_orientation_dependent);
}

TEST_F(ObjectParserStructsTest, ObjectSubtypeInfoAssignment) {
  zelda3::ObjectSubtypeInfo info;

  info.subtype = 2;
  info.subtype_ptr = 0x83F0;
  info.routine_ptr = 0x8470;
  info.max_tile_count = 16;

  EXPECT_EQ(info.subtype, 2);
  EXPECT_EQ(info.subtype_ptr, 0x83F0);
  EXPECT_EQ(info.routine_ptr, 0x8470);
  EXPECT_EQ(info.max_tile_count, 16);
}

TEST_F(ObjectParserStructsTest, ObjectSizeInfoAssignment) {
  zelda3::ObjectSizeInfo info;

  info.width_tiles = 4;
  info.height_tiles = 2;
  info.is_horizontal = false;
  info.is_repeatable = true;
  info.repeat_count = 3;

  EXPECT_EQ(info.width_tiles, 4);
  EXPECT_EQ(info.height_tiles, 2);
  EXPECT_FALSE(info.is_horizontal);
  EXPECT_TRUE(info.is_repeatable);
  EXPECT_EQ(info.repeat_count, 3);
}

}  // namespace test
}  // namespace yaze