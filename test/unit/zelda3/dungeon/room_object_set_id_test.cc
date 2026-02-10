#include "gtest/gtest.h"

#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

TEST(RoomObjectSetIdTest, RecomputesAllBgsAndInvalidatesTileCache) {
  RoomObject obj(/*id=*/0x21, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);

  // Seed state so we can validate that changing the ID invalidates caches.
  obj.tiles_loaded_ = true;
  obj.tile_count_ = 123;
  obj.tile_data_ptr_ = 456;
  obj.tiles_.push_back(gfx::TileInfo{});
  obj.all_bgs_ = false;

  obj.set_id(/*id=*/0x0C);

  EXPECT_EQ(obj.id_, 0x0C);
  EXPECT_TRUE(obj.all_bgs_);
  EXPECT_FALSE(obj.tiles_loaded_);
  EXPECT_TRUE(obj.tiles_.empty());
  EXPECT_EQ(obj.tile_count_, 0);
  EXPECT_EQ(obj.tile_data_ptr_, -1);
}

TEST(RoomObjectSetIdTest, NoOpWhenIdUnchanged) {
  RoomObject obj(/*id=*/0x21, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);

  obj.tiles_loaded_ = true;
  obj.tile_count_ = 1;
  obj.tile_data_ptr_ = 1;
  obj.tiles_.push_back(gfx::TileInfo{});
  obj.all_bgs_ = true;  // simulate custom/manual override

  obj.set_id(/*id=*/0x21);

  EXPECT_EQ(obj.id_, 0x21);
  EXPECT_TRUE(obj.tiles_loaded_);
  EXPECT_FALSE(obj.tiles_.empty());
  EXPECT_EQ(obj.tile_count_, 1);
  EXPECT_EQ(obj.tile_data_ptr_, 1);
  EXPECT_TRUE(obj.all_bgs_);
}

}  // namespace zelda3
}  // namespace yaze

