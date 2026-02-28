#include "zelda3/dungeon/track_collision_generator.h"

#include <algorithm>

#include <gtest/gtest.h>

#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/room.h"

namespace yaze::zelda3 {
namespace {

int CountExpectedTilesInBounds(const RoomObject& obj) {
  const auto dims = DimensionService::Get().GetDimensions(obj);
  int count = 0;
  for (int dy = 0; dy < std::max(1, dims.height_tiles); ++dy) {
    for (int dx = 0; dx < std::max(1, dims.width_tiles); ++dx) {
      const int x = obj.x_ + dims.offset_x_tiles + dx;
      const int y = obj.y_ + dims.offset_y_tiles + dy;
      if (x >= 0 && x < 64 && y >= 0 && y < 64) {
        ++count;
      }
    }
  }
  return count;
}

TEST(TrackCollisionGeneratorTest, UsesDimensionServiceNotLegacyWidthHeight) {
  Room room;
  RoomObject track_obj(0x01, 10, 10, 0, 0);

  // Legacy width_/height_ can be stale; generator must ignore them.
  track_obj.width_ = 16;
  track_obj.height_ = 16;
  room.AddTileObject(track_obj);

  const int expected = CountExpectedTilesInBounds(track_obj);
  ASSERT_GT(expected, 0);

  GeneratorOptions options;
  options.track_object_id = 0x01;
  auto result_or = GenerateTrackCollision(&room, options);
  ASSERT_TRUE(result_or.ok()) << result_or.status();

  const auto& result = result_or.value();
  EXPECT_EQ(result.tiles_generated, expected);
}

TEST(TrackCollisionGeneratorTest, DegenerateTrackDimensionsFallbackToTwoByTwo) {
  Room room;
  RoomObject track_obj(0x31, 10, 10, 0, 0);
  room.AddTileObject(track_obj);

  auto result_or = GenerateTrackCollision(&room, GeneratorOptions{});
  ASSERT_TRUE(result_or.ok()) << result_or.status();

  const auto& result = result_or.value();
  EXPECT_EQ(result.tiles_generated, 4);

  const auto idx = [](int x, int y) {
    return static_cast<size_t>(y * 64 + x);
  };
  EXPECT_NE(result.collision_map.tiles[idx(10, 10)], 0);
  EXPECT_NE(result.collision_map.tiles[idx(11, 10)], 0);
  EXPECT_NE(result.collision_map.tiles[idx(10, 11)], 0);
  EXPECT_NE(result.collision_map.tiles[idx(11, 11)], 0);
}

TEST(TrackCollisionGeneratorTest, LegacyWidthHeightDoesNotChangeOutput) {
  Room room_a;
  RoomObject obj_a(0x31, 18, 22, 0, 0);
  obj_a.width_ = 16;
  obj_a.height_ = 16;
  room_a.AddTileObject(obj_a);

  Room room_b;
  RoomObject obj_b(0x31, 18, 22, 0, 0);
  obj_b.width_ = 1;
  obj_b.height_ = 1;
  room_b.AddTileObject(obj_b);

  auto res_a_or = GenerateTrackCollision(&room_a, GeneratorOptions{});
  auto res_b_or = GenerateTrackCollision(&room_b, GeneratorOptions{});

  ASSERT_TRUE(res_a_or.ok()) << res_a_or.status();
  ASSERT_TRUE(res_b_or.ok()) << res_b_or.status();

  const auto& res_a = res_a_or.value();
  const auto& res_b = res_b_or.value();

  EXPECT_EQ(res_a.tiles_generated, res_b.tiles_generated);
  EXPECT_EQ(res_a.stop_count, res_b.stop_count);
  EXPECT_EQ(res_a.corner_count, res_b.corner_count);
  EXPECT_EQ(res_a.switch_count, res_b.switch_count);
  EXPECT_EQ(res_a.collision_map.tiles, res_b.collision_map.tiles);
}

}  // namespace
}  // namespace yaze::zelda3
