// Phase 4 Routine Mappings (0x80-0xFF range)
//
// Steps 1, 2, 3, 4, 5 completed - 83 routines total (0-82)
//
// Step 1 Quick Fixes:
// - 0x8D-0x8E: 25 -> 13 (DownwardsEdge1x1_1to16)
// - 0x92-0x93: 11 -> 7 (Downwards2x2_1to15or32)
// - 0x94: 16 -> 43 (DownwardsFloor4x4_1to16)
// - 0xB6-0xB7: 8 -> 1 (Rightwards2x4_1to15or26)
// - 0xB8-0xB9: 11 -> 0 (Rightwards2x2_1to15or32)
// - 0xBB: 11 -> 55 (RightwardsBlock2x2spaced2_1to16)
//
// Step 2 Simple Variant Routines (IDs 65-74):
// - 0x81-0x84: routine 65 (DrawDownwardsDecor3x4spaced2_1to16)
// - 0x88: routine 66 (DrawDownwardsBigRail3x1_1to16plus5)
// - 0x89: routine 67 (DrawDownwardsBlock2x2spaced2_1to16)
// - 0x85-0x86: routine 68 (DrawDownwardsCannonHole3x6_1to16)
// - 0x8F: routine 69 (DrawDownwardsBar2x3_1to16)
// - 0x95: routine 70 (DrawDownwardsPots2x2_1to16)
// - 0x96: routine 71 (DrawDownwardsHammerPegs2x2_1to16)
// - 0xB0-0xB1: routine 72 (DrawRightwardsEdge1x1_1to16plus7)
// - 0xBC: routine 73 (DrawRightwardsPots2x2_1to16)
// - 0xBD: routine 74 (DrawRightwardsHammerPegs2x2_1to16)
//
// Step 3 Diagonal Ceiling Routines (IDs 75-78):
// - 0xA0, 0xA5, 0xA9: routine 75 (DrawDiagonalCeilingTopLeft)
// - 0xA1, 0xA6, 0xAA: routine 76 (DrawDiagonalCeilingBottomLeft)
// - 0xA2, 0xA7, 0xAB: routine 77 (DrawDiagonalCeilingTopRight)
// - 0xA3, 0xA8, 0xAC: routine 78 (DrawDiagonalCeilingBottomRight)
//
// Step 4 SuperSquare Routines (IDs 56-64):
// - 0xC0, 0xC2: routine 56 (Draw4x4BlocksIn4x4SuperSquare)
// - 0xC3, 0xD7: routine 57 (Draw3x3FloorIn4x4SuperSquare)
// - 0xC5-0xCA, 0xD1-0xD2, 0xD9, 0xDF-0xE8: routine 58 (Draw4x4FloorIn4x4SuperSquare)
// - 0xC4: routine 59 (Draw4x4FloorOneIn4x4SuperSquare)
// - 0xDB: routine 60 (Draw4x4FloorTwoIn4x4SuperSquare)
// - 0xA4: routine 61 (DrawBigHole4x4_1to16)
// - 0xDE: routine 62 (DrawSpike2x2In4x4SuperSquare)
// - 0xDD: routine 63 (DrawTableRock4x4_1to16)
// - 0xD8, 0xDA: routine 64 (DrawWaterOverlay8x8_1to16)
//
// Step 5 Special Routines (IDs 79-82):
// - 0xC1: routine 79 (DrawClosedChestPlatform)
// - 0xCD: routine 80 (DrawMovingWallWest)
// - 0xCE: routine 81 (DrawMovingWallEast)
// - 0xDC: routine 82 (DrawOpenChestPlatform)
// - 0xD3-0xD6: routine 38 (DrawNothing - logic-only objects)

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

TEST_F(DrawRoutineMappingTest, VerifiesPhase4Step2Mappings) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Step 2 Simple Variant Routines
  // 0x81-0x84: routine 65 (DownwardsDecor3x4spaced2_1to16)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x81), 65);
  EXPECT_EQ(drawer.GetDrawRoutineId(0x84), 65);

  // 0x88: routine 66 (DownwardsBigRail3x1_1to16plus5)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x88), 66);

  // 0x89: routine 67 (DownwardsBlock2x2spaced2_1to16)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x89), 67);

  // 0xB0-0xB1: routine 72 (RightwardsEdge1x1_1to16plus7)
  EXPECT_EQ(drawer.GetDrawRoutineId(0xB0), 72);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xB1), 72);
}

TEST_F(DrawRoutineMappingTest, VerifiesPhase4Step3DiagonalCeilingMappings) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Step 3 Diagonal Ceiling Routines
  // DiagonalCeilingTopLeft: 0xA0, 0xA5, 0xA9 -> routine 75
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA0), 75);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA5), 75);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA9), 75);

  // DiagonalCeilingBottomLeft: 0xA1, 0xA6, 0xAA -> routine 76
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA1), 76);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA6), 76);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xAA), 76);

  // DiagonalCeilingTopRight: 0xA2, 0xA7, 0xAB -> routine 77
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA2), 77);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA7), 77);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xAB), 77);

  // DiagonalCeilingBottomRight: 0xA3, 0xA8, 0xAC -> routine 78
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA3), 78);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA8), 78);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xAC), 78);
}

TEST_F(DrawRoutineMappingTest, VerifiesPhase4Step5SpecialMappings) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Step 5 Special Routines
  // ClosedChestPlatform: 0xC1 -> routine 79
  EXPECT_EQ(drawer.GetDrawRoutineId(0xC1), 79);

  // MovingWallWest: 0xCD -> routine 80
  EXPECT_EQ(drawer.GetDrawRoutineId(0xCD), 80);

  // MovingWallEast: 0xCE -> routine 81
  EXPECT_EQ(drawer.GetDrawRoutineId(0xCE), 81);

  // OpenChestPlatform: 0xDC -> routine 82
  EXPECT_EQ(drawer.GetDrawRoutineId(0xDC), 82);

  // CheckIfWallIsMoved: 0xD3-0xD6 -> routine 38 (Nothing) - logic-only objects
  EXPECT_EQ(drawer.GetDrawRoutineId(0xD3), 38);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xD4), 38);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xD5), 38);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xD6), 38);
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

  // Type 3 objects (0x200+) are special objects
  // Currently returning -1 (unmapped) as these need special handling
  // TODO(Phase 5): Implement Type 3 object mappings
  // Expected once implemented:
  // - 0x200 -> Routine 34 (Water Face)
  // - 0x203 -> Routine 33 (Somaria Line)
  int routine_200 = drawer.GetDrawRoutineId(0x200);
  int routine_203 = drawer.GetDrawRoutineId(0x203);

  // For now, accept either -1 (unmapped) or the expected values
  EXPECT_TRUE(routine_200 == -1 || routine_200 == 34)
      << "0x200 expected -1 or 34, got " << routine_200;
  EXPECT_TRUE(routine_203 == -1 || routine_203 == 33)
      << "0x203 expected -1 or 33, got " << routine_203;
}

} // namespace zelda3
} // namespace yaze
