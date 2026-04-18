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
// - 0x85-0x86: routine 68 (DrawDownwardsCannonHole3x4_1to16)
// - 0x8F: routine 69 (DrawDownwardsBar2x5_1to16)
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

#include <algorithm>
#include <vector>

#include "app/gfx/render/background_buffer.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room_object.h"

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

namespace {

struct TilePoint {
  int x;
  int y;
};

gfx::TileInfo MakeTile(uint16_t id, uint8_t palette = 0) {
  return gfx::TileInfo(id, palette, false, false, false);
}

std::vector<TilePoint> CollectNonZeroTiles(const gfx::BackgroundBuffer& bg) {
  std::vector<TilePoint> points;
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      if (bg.GetTileAt(x, y) != 0) {
        points.push_back({x, y});
      }
    }
  }
  return points;
}

bool ContainsPoint(const std::vector<TilePoint>& points, int x, int y) {
  for (const auto& point : points) {
    if (point.x == x && point.y == y) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_F(DrawRoutineMappingTest,
       HorizontalRailRoutinesKeepExistingSmallCornerTile) {
  auto& reg = DrawRoutineRegistry::Get();
  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x00E2, 0), MakeTile(0x0240, 1), MakeTile(0x0241, 2)};

  struct Case {
    int routine_id;
    RoomObject object;
    int end_x;
  };

  const std::vector<Case> cases = {
      {DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus3,
       RoomObject(0x22, 5, 7, 0, 0),
       8},
      {DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus23,
       RoomObject(0x5F, 5, 7, 0, 0),
       27},
  };

  for (const auto& tc : cases) {
    SCOPED_TRACE(::testing::Message() << "routine=" << tc.routine_id);

    const DrawRoutineInfo* info = reg.GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);

    gfx::BackgroundBuffer bg;
    const uint16_t preexisting_corner =
        gfx::TileInfoToWord(MakeTile(0x00E2, 6));
    bg.SetTileAt(tc.object.x_, tc.object.y_, preexisting_corner);

    DrawContext ctx{bg,
                    tc.object,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    EXPECT_EQ(bg.GetTileAt(tc.object.x_, tc.object.y_), preexisting_corner);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.object.x_ + 1, tc.object.y_),
              tiles[1].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.end_x, tc.object.y_),
              tiles[2].id_);
  }
}

TEST_F(DrawRoutineMappingTest,
       HorizontalTrimRailRoutineKeepsCompatibleExistingCornerTile) {
  auto& reg = DrawRoutineRegistry::Get();
  const DrawRoutineInfo* info =
      reg.GetRoutineInfo(DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus2);
  ASSERT_NE(info, nullptr);

  gfx::BackgroundBuffer bg;
  const RoomObject object(0x23, 9, 11, 0, 0);
  const uint16_t compatible_corner = gfx::TileInfoToWord(MakeTile(0x01A6, 5));
  bg.SetTileAt(object.x_, object.y_, compatible_corner);

  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x01DB, 0), MakeTile(0x0260, 1), MakeTile(0x0261, 2)};
  DrawContext ctx{bg,
                  object,
                  std::span<const gfx::TileInfo>(tiles),
                  /*state=*/nullptr,
                  rom_.get(),
                  /*room_id=*/0,
                  /*room_gfx_buffer=*/nullptr,
                  /*secondary_bg=*/nullptr};
  info->function(ctx);

  EXPECT_EQ(bg.GetTileAt(object.x_, object.y_), compatible_corner);
  EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, object.x_ + 1, object.y_),
            tiles[1].id_);
  EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, object.x_ + 2, object.y_),
            tiles[2].id_);
}

