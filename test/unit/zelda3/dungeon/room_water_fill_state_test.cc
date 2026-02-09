#include "gtest/gtest.h"

#include "zelda3/dungeon/room.h"

namespace yaze::zelda3 {
namespace {

TEST(RoomWaterFillStateTest, StartsEmptyAndNotDirty) {
  Room room;
  EXPECT_FALSE(room.has_water_fill_zone());
  EXPECT_EQ(room.WaterFillTileCount(), 0);
  EXPECT_EQ(room.water_fill_sram_bit_mask(), 0);
  EXPECT_FALSE(room.water_fill_dirty());
}

TEST(RoomWaterFillStateTest, SetWaterFillTileUpdatesCountAndDirtyFlag) {
  Room room;
  room.ClearWaterFillDirty();

  room.SetWaterFillTile(10, 20, true);
  EXPECT_TRUE(room.has_water_fill_zone());
  EXPECT_TRUE(room.GetWaterFillTile(10, 20));
  EXPECT_EQ(room.WaterFillTileCount(), 1);
  EXPECT_TRUE(room.water_fill_dirty());

  room.ClearWaterFillDirty();
  // Writing the same value should be a no-op.
  room.SetWaterFillTile(10, 20, true);
  EXPECT_EQ(room.WaterFillTileCount(), 1);
  EXPECT_FALSE(room.water_fill_dirty());

  room.SetWaterFillTile(10, 20, false);
  EXPECT_FALSE(room.has_water_fill_zone());
  EXPECT_FALSE(room.GetWaterFillTile(10, 20));
  EXPECT_EQ(room.WaterFillTileCount(), 0);
  EXPECT_TRUE(room.water_fill_dirty());
}

TEST(RoomWaterFillStateTest, ClearWaterFillZoneResetsMaskAndTiles) {
  Room room;
  room.SetWaterFillTile(1, 1, true);
  room.set_water_fill_sram_bit_mask(0x04);
  room.ClearWaterFillDirty();

  room.ClearWaterFillZone();
  EXPECT_FALSE(room.has_water_fill_zone());
  EXPECT_FALSE(room.GetWaterFillTile(1, 1));
  EXPECT_EQ(room.WaterFillTileCount(), 0);
  EXPECT_EQ(room.water_fill_sram_bit_mask(), 0);
  EXPECT_TRUE(room.water_fill_dirty());
}

TEST(RoomWaterFillStateTest, SetWaterFillSramBitMaskMarksDirtyOnlyOnChange) {
  Room room;
  room.ClearWaterFillDirty();

  room.set_water_fill_sram_bit_mask(0x02);
  EXPECT_EQ(room.water_fill_sram_bit_mask(), 0x02);
  EXPECT_TRUE(room.water_fill_dirty());

  room.ClearWaterFillDirty();
  room.set_water_fill_sram_bit_mask(0x02);
  EXPECT_FALSE(room.water_fill_dirty());
}

}  // namespace
}  // namespace yaze::zelda3

