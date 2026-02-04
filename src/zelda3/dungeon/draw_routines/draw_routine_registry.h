#ifndef YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_REGISTRY_H
#define YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_REGISTRY_H

#include <unordered_map>
#include <vector>

#include "zelda3/dungeon/draw_routines/draw_routine_types.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Canonical Draw Routine IDs
 *
 * These IDs are used by both ObjectDrawer (runtime rendering) and
 * ObjectGeometry (bounds calculation). Both systems MUST use these same IDs
 * to ensure selection bounds match rendered bounds.
 *
 * Based on bank_01.asm routine table at $018200.
 */
namespace DrawRoutineIds {

// Rightwards routines (0-4, 16, 20-32)
constexpr int kRightwards2x2_1to15or32 = 0;
constexpr int kRightwards2x4_1to15or26 = 1;
constexpr int kRightwards2x4_1to16 = 2;
constexpr int kRightwards2x4_1to16_BothBG = 3;
constexpr int kRightwards2x2_1to16 = 4;
constexpr int kRightwards4x4_1to16 = 16;
constexpr int kRightwards1x2_1to16_plus2 = 20;
constexpr int kRightwardsHasEdge1x1_1to16_plus3 = 21;
constexpr int kRightwardsHasEdge1x1_1to16_plus2 = 22;
constexpr int kRightwardsHasEdge1x1_1to16_plus23 = 118;
constexpr int kRightwardsTopCorners1x2_1to16_plus13 = 23;
constexpr int kRightwardsBottomCorners1x2_1to16_plus13 = 24;
constexpr int kRightwards1x1Solid_1to16_plus3 = 25;
constexpr int kRightwardsDecor4x4spaced2_1to16 = 27;
constexpr int kRightwardsStatue2x3spaced2_1to16 = 28;
constexpr int kRightwardsPillar2x4spaced4_1to16 = 29;
constexpr int kRightwardsDecor4x3spaced4_1to16 = 30;
constexpr int kRightwardsDoubled2x2spaced2_1to16 = 31;
constexpr int kRightwardsDecor2x2spaced12_1to16 = 32;

// Diagonal routines (5-6, 17-18)
constexpr int kDiagonalAcute_1to16 = 5;
constexpr int kDiagonalGrave_1to16 = 6;
constexpr int kDiagonalAcute_1to16_BothBG = 17;
constexpr int kDiagonalGrave_1to16_BothBG = 18;

// Downwards routines (7-15, 43-50, 65-71)
constexpr int kDownwards2x2_1to15or32 = 7;
constexpr int kDownwards4x2_1to15or26 = 8;
constexpr int kDownwards4x2_1to16_BothBG = 9;
constexpr int kDownwardsDecor4x2spaced4_1to16 = 10;
constexpr int kDownwards2x2_1to16 = 11;
constexpr int kDownwardsHasEdge1x1_1to16_plus3 = 12;
constexpr int kDownwardsEdge1x1_1to16 = 13;
constexpr int kDownwardsLeftCorners2x1_1to16_plus12 = 14;
constexpr int kDownwardsRightCorners2x1_1to16_plus12 = 15;
constexpr int kDownwardsFloor4x4_1to16 = 43;
constexpr int kDownwards1x1Solid_1to16_plus3 = 44;
constexpr int kDownwardsDecor4x4spaced2_1to16 = 45;
constexpr int kDownwardsPillar2x4spaced2_1to16 = 46;
constexpr int kDownwardsDecor3x4spaced4_1to16 = 47;
constexpr int kDownwardsDecor2x2spaced12_1to16 = 48;
constexpr int kDownwardsLine1x1_1to16plus1 = 49;
constexpr int kDownwardsDecor2x4spaced8_1to16 = 50;
constexpr int kDownwardsDecor3x4spaced2_1to16 = 65;
constexpr int kDownwardsBigRail3x1_1to16plus5 = 66;
constexpr int kDownwardsBlock2x2spaced2_1to16 = 67;
constexpr int kDownwardsCannonHole3x6_1to16 = 68;
constexpr int kDownwardsBar2x3_1to16 = 69;
constexpr int kDownwardsPots2x2_1to16 = 70;
constexpr int kDownwardsHammerPegs2x2_1to16 = 71;

// Vertical rail routine with CORNER+MIDDLE+END pattern (for 0x8A-0x8C)
constexpr int kDownwardsHasEdge1x1_1to16_plus23 = 117;

// Corner routines (19, 35-37, 75-78)
constexpr int kCorner4x4 = 19;
constexpr int kCorner4x4_BothBG = 35;
constexpr int kWeirdCornerBottom_BothBG = 36;
constexpr int kWeirdCornerTop_BothBG = 37;
constexpr int kDiagonalCeilingTopLeft = 75;
constexpr int kDiagonalCeilingBottomLeft = 76;
constexpr int kDiagonalCeilingTopRight = 77;
constexpr int kDiagonalCeilingBottomRight = 78;

// Special routines (26, 33-34, 38-39)
constexpr int kDoorSwitcherer = 26;
constexpr int kSomariaLine = 33;
constexpr int kWaterFace = 34;
constexpr int kNothing = 38;
constexpr int kChest = 39;

// Additional rightwards (51-55, 72-74)
constexpr int kRightwardsLine1x1_1to16plus1 = 51;
constexpr int kRightwardsBar4x3_1to16 = 52;
constexpr int kRightwardsShelf4x4_1to16 = 53;
constexpr int kRightwardsBigRail1x3_1to16plus5 = 54;
constexpr int kRightwardsBlock2x2spaced2_1to16 = 55;
constexpr int kRightwardsEdge1x1_1to16plus7 = 72;
constexpr int kRightwardsPots2x2_1to16 = 73;
constexpr int kRightwardsHammerPegs2x2_1to16 = 74;

// SuperSquare routines (56-64)
constexpr int k4x4BlocksIn4x4SuperSquare = 56;
constexpr int k3x3FloorIn4x4SuperSquare = 57;
constexpr int k4x4FloorIn4x4SuperSquare = 58;
constexpr int k4x4FloorOneIn4x4SuperSquare = 59;
constexpr int k4x4FloorTwoIn4x4SuperSquare = 60;
constexpr int kBigHole4x4_1to16 = 61;
constexpr int kSpike2x2In4x4SuperSquare = 62;
constexpr int kTableRock4x4_1to16 = 63;
constexpr int kWaterOverlay8x8_1to16 = 64;

// Chest platform and moving wall routines (79-82)
constexpr int kClosedChestPlatform = 79;
constexpr int kMovingWallWest = 80;
constexpr int kMovingWallEast = 81;
constexpr int kOpenChestPlatform = 82;

// Stair routines (83-91)
constexpr int kInterRoomFatStairsUp = 83;
constexpr int kInterRoomFatStairsDownA = 84;
constexpr int kInterRoomFatStairsDownB = 85;
constexpr int kAutoStairs = 86;
constexpr int kStraightInterRoomStairs = 87;
constexpr int kSpiralStairsGoingUpUpper = 88;
constexpr int kSpiralStairsGoingDownUpper = 89;
constexpr int kSpiralStairsGoingUpLower = 90;
constexpr int kSpiralStairsGoingDownLower = 91;

// Interactive object routines (92-96)
constexpr int kBigKeyLock = 92;
constexpr int kBombableFloor = 93;
constexpr int kEmptyWaterFace = 94;
constexpr int kSpittingWaterFace = 95;
constexpr int kDrenchingWaterFace = 96;

// Additional Type 2/3 routines (97-115)
constexpr int kWaterfall = 97;
constexpr int kFloorTile4x2 = 98;
constexpr int kCannonHole4x3 = 99;
constexpr int kBed4x5 = 100;
constexpr int kRightwards3x6 = 101;
constexpr int kVerticalTurtleRockPipe = 102;
constexpr int kHorizontalTurtleRockPipe = 103;
constexpr int kLightBeam = 104;
constexpr int kBigLightBeam = 105;
constexpr int kBossShell4x4 = 106;
constexpr int kSolidWallDecor3x4 = 107;
constexpr int kArcheryGameTargetDoor = 108;
constexpr int kGanonTriforceFloorDecor = 109;
constexpr int kLargeCanvasObject = 110;
constexpr int kUtility6x3 = 111;
constexpr int kUtility3x5 = 112;
constexpr int kSingle4x4 = 113;
constexpr int kSingle4x3 = 114;
constexpr int kRupeeFloor = 115;

// Custom object routines (130+)
constexpr int kCustomObject = 130;

}  // namespace DrawRoutineIds

/**
 * @brief Unified draw routine registry
 *
 * Singleton that provides draw routine metadata for both ObjectDrawer
 * and ObjectGeometry. This ensures both systems use the same routine
 * IDs and dimension calculations.
 */
class DrawRoutineRegistry {
 public:
  static DrawRoutineRegistry& Get();

  // Initialize the registry - must be called once at startup
  void Initialize();

  // Look up routine info by ID
  const DrawRoutineInfo* GetRoutineInfo(int routine_id) const;

  // Get all registered routines
  const std::vector<DrawRoutineInfo>& GetAllRoutines() const { return routines_; }

  // Check if a routine draws to both BG layers
  bool RoutineDrawsToBothBGs(int routine_id) const;

  // Get routine metadata for dimension calculation
  bool GetRoutineDimensions(int routine_id, int* base_width, int* base_height) const;

 private:
  DrawRoutineRegistry() = default;
  void BuildRegistry();

  std::vector<DrawRoutineInfo> routines_;
  std::unordered_map<int, const DrawRoutineInfo*> routine_map_;
  bool initialized_ = false;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_REGISTRY_H