TEST_F(DrawRoutineMappingTest,
       VerticalRailRoutinesKeepExistingSmallCornerTile) {
  auto& reg = DrawRoutineRegistry::Get();
  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x00E3, 0), MakeTile(0x0280, 1), MakeTile(0x0281, 2)};

  struct Case {
    int routine_id;
    RoomObject object;
    int end_y;
  };

  const std::vector<Case> cases = {
      {DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus3,
       RoomObject(0x69, 13, 3, 0, 0),
       5},
      {DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23,
       RoomObject(0x8A, 13, 3, 0, 0),
       25},
  };

  for (const auto& tc : cases) {
    SCOPED_TRACE(::testing::Message() << "routine=" << tc.routine_id);

    const DrawRoutineInfo* info = reg.GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);

    gfx::BackgroundBuffer bg;
    const uint16_t preexisting_corner =
        gfx::TileInfoToWord(MakeTile(0x00E3, 7));
    bg.SetTileAt(tc.object.x_, tc.object.y_, preexisting_corner);

    DrawContext ctx{bg,
                    tc.object,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    EXPECT_EQ(bg.GetTileAt(tc.object.x_, tc.object.y_), preexisting_corner);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.object.x_, tc.object.y_ + 1),
              tiles[1].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.object.x_, tc.end_y),
              tiles[2].id_);
  }
}

TEST_F(DrawRoutineMappingTest,
       DownwardsCornerVariantsSkipOpeningCapWhenCornerAlreadyExists) {
  auto& reg = DrawRoutineRegistry::Get();
  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x0300, 0), MakeTile(0x0301, 1), MakeTile(0x0302, 2),
      MakeTile(0x0303, 3), MakeTile(0x0304, 4), MakeTile(0x0305, 5)};

  struct Case {
    int routine_id;
    RoomObject object;
    int corner_x;
    int corner_y;
    int body_x;
    int body_y;
    int fill_x;
    int fill_y;
    int end_top_x;
    int end_bottom_x;
  };

  const std::vector<Case> cases = {
      {DrawRoutineIds::kDownwardsLeftCorners2x1_1to16_plus12,
       RoomObject(0x6C, 6, 8, 0, 0),
       18,
       8,
       18,
       8,
       19,
       8,
       18,
       18},
      {DrawRoutineIds::kDownwardsRightCorners2x1_1to16_plus12,
       RoomObject(0x6D, 6, 8, 0, 0),
       19,
       8,
       19,
       8,
       18,
       8,
       19,
       19},
  };

  for (const auto& tc : cases) {
    SCOPED_TRACE(::testing::Message() << "routine=" << tc.routine_id);

    const DrawRoutineInfo* info = reg.GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);

    gfx::BackgroundBuffer bg;
    const uint16_t preexisting_corner =
        gfx::TileInfoToWord(MakeTile(0x00E3, 7));
    bg.SetTileAt(tc.corner_x, tc.corner_y, preexisting_corner);

    DrawContext ctx{bg,
                    tc.object,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.body_x, tc.body_y), tiles[3].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.fill_x, tc.fill_y), tiles[0].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.end_top_x, tc.object.y_ + 10),
              tiles[4].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.end_bottom_x, tc.object.y_ + 11),
              tiles[5].id_);
  }
}

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

