#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/dungeon/object_drawer.h"

namespace yaze {
namespace zelda3 {

class DrawRoutineMappingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Minimal ROM needed for ObjectDrawer construction
    rom_ = std::make_unique<Rom>();
    // No data needed for mapping logic as it's hardcoded in InitializeDrawRoutines
  }

  std::unique_ptr<Rom> rom_;
};

TEST_F(DrawRoutineMappingTest, VerifiesSubtype1Mappings) {
  ObjectDrawer drawer(rom_.get(), 0);
  
  // Test a few key mappings from bank_01.asm analysis
  
  // 0x00 -> Routine 0 (Rightwards2x2_1to15or32)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x00), 0);
  
  // 0x01-0x02 -> Routine 1 (Rightwards2x4_1to15or26)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x01), 1);
  EXPECT_EQ(drawer.GetDrawRoutineId(0x02), 1);
  
  // 0x09 -> Routine 5 (DiagonalAcute_1to16)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x09), 5);
  
  // 0x15 -> Routine 17 (DiagonalAcute_BothBG)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x15), 17);
  
  // 0x33 -> Routine 16 (4x4)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x33), 16);
}

TEST_F(DrawRoutineMappingTest, VerifiesNewlyAddedMappings) {
  ObjectDrawer drawer(rom_.get(), 0);
  
  // 0x50-0x5F range (previously missing)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x50), 25); // 1x1
  EXPECT_EQ(drawer.GetDrawRoutineId(0x51), 1);  // 2x4
}

TEST_F(DrawRoutineMappingTest, VerifiesSubtype2Mappings) {
  ObjectDrawer drawer(rom_.get(), 0);
  
  // 0x100-0x107 -> Routine 16 (4x4)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x100), 16);
  
  // 0x108 -> Routine 35 (4x4 Corner BothBG)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x108), 35);
  
  // 0x110 -> Routine 36 (Weird Corner Bottom)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x110), 36);
}

TEST_F(DrawRoutineMappingTest, VerifiesSubtype3Mappings) {
  ObjectDrawer drawer(rom_.get(), 0);
  
  // 0x200 -> Routine 34 (Water Face)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x200), 34);
  
  // 0x203 -> Routine 33 (Somaria Line)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x203), 33);
}

} // namespace zelda3
} // namespace yaze
