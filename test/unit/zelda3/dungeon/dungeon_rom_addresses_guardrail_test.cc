#include "zelda3/dungeon/dungeon_rom_addresses.h"

#include <cstddef>

#include "gtest/gtest.h"

namespace yaze::zelda3 {

TEST(DungeonRomAddressesGuardrailTest, WaterFillReservedRegionBoundary) {
  const auto below_end =
      static_cast<std::size_t>(kWaterFillTableEnd - 1);
  const auto at_end = static_cast<std::size_t>(kWaterFillTableEnd);

  EXPECT_FALSE(HasWaterFillReservedRegion(below_end));
  EXPECT_TRUE(HasWaterFillReservedRegion(at_end));
}

TEST(DungeonRomAddressesGuardrailTest, CustomCollisionPointerTableBoundary) {
  const int ptr_table_end =
      kCustomCollisionRoomPointers + (kNumberOfRooms * 3);
  const auto below_end = static_cast<std::size_t>(ptr_table_end - 1);
  const auto at_end = static_cast<std::size_t>(ptr_table_end);

  EXPECT_FALSE(HasCustomCollisionPointerTable(below_end));
  EXPECT_TRUE(HasCustomCollisionPointerTable(at_end));
}

TEST(DungeonRomAddressesGuardrailTest, CustomCollisionDataRegionBoundary) {
  const auto at_data_start =
      static_cast<std::size_t>(kCustomCollisionDataPosition);
  const auto below_soft_end =
      static_cast<std::size_t>(kCustomCollisionDataSoftEnd - 1);
  const auto at_soft_end =
      static_cast<std::size_t>(kCustomCollisionDataSoftEnd);

  EXPECT_FALSE(HasCustomCollisionDataRegion(at_data_start));
  EXPECT_FALSE(HasCustomCollisionDataRegion(below_soft_end));
  EXPECT_TRUE(HasCustomCollisionDataRegion(at_soft_end));
}

TEST(DungeonRomAddressesGuardrailTest, CustomCollisionWriteSupportRequiresBoth) {
  const int ptr_table_end =
      kCustomCollisionRoomPointers + (kNumberOfRooms * 3);
  const auto pointer_only_size = static_cast<std::size_t>(ptr_table_end);
  const auto data_only_size =
      static_cast<std::size_t>(kCustomCollisionDataSoftEnd);

  EXPECT_FALSE(HasCustomCollisionWriteSupport(pointer_only_size));
  EXPECT_TRUE(HasCustomCollisionWriteSupport(data_only_size));
}

}  // namespace yaze::zelda3

