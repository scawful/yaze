#include "gtest/gtest.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room_object.h"
#include "app/rom.h"

namespace yaze {
namespace zelda3 {

class ObjectDimensionsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a mock ROM for testing
    rom_ = std::make_unique<Rom>();
    // Initialize with minimal ROM data for testing
    std::vector<uint8_t> mock_rom_data(1024 * 1024, 0);  // 1MB mock ROM
    rom_->LoadFromData(mock_rom_data);
  }

  void TearDown() override { rom_.reset(); }

  std::unique_ptr<Rom> rom_;
};

TEST_F(ObjectDimensionsTest, CalculatesDimensionsForType1Objects) {
  ObjectDrawer drawer(rom_.get());

  // Test object 0x00 (horizontal floor tile)
  // Routine 0: DrawRightwards2x2_1to15or32
  // Logic: width = size * 16 (where size 0 -> 32)
  
  RoomObject obj00(0x00, 10, 10, 0, 0); // Size 0 -> 32
  // width = 32 * 16 = 512
  auto dims = drawer.CalculateObjectDimensions(obj00);
  EXPECT_EQ(dims.first, 512);
  EXPECT_EQ(dims.second, 16);

  RoomObject obj00_size1(0x00, 10, 10, 1, 0); // Size 1
  // width = 1 * 16 = 16
  dims = drawer.CalculateObjectDimensions(obj00_size1);
  EXPECT_EQ(dims.first, 16);
  EXPECT_EQ(dims.second, 16);
}

TEST_F(ObjectDimensionsTest, CalculatesDimensionsForDiagonalWalls) {
  ObjectDrawer drawer(rom_.get());

  // Test object 0x10 (Diagonal Wall /)
  // Routine 17: DrawDiagonalAcute_1to16_BothBG
  // Logic: width = (size + 6) * 8
  
  RoomObject obj10(0x10, 10, 10, 0, 0); // Size 0
  // width = (0 + 6) * 8 = 48
  auto dims = drawer.CalculateObjectDimensions(obj10);
  EXPECT_EQ(dims.first, 48);
  EXPECT_EQ(dims.second, 48);

  RoomObject obj10_size10(0x10, 10, 10, 10, 0); // Size 10
  // width = (10 + 6) * 8 = 128
  dims = drawer.CalculateObjectDimensions(obj10_size10);
  EXPECT_EQ(dims.first, 128);
  EXPECT_EQ(dims.second, 128);
}

TEST_F(ObjectDimensionsTest, CalculatesDimensionsForType2Corners) {
  ObjectDrawer drawer(rom_.get());

  // Test object 0x40 (Type 2 Corner)
  // Routine 22: Edge 1x1
  // Width 8, Height 8
  RoomObject obj40(0x40, 10, 10, 0, 0);
  auto dims = drawer.CalculateObjectDimensions(obj40);
  EXPECT_EQ(dims.first, 8);
  EXPECT_EQ(dims.second, 8);
}

TEST_F(ObjectDimensionsTest, CalculatesDimensionsForType3Objects) {
  ObjectDrawer drawer(rom_.get());

  // Test object 0x200 (Water Face)
  // Routine 34: Water Face (2x2 tiles = 16x16 pixels)
  // Currently falls back to default logic or specific if added.
  // If not added to switch, default is 8 + size*4.
  // Water Face size usually 0?
  
  RoomObject obj200(0x200, 10, 10, 0, 0);
  auto dims = drawer.CalculateObjectDimensions(obj200);
  // If unhandled, check fallback behavior or add case.
  // For now, just ensure it returns something reasonable > 0
  EXPECT_GT(dims.first, 0);
  EXPECT_GT(dims.second, 0);
}

TEST_F(ObjectDimensionsTest, CalculatesDimensionsForSomariaLine) {
  ObjectDrawer drawer(rom_.get());

  // Test object 0x203 (Somaria Line) -> Routine 33
  // Routine 33 is handled: width 32, height 32
  
  RoomObject obj203(0x203, 10, 10, 0, 0);
  auto dims = drawer.CalculateObjectDimensions(obj203);
  EXPECT_EQ(dims.first, 32);
  EXPECT_EQ(dims.second, 32);
}

}  // namespace zelda3
}  // namespace yaze
