#include "zelda3/dungeon/room_spatial.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "core/features.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "unit/zelda3/dungeon/room_census_test_rom.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/geometry/object_geometry.h"

namespace yaze::zelda3 {
namespace {

class RoomSpatialTest : public ::testing::Test {
 protected:
  void SetUp() override {
    flags_ = core::FeatureFlags::get();
    core::FeatureFlags::get().kEnableCustomObjects = false;
    DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    ObjectGeometry::Get().ClearCache();
    std::vector<uint8_t> bytes(0x200000, 0);
    // Every subtype pointer selects this positive tile payload. No CHR/palette
    // rasterization is needed to measure the actual tile writes.
    for (size_t index = 0; index < 256; ++index) {
      bytes[kRoomObjectTileAddress + index * 2] = 1 + index % 0xFE;
      bytes[kRoomObjectTileAddress + index * 2 + 1] = 0x20;
    }
    ASSERT_TRUE(rom_.LoadFromData(bytes).ok());
    before_ = rom_.vector();
    dirty_before_ = rom_.dirty();
  }
  void TearDown() override {
    EXPECT_EQ(rom_.vector(), before_);
    EXPECT_EQ(rom_.dirty(), dirty_before_);
    core::FeatureFlags::get() = flags_;
    DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    ObjectGeometry::Get().ClearCache();
  }
  core::FeatureFlags::Flags flags_;
  Rom rom_;
  std::vector<uint8_t> before_;
  bool dirty_before_ = false;

