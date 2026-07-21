#include "zelda3/dungeon/geometry/object_geometry.h"

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "zelda3/dungeon/dungeon_state.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {

namespace {

class ActiveWaterFaceState final : public DungeonState {
 public:
  bool IsChestOpen(int, int) const override { return false; }
  bool IsBigChestOpen() const override { return false; }
  bool IsDoorOpen(int, int) const override { return false; }
  bool IsDoorSwitchActive(int) const override { return false; }
  bool IsWaterFaceActive(int) const override { return true; }
  bool IsWallMoved(int) const override { return false; }
  bool IsFloorBombable(int) const override { return false; }
  bool IsRupeeFloorCleared(int) const override { return false; }
  bool IsCrystalSwitchBlue() const override { return true; }
};

}  // namespace

TEST(ObjectGeometryTest, Rightwards2x2UsesThirtyTwoWhenSizeZero) {
  RoomObject obj(/*id=*/0x00, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByRoutineId(/*routine_id=*/0, obj);
  ASSERT_TRUE(bounds.ok());
  EXPECT_EQ(bounds->min_x_tiles, 0);
  EXPECT_EQ(bounds->min_y_tiles, 0);
  EXPECT_EQ(bounds->width_tiles, 64);  // 32 repeats × 2 tiles wide
  EXPECT_EQ(bounds->height_tiles, 2);  // 2 tiles tall
  EXPECT_EQ(bounds->width_pixels(), 512);
  EXPECT_EQ(bounds->height_pixels(), 16);
}

TEST(ObjectGeometryTest, Downwards2x2UsesThirtyTwoWhenSizeZero) {
  RoomObject obj(/*id=*/0x60, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByRoutineId(/*routine_id=*/7, obj);
  ASSERT_TRUE(bounds.ok());
  EXPECT_EQ(bounds->min_x_tiles, 0);
  EXPECT_EQ(bounds->min_y_tiles, 0);
  EXPECT_EQ(bounds->width_tiles, 2);    // 2 tiles wide
  EXPECT_EQ(bounds->height_tiles, 64);  // 32 repeats × 2 tiles tall
}

TEST(ObjectGeometryTest, DiagonalAcuteExtendsUpward) {
  RoomObject obj(/*id=*/0x09, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByRoutineId(/*routine_id=*/5, obj);
  ASSERT_TRUE(bounds.ok());
  EXPECT_EQ(bounds->width_tiles, 7);    // count = size + 7
  EXPECT_EQ(bounds->height_tiles, 11);  // count + 4 rows
  EXPECT_EQ(bounds->min_x_tiles, 0);
  EXPECT_EQ(bounds->min_y_tiles, -6);  // routine walks upward from origin
}

TEST(ObjectGeometryTest, DiagonalCeilingBoundsMatchRenderForAllAnchors) {
  // USDASM: RoomDraw_DiagonalCeiling*A ($01:8BE0+) uses GetSize_1to16_timesA
  // with A=4, so the rendered square's side = (size_nibble & 0x0F) + 4.
  // All four anchors (TopLeft=75, BottomLeft=76, TopRight=77, BottomRight=78)
  // must report matching side x side render bounds regardless of anchor.
  struct Case {
    int routine_id;
    const char* name;
  };
  const Case anchors[] = {
      {75, "TopLeft"},
      {76, "BottomLeft"},
      {77, "TopRight"},
      {78, "BottomRight"},
  };

  const uint8_t sizes[] = {0x00, 0x01, 0x03, 0x07, 0x0F};
  for (const auto& anchor : anchors) {
    for (uint8_t size : sizes) {
      SCOPED_TRACE(absl::StrFormat("%s size=0x%02X", anchor.name, size));
      const int expected_side = (size & 0x0F) + 4;
      RoomObject obj(/*id=*/0xA0, /*x=*/0, /*y=*/0, size);
      auto bounds =
          ObjectGeometry::Get().MeasureByRoutineId(anchor.routine_id, obj);
      ASSERT_TRUE(bounds.ok()) << bounds.status();
      EXPECT_EQ(bounds->width_tiles, expected_side);
      EXPECT_EQ(bounds->height_tiles, expected_side);
    }
  }
}

TEST(ObjectGeometryTest, SomariaPathPiecesAreSingleTileObjects) {
  constexpr int16_t kObjectIds[] = {0xF83, 0xF84, 0xF85, 0xF86, 0xF87, 0xF88,
                                    0xF89, 0xF8A, 0xF8B, 0xF8C, 0xF8E, 0xF8F};

  for (int16_t object_id : kObjectIds) {
    SCOPED_TRACE(absl::StrFormat("object_id=0x%04X", object_id));
    RoomObject obj(object_id, /*x=*/0, /*y=*/0, /*size=*/0x0F);
    auto bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
    ASSERT_TRUE(bounds.ok());
    EXPECT_EQ(bounds->min_x_tiles, 0);
    EXPECT_EQ(bounds->min_y_tiles, 0);
    EXPECT_EQ(bounds->width_tiles, 1);
    EXPECT_EQ(bounds->height_tiles, 1);
  }
}

TEST(ObjectGeometryTest, WeirdCornerBottomBothBGUsesUsdasm3x4Shape) {
  // USDASM: RoomDraw_WeirdCornerBottom_BothBG ($01:9854) -> 3 columns x 4 rows.
  RoomObject obj(/*id=*/0x110, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds =
      ObjectGeometry::Get().MeasureByRoutineId(/*routine_id=*/36, obj);
  ASSERT_TRUE(bounds.ok());
  EXPECT_EQ(bounds->width_tiles, 3);
  EXPECT_EQ(bounds->height_tiles, 4);
}

TEST(ObjectGeometryTest, WeirdCornerTopBothBGUsesUsdasm4x3Shape) {
  // USDASM: RoomDraw_WeirdCornerTop_BothBG ($01:985C) -> 4 columns x 3 rows.
  RoomObject obj(/*id=*/0x114, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds =
      ObjectGeometry::Get().MeasureByRoutineId(/*routine_id=*/37, obj);
  ASSERT_TRUE(bounds.ok());
  EXPECT_EQ(bounds->width_tiles, 4);
  EXPECT_EQ(bounds->height_tiles, 3);
}

TEST(ObjectGeometryTest, MeasureByObjectIdKnownObject) {
  // Object 0x00 -> routine 0 (Rightwards2x2_1to15or32)
  RoomObject obj(/*id=*/0x00, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
  ASSERT_TRUE(bounds.ok());
  EXPECT_EQ(bounds->width_tiles, 64);  // 32 repeats × 2 tiles wide
  EXPECT_EQ(bounds->height_tiles, 2);
}

TEST(ObjectGeometryTest, MeasureByObjectIdSubtype3SpecialsHaveUsdasmBounds) {
  struct Case {
    int16_t object_id;
    uint8_t size;
    int width_tiles;
    int height_tiles;
  };

  const Case cases[] = {
      {0x0F90, 0x00, 2, 2},  // Single2x2
      {0x0F92, 0x00, 5, 8},  // RupeeFloor
      {0x0FB1, 0x00, 4, 3},  // Single4x3
      {0x0FC7, 0x00, 4, 4},  // BombableFloor
      {0x0FE6, 0x00, 4, 4},  // Actual4x4
      {0x0FEB, 0x00, 4, 4},  // Single4x4
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.object_id);
    RoomObject obj(test_case.object_id, /*x=*/0, /*y=*/0, test_case.size);
    auto bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
    ASSERT_TRUE(bounds.ok());
    EXPECT_EQ(bounds->width_tiles, test_case.width_tiles);
    EXPECT_EQ(bounds->height_tiles, test_case.height_tiles);
  }
}

TEST(ObjectGeometryTest,
     MeasureByObjectIdEmptyWaterFaceUsesMaxStatefulFootprint) {
  RoomObject obj(/*id=*/0x0F80, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
  ASSERT_TRUE(bounds.ok());
  EXPECT_EQ(bounds->width_tiles, 4);
  EXPECT_EQ(bounds->height_tiles, 5);
}

TEST(ObjectGeometryTest,
     MeasureByObjectIdForStateKeepsWaterFaceBranchesDistinct) {
  RoomObject obj(/*id=*/0x0F80, /*x=*/0, /*y=*/0, /*size=*/0);

  auto default_bounds =
      ObjectGeometry::Get().MeasureByObjectIdForState(obj, nullptr);
  ASSERT_TRUE(default_bounds.ok());
  EXPECT_EQ(default_bounds->width_tiles, 4);
  EXPECT_EQ(default_bounds->height_tiles, 3);

  ActiveWaterFaceState active_state;
  auto active_bounds =
      ObjectGeometry::Get().MeasureByObjectIdForState(obj, &active_state);
  ASSERT_TRUE(active_bounds.ok());
  EXPECT_EQ(active_bounds->width_tiles, 4);
  EXPECT_EQ(active_bounds->height_tiles, 5);
}

TEST(ObjectGeometryTest, SmallChestGeometryUsesCanonicalTwoByTwoPayload) {
  ObjectGeometry::Get().ClearCache();

  for (const int16_t object_id : {int16_t{0x0F99}, int16_t{0x0F9A}}) {
    SCOPED_TRACE(absl::StrFormat("object_id=0x%03X", object_id));
    RoomObject obj(object_id, /*x=*/0, /*y=*/0, /*size=*/0);

    auto editor_bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
    ASSERT_TRUE(editor_bounds.ok());
    EXPECT_EQ(editor_bounds->width_tiles, 2);
    EXPECT_EQ(editor_bounds->height_tiles, 2);

    auto default_state_bounds =
        ObjectGeometry::Get().MeasureByObjectIdForState(obj, nullptr);
    ASSERT_TRUE(default_state_bounds.ok());
    EXPECT_EQ(default_state_bounds->width_tiles, 2);
    EXPECT_EQ(default_state_bounds->height_tiles, 2);
  }
}

TEST(ObjectGeometryTest, Subtype1ChestGeometryKeepsFourByFourPayload) {
  ObjectGeometry::Get().ClearCache();

  struct ChestCase {
    int16_t object_id;
    uint8_t size;
  };
  for (const auto& test_case : {
           ChestCase{0x00F9, 0x00},
           ChestCase{0x00FD, 0x0F},
       }) {
    SCOPED_TRACE(absl::StrFormat("object_id=0x%03X", test_case.object_id));
    RoomObject obj(test_case.object_id, /*x=*/0, /*y=*/0, test_case.size);

    auto editor_bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
    ASSERT_TRUE(editor_bounds.ok());
    EXPECT_EQ(editor_bounds->width_tiles, 4);
    EXPECT_EQ(editor_bounds->height_tiles, 4);

    auto default_state_bounds =
        ObjectGeometry::Get().MeasureByObjectIdForState(obj, nullptr);
    ASSERT_TRUE(default_state_bounds.ok());
    EXPECT_EQ(default_state_bounds->width_tiles, 4);
    EXPECT_EQ(default_state_bounds->height_tiles, 4);
  }
}

TEST(ObjectGeometryTest, MeasureByObjectIdMigratedSpecialsHaveUsdasmBounds) {
  struct Case {
    int16_t object_id;
    uint8_t size;
    int width_tiles;
    int height_tiles;
  };

  const Case cases[] = {
      {0x0122, 0x00, 4, 5},   // Bed4x5
      {0x012C, 0x00, 6, 3},   // Rightwards3x6
      {0x0135, 0x00, 4, 2},   // WaterHopStairsA
      {0x0136, 0x00, 4, 2},   // WaterHopStairsB
      {0x0137, 0x00, 10, 4},  // DamFloodGate
      {0x013E, 0x00, 6, 3},   // Utility6x3
      {0x0FD5, 0x00, 3, 5},   // Utility3x5
      {0x0FBA, 0x00, 4, 6},   // VerticalTurtleRockPipe
      {0x0FBC, 0x00, 6, 4},   // HorizontalTurtleRockPipe
      {0x0FF0, 0x00, 4, 10},  // LightBeamOnFloor
      {0x0FF1, 0x00, 8, 8},   // BigLightBeamOnFloor
      {0x0FF2, 0x00, 4, 4},   // BossShell4x4
      {0x0FE9, 0x00, 3, 4},   // SolidWallDecor3x4
      {0x0FE0, 0x00, 3, 6},   // ArcheryGameTargetDoor
      {0x0FF8, 0x00, 8, 8},   // GanonTriforceFloorDecor
      {0x0055, 0x01, 16, 2},  // Wall torches: 4x2 stamps, 12-tile stride
      {0x0047, 0x00, 4, 5},   // Waterfall47 size=0
      {0x0048, 0x00, 4, 3},   // Waterfall48 size=0
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.object_id);
    RoomObject obj(test_case.object_id, /*x=*/0, /*y=*/0, test_case.size);
    auto bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
    ASSERT_TRUE(bounds.ok());
    EXPECT_EQ(bounds->width_tiles, test_case.width_tiles);
    EXPECT_EQ(bounds->height_tiles, test_case.height_tiles);
  }
}

TEST(ObjectGeometryTest, MeasureByObjectIdUnmappedReturnsError) {
  // 0xF8 is now mapped (routine 39), use a truly unmapped ID
  RoomObject obj(/*id=*/0x7FFF, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
  EXPECT_FALSE(bounds.ok());
}

TEST(ObjectGeometryTest, MeasureByObjectIdCachePopulated) {
  ObjectGeometry::Get().ClearCache();
  RoomObject obj(/*id=*/0x60, /*x=*/0, /*y=*/0, /*size=*/3);

  // First call: measures
  auto bounds1 = ObjectGeometry::Get().MeasureByObjectId(obj);
  ASSERT_TRUE(bounds1.ok());

  // Second call: should return cached result (same values)
  auto bounds2 = ObjectGeometry::Get().MeasureByObjectId(obj);
  ASSERT_TRUE(bounds2.ok());
  EXPECT_EQ(bounds1->width_tiles, bounds2->width_tiles);
  EXPECT_EQ(bounds1->height_tiles, bounds2->height_tiles);
}

TEST(ObjectGeometryTest, ClearCacheWorks) {
  ObjectGeometry::Get().ClearCache();
  RoomObject obj(/*id=*/0x00, /*x=*/0, /*y=*/0, /*size=*/0);
  auto bounds = ObjectGeometry::Get().MeasureByObjectId(obj);
  ASSERT_TRUE(bounds.ok());

  ObjectGeometry::Get().ClearCache();
  // Should still work after clearing
  auto bounds2 = ObjectGeometry::Get().MeasureByObjectId(obj);
  ASSERT_TRUE(bounds2.ok());
  EXPECT_EQ(bounds->width_tiles, bounds2->width_tiles);
}

}  // namespace yaze::zelda3