TEST_F(DrawRoutineMappingTest,
       DiagonalCeilingRoutinesRenderExpectedOrientationAndArea) {
  auto& reg = DrawRoutineRegistry::Get();

  gfx::TileInfo fill_tile;
  fill_tile.id_ = 1;
  fill_tile.palette_ = 0;
  std::vector<gfx::TileInfo> tiles = {fill_tile};

  struct Case {
    int routine_id;
    RoomObject object;
    int expected_min_x;
    int expected_max_x;
    int expected_min_y;
    int expected_max_y;
    TilePoint anchor;
    TilePoint should_exist;
    TilePoint should_not_exist;
  };

  const std::vector<Case> cases = {
      {75,
       RoomObject(0xA0, 10, 10, 0, 0),
       10,
       13,
       10,
       13,
       {10, 10},
       {13, 10},
       {13, 13}},
      {76,
       RoomObject(0xA1, 10, 10, 0, 0),
       10,
       13,
       7,
       10,
       {10, 10},
       {10, 7},
       {13, 7}},
      {77,
       RoomObject(0xA2, 10, 10, 0, 0),
       7,
       10,
       10,
       13,
       {10, 10},
       {7, 10},
       {7, 13}},
      {78,
       RoomObject(0xA3, 10, 10, 0, 0),
       7,
       10,
       7,
       10,
       {10, 10},
       {10, 7},
       {7, 7}},
  };

  for (const auto& tc : cases) {
    SCOPED_TRACE(::testing::Message() << "routine=" << tc.routine_id);

    const DrawRoutineInfo* info = reg.GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);

    gfx::BackgroundBuffer bg;
    DrawContext ctx{bg,
                    tc.object,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    const auto points = CollectNonZeroTiles(bg);
    ASSERT_FALSE(points.empty());

    // size nibble=0 => side=(0+4), triangle area = 4+3+2+1 = 10 tiles.
    EXPECT_EQ(points.size(), 10u);

    int min_x = points.front().x;
    int max_x = points.front().x;
    int min_y = points.front().y;
    int max_y = points.front().y;
    for (const auto& point : points) {
      min_x = std::min(min_x, point.x);
      max_x = std::max(max_x, point.x);
      min_y = std::min(min_y, point.y);
      max_y = std::max(max_y, point.y);
    }

    EXPECT_EQ(min_x, tc.expected_min_x);
    EXPECT_EQ(max_x, tc.expected_max_x);
    EXPECT_EQ(min_y, tc.expected_min_y);
    EXPECT_EQ(max_y, tc.expected_max_y);

    EXPECT_TRUE(ContainsPoint(points, tc.anchor.x, tc.anchor.y));
    EXPECT_TRUE(ContainsPoint(points, tc.should_exist.x, tc.should_exist.y));
    EXPECT_FALSE(
        ContainsPoint(points, tc.should_not_exist.x, tc.should_not_exist.y));
  }
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

  // 0x100-0x107 -> Routine 16 (RoomDraw_4x4)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x100), 16);

  // 0x108 -> Routine 35 (4x4 Corner BothBG)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x108), 35);

  // 0x110 -> Routine 36 (Weird Corner Bottom)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x110), 36);

  // Type-2 specials
  EXPECT_EQ(drawer.GetDrawRoutineId(0x122), DrawRoutineIds::kBed4x5);
  EXPECT_EQ(drawer.GetDrawRoutineId(0x12C), DrawRoutineIds::kRightwards3x6);
  EXPECT_EQ(drawer.GetDrawRoutineId(0x13E), DrawRoutineIds::kUtility6x3);
}

TEST_F(DrawRoutineMappingTest, VerifiesSubtype3Mappings) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Type-3 key mappings from usdasm routine table.
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF80), DrawRoutineIds::kEmptyWaterFace);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF83), DrawRoutineIds::kSomariaLine);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF8D), DrawRoutineIds::kPrisonCell);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF90), DrawRoutineIds::kSingle2x2);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF92), DrawRoutineIds::kRupeeFloor);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFB1), DrawRoutineIds::kSingle4x3);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFD5), DrawRoutineIds::kUtility3x5);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFBA),
            DrawRoutineIds::kVerticalTurtleRockPipe);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFBC),
            DrawRoutineIds::kHorizontalTurtleRockPipe);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFE0),
            DrawRoutineIds::kArcheryGameTargetDoor);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFE6), DrawRoutineIds::kActual4x4);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFE9), DrawRoutineIds::kSolidWallDecor3x4);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFEB), DrawRoutineIds::kSingle4x4);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFF0), DrawRoutineIds::kLightBeam);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFF1), DrawRoutineIds::kBigLightBeam);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFF2), DrawRoutineIds::kBossShell4x4);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFF8),
            DrawRoutineIds::kGanonTriforceFloorDecor);
}

}  // namespace zelda3
}  // namespace yaze