  std::vector<RoomObjectFootprint> WallFootprints(
      const std::vector<RoomObject>& objects) {
    std::vector<RoomObjectFootprint> result;
    for (size_t index = 0; index < objects.size(); ++index) {
      auto footprint = MeasureRoomObjectFootprint(&rom_, 0, objects[index]);
      EXPECT_TRUE(footprint.ok()) << footprint.status();
      if (!footprint.ok())
        return {};
      footprint->object_index = index;
      result.push_back(std::move(*footprint));
    }
    return result;
  }
};

TEST_F(RoomSpatialTest, RetainingJoinFindsTheRejectedZoraEndpointOnBothPlanes) {
  // Actual retained room26 relation28 ->3; graft writers76/77 (HIGH) or
  //95/96 (LOW). Positive synthetic payloads exercise real native raster draws.
  for (int pass : {0, 2}) {
    auto objects = WallFootprints({RoomObject(0x003, 21, 44, 7),
                                   RoomObject(0x10C, 37, 44, 0),
                                   RoomObject(0x064, 37, 43, 1, pass),
                                   RoomObject(0x10E, 37, 47, 0, pass)});
    ASSERT_EQ(objects.size(), 4);
    objects[0].object_index = 28;
    objects[1].object_index = 3;
    objects[2].object_index = pass == 0 ? 76 : 95;
    objects[3].object_index = pass == 0 ? 77 : 96;
    const auto issues = InspectRetainingWallJoins(objects);
    ASSERT_EQ(issues.size(), 1);
    const auto& issue = issues.front();
    EXPECT_EQ(issue.kind, "native_architectural_endpoint_loss");
    EXPECT_EQ(issue.face_index, 28);
    EXPECT_EQ(issue.corner_index, 3);
    EXPECT_FALSE(issue.face_is_layout);
    EXPECT_FALSE(issue.corner_is_layout);
    EXPECT_EQ(issue.upper_cells, 16);
    EXPECT_EQ(issue.lower_cells, 16);
    ASSERT_EQ(issue.lost_cells.size(), 32);
    for (const auto& cell : issue.lost_cells) {
      EXPECT_GE(cell.x, 37);
      EXPECT_LT(cell.x, 41);
      EXPECT_GE(cell.y, 44);
      EXPECT_LT(cell.y, 48);
    }
    ASSERT_EQ(issue.overwriters.size(), 2);
    EXPECT_EQ(issue.overwriters[0].object_index, objects[2].object_index);
    EXPECT_EQ(issue.overwriters[0].upper_cells, 12);
    EXPECT_EQ(issue.overwriters[0].lower_cells, 12);
    EXPECT_EQ(issue.overwriters[1].upper_cells, 4);
    EXPECT_EQ(issue.overwriters[1].lower_cells, 4);
    EXPECT_EQ(CountUnassessedRetainingWallObjects(objects), 0);
  }
}

TEST_F(RoomSpatialTest, CompleteVanilla36DonorIgnoresWaterAndDecorationPaint) {
  // Complete source connection14..23, not an isolated seven-object bank.
  auto objects = WallFootprints(
      {RoomObject(0x108, 17, 7, 0), RoomObject(0x003, 21, 7, 0),
       RoomObject(0x10A, 23, 7, 0), RoomObject(0x064, 23, 11, 1),
       RoomObject(0x10E, 23, 15, 0), RoomObject(0x003, 27, 15, 4),
       RoomObject(0x10C, 37, 15, 0), RoomObject(0x063, 37, 11, 1),
       RoomObject(0x108, 37, 7, 0), RoomObject(0x003, 41, 7, 3),
       RoomObject(0xC9, 27, 15, 15, 1), RoomObject(0x00, 37, 15, 1, 2)});
  ASSERT_EQ(objects.size(), 12);
  // The water really overlaps all16 lower corner writes. It is not a wall
  // endpoint replacement, and must not trigger a blanket overlap rejection.
  EXPECT_EQ(std::count_if(objects[10].cells.begin(), objects[10].cells.end(),
                          [](const auto& cell) {
                            return cell.layer == 1 && cell.x >= 37 &&
                                   cell.x < 41 && cell.y >= 15 && cell.y < 19;
                          }),
            16);
  EXPECT_TRUE(InspectRetainingWallJoins(objects).empty());
  EXPECT_EQ(CountUnassessedRetainingWallObjects(objects), 0);
}

TEST_F(RoomSpatialTest, NativeRightApproachMayShareOnlyTheCornerTopRow) {
  // Original GEN14 room26: preserved28 ->3, then native right face48.
  // Compatibility follows facing and the one-row junction, not word equality.
  auto objects = WallFootprints({RoomObject(0x003, 21, 44, 7),
                                 RoomObject(0x10C, 37, 44, 0),
                                 RoomObject(0x063, 37, 43, 0)});
  ASSERT_EQ(objects.size(), 3);
  objects[0].object_index = 28;
  objects[1].object_index = 3;
  objects[2].object_index = 48;
  EXPECT_EQ(objects[2].bounds.bottom(), 45);
  EXPECT_EQ(std::count_if(objects[2].cells.begin(), objects[2].cells.end(),
                          [](const auto& cell) { return cell.y == 44; }),
            8);  // Four top-row cells per physical plane.
  EXPECT_EQ(CountUnassessedRetainingWallObjects(objects), 0);
  EXPECT_TRUE(InspectRetainingWallJoins(objects).empty());
}

TEST_F(RoomSpatialTest, NorthApproachDoesNotWaiveDeepMirroredOrShiftedPaint) {
  for (const auto& [wall, lost_per_plane] :
       std::vector<std::pair<RoomObject, size_t>>{
           {RoomObject(0x063, 37, 43, 1), 12},
           {RoomObject(0x064, 37, 43, 0), 4},
           {RoomObject(0x063, 36, 43, 0), 3}}) {
    const auto objects = WallFootprints(
        {RoomObject(0x003, 21, 44, 7), RoomObject(0x10C, 37, 44, 0), wall});
    ASSERT_EQ(objects.size(), 3);
    const auto issues = InspectRetainingWallJoins(objects);
    ASSERT_EQ(issues.size(), 1) << wall.id_;
    EXPECT_EQ(issues[0].upper_cells, lost_per_plane);
    EXPECT_EQ(issues[0].lower_cells, lost_per_plane);
  }
}

TEST_F(RoomSpatialTest, JoinCoverageKeepsPartialLossAndNamespacesExplicit) {
  auto objects = WallFootprints({RoomObject(0x003, 21, 44, 7),
                                 RoomObject(0x10C, 37, 44, 0),
                                 RoomObject(0x064, 37, 43, 1)});
  ASSERT_EQ(objects.size(), 3);
  objects[0].is_layout = true;
  objects[0].object_index = 8;
  objects[1].object_index = 8;  // Equal index in a different namespace.
  const auto issues = InspectRetainingWallJoins(objects);
  ASSERT_EQ(issues.size(), 1);
  EXPECT_TRUE(issues[0].face_is_layout);
  EXPECT_FALSE(issues[0].corner_is_layout);
  EXPECT_EQ(issues[0].upper_cells, 12);
  EXPECT_EQ(issues[0].lower_cells, 12);
  objects[0].layers.custom_override_active = true;
  EXPECT_EQ(CountUnassessedRetainingWallObjects(objects), 1);
  EXPECT_TRUE(InspectRetainingWallJoins(objects).empty());
  objects[0].layers.custom_override_active = false;
  objects[1].full_bounds_measured = false;
  EXPECT_EQ(CountUnassessedRetainingWallObjects(objects), 1);
  EXPECT_TRUE(InspectRetainingWallJoins(objects).empty());
}

TEST_F(RoomSpatialTest, EquivalentNativeCornerRepaintPreservesTheTerminal) {
  auto objects = WallFootprints({RoomObject(0x003, 21, 44, 7),
                                 RoomObject(0x10C, 37, 44, 0),
                                 RoomObject(0x10C, 37, 44, 0, 2)});
  ASSERT_EQ(objects.size(), 3);
  EXPECT_TRUE(InspectRetainingWallJoins(objects).empty());
}

TEST_F(RoomSpatialTest, AnalysisChecksJoinsAfterSortingWithoutRenumbering) {
  Room room(0, &rom_);
  room.AddTileObject(RoomObject(0x064, 37, 43, 1, 2));
  room.AddTileObject(RoomObject(0x003, 21, 44, 7, 0));
  room.AddTileObject(RoomObject(0x10C, 37, 44, 0, 0));
  room.AddTileObject(RoomObject(0x10E, 37, 47, 0, 2));
  const auto analysis = AnalyzeRoomSpatial(&rom_, room);
  ASSERT_TRUE(analysis.ok()) << analysis.status();
  EXPECT_EQ(analysis->retaining_wall_join_coverage,
            "partial_native_003_east_10c_wall_stage");
  EXPECT_EQ(analysis->retaining_wall_join_unassessed_objects, 0);
  ASSERT_EQ(analysis->retaining_wall_join_issues.size(), 1);
  EXPECT_EQ(analysis->retaining_wall_join_issues[0].face_index, 1);
  EXPECT_EQ(analysis->retaining_wall_join_issues[0].corner_index, 2);
  EXPECT_EQ(analysis->retaining_wall_join_issues[0].overwriters[0].object_index,
            0);
}

TEST_F(RoomSpatialTest, FootprintUsesEightPixelUnitsAndSourceAnchor) {
  const RoomObject object(0x00, 5, 7, 1);
  const auto footprint = MeasureRoomObjectFootprint(&rom_, 0, object);
  ASSERT_TRUE(footprint.ok()) << footprint.status();
  EXPECT_TRUE(footprint->measured);
  EXPECT_TRUE(footprint->full_bounds_measured);
  EXPECT_EQ(footprint->anchor_x, 5);
  EXPECT_EQ(footprint->anchor_y, 7);
  EXPECT_EQ(footprint->bounds.x * 8, 40);
  EXPECT_EQ(footprint->bounds.y * 8, 56);
  EXPECT_EQ(footprint->bounds.width, 2);
  EXPECT_EQ(footprint->bounds.height, 2);
  EXPECT_EQ(footprint->cells.size(), 4);
  EXPECT_FALSE(footprint->outside_room);
}

TEST_F(RoomSpatialTest, UpwardExtentIsNotHiddenByClippedTraceOrInvalidSize) {
  const auto footprint =
      MeasureRoomObjectFootprint(&rom_, 0, RoomObject(0x09, 10, 2, 0));
  ASSERT_TRUE(footprint.ok()) << footprint.status();
  EXPECT_EQ(footprint->anchor_y, 2);
  EXPECT_EQ(footprint->bounds.y, -3);
  EXPECT_TRUE(footprint->outside_room);
  EXPECT_TRUE(footprint->full_bounds_measured);
  EXPECT_TRUE(std::all_of(footprint->cells.begin(), footprint->cells.end(),
                          [](const auto& cell) { return cell.y >= 0; }));
  EXPECT_FALSE(
      MeasureRoomObjectFootprint(&rom_, 0, RoomObject(0x100, 10, 10, 1)).ok());
}

TEST_F(RoomSpatialTest, BothBuffersAndThirdListRemainDistinctFromPlanes) {
  RoomObject both(0x00, 6, 8, 1, 2);
  both.all_bgs_ = true;
  const auto footprint = MeasureRoomObjectFootprint(&rom_, 0, both);
  ASSERT_TRUE(footprint.ok()) << footprint.status();
  EXPECT_EQ(footprint->list_index, 2);
  EXPECT_EQ(both.GetLayerValue(), 2);
  EXPECT_EQ(footprint->layers.effective_bg_layer,
            EffectiveBgLayer::kBothBg1Bg2);
  EXPECT_EQ(std::count_if(footprint->cells.begin(), footprint->cells.end(),
                          [](const auto& cell) { return cell.layer == 0; }),
            4);
  EXPECT_EQ(std::count_if(footprint->cells.begin(), footprint->cells.end(),
                          [](const auto& cell) { return cell.layer == 1; }),
            4);
  both.all_bgs_ = false;
  const auto upper = MeasureRoomObjectFootprint(&rom_, 0, both);
  ASSERT_TRUE(upper.ok()) << upper.status();
  EXPECT_TRUE(std::all_of(upper->cells.begin(), upper->cells.end(),
                          [](const auto& cell) { return cell.layer == 0; }));
}

TEST_F(RoomSpatialTest, MissingRasterPayloadCannotQualifyStandaloneGeometry) {
  auto bytes = before_;
  // A corrupt signed subtype offset points before the image. ObjectDrawer
  // skips the missing payload with OK; standalone geometry still draws its
  // positive dummy tiles and reports the ordinary C9 floor rectangle.
  bytes[kRoomObjectSubtype1 + 0xC9 * 2] = 0x00;
  bytes[kRoomObjectSubtype1 + 0xC9 * 2 + 1] = 0x90;
  Rom missing_payload;
  ASSERT_TRUE(missing_payload.LoadFromData(bytes).ok());
  const auto dirty = missing_payload.dirty();
  const auto footprint = MeasureRoomObjectFootprint(
      &missing_payload, 0, RoomObject(0xC9, 20, 20, 0));
  ASSERT_TRUE(footprint.ok()) << footprint.status();
  EXPECT_TRUE(footprint->cells.empty());
  EXPECT_EQ(footprint->bounds.width, 4);
  EXPECT_EQ(footprint->bounds.height, 4);
  EXPECT_FALSE(footprint->measured);
  EXPECT_FALSE(footprint->full_bounds_measured);
  EXPECT_EQ(footprint->measurement_source, "geometry_without_raster_trace");
  EXPECT_EQ(missing_payload.vector(), bytes);
  EXPECT_EQ(missing_payload.dirty(), dirty);
}

TEST_F(RoomSpatialTest, FloorOperatorsUseTheLiveRoomPatternSelectors) {
  auto bytes = test::BuildRoomCensusTestRom();
  bytes[test::kTestObjectDataPc] = 0x21;  // upper pattern1, lower pattern2
  for (int pattern = 0; pattern < 3; ++pattern) {
    for (int tile = 0; tile < 4; ++tile) {
      for (const int table :
           {kRoomObjectTileAddress, kRoomObjectTileAddressFloor}) {
        bytes[table + pattern * 16 + tile * 2] = 0x40 + pattern;
        bytes[table + pattern * 16 + tile * 2 + 1] = 0x20;
      }
    }
  }
  Rom floor_rom;
  ASSERT_TRUE(floor_rom.LoadFromData(bytes).ok());
  const auto dirty = floor_rom.dirty();
  for (const auto [id, word] :
       {std::pair{0xC4, 0x2041}, std::pair{0xDB, 0x2042}}) {
    const auto footprint =
        MeasureRoomObjectFootprint(&floor_rom, 1, RoomObject(id, 10, 10, 0));
    ASSERT_TRUE(footprint.ok()) << footprint.status();
    ASSERT_FALSE(footprint->cells.empty());
    EXPECT_EQ(footprint->cells.front().word, word);
  }
  Room edited_room(1, &floor_rom);
  edited_room.AddTileObject(RoomObject(0xC4, 10, 10, 0));
  edited_room.set_floor1(2);
  const auto edited = AnalyzeRoomSpatial(&floor_rom, edited_room);
  ASSERT_TRUE(edited.ok()) << edited.status();
  ASSERT_EQ(edited->objects.size(), 1);
  ASSERT_FALSE(edited->objects.front().cells.empty());
  EXPECT_EQ(edited->objects.front().cells.front().word, 0x2042);
  EXPECT_EQ(floor_rom.vector(), bytes);
  EXPECT_EQ(floor_rom.dirty(), dirty);
}

TEST_F(RoomSpatialTest, AnalysisPreservesPainterOrderAndReportsSharedWrites) {
  Room room(0, &rom_);
  room.AddTileObject(RoomObject(0x00, 8, 8, 1, 2));
  room.AddTileObject(RoomObject(0x00, 8, 8, 1, 0));
  room.AddTileObject(RoomObject(0x00, 8, 8, 1, 1));
  room.set_floor1(3);
  room.set_floor2(4);
  room.custom_collision().SetOverride(650,
                                      0);  // Explicit floor is still owned.
  Room::Door door;
  door.position = 6;
  door.direction = DoorDirection::South;
  door.type = DoorType::NormalDoor;
  room.AddDoor(door);
  RoomTilemaps tilemaps;
  tilemaps.bg1.assign(kRoomTilemapWords, 0);
  tilemaps.bg2.assign(kRoomTilemapWords, 0);
  RoomSpatialOptions options;
  options.tilemaps = &tilemaps;
  const auto analysis = AnalyzeRoomSpatial(&rom_, room, options);
  ASSERT_TRUE(analysis.ok()) << analysis.status();
  ASSERT_EQ(analysis->objects.size(), 3);
  EXPECT_EQ(analysis->objects[0].object_index, 1);
  EXPECT_EQ(analysis->objects[1].object_index, 2);
  EXPECT_EQ(analysis->objects[2].object_index, 0);
  ASSERT_EQ(analysis->overlaps.size(), 1);
  EXPECT_EQ(analysis->overlaps[0].upper_cells, 4);
  EXPECT_EQ(analysis->overlaps[0].lower_cells, 0);
  EXPECT_EQ(analysis->overlaps[0].classification, "ordered_shared_write");
  EXPECT_EQ(analysis->floor1, 3);
  EXPECT_EQ(analysis->floor2, 4);
  EXPECT_EQ(analysis->custom_collision_overrides, 1);
  EXPECT_TRUE(analysis->collision.has_value());
  ASSERT_EQ(analysis->doors.size(), 1);
  EXPECT_EQ(analysis->doors[0].anchor_x, 14);
  EXPECT_EQ(analysis->doors[0].anchor_y, 58);
  EXPECT_EQ(analysis->doors[0].bounds.y, 59);
  EXPECT_FALSE(analysis->explicit_object_state);
  EXPECT_EQ(room.GetTileObjects()[0].GetLayerValue(), 2);
}

}  // namespace
}  // namespace yaze::zelda3
