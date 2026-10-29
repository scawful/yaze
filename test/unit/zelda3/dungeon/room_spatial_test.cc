#include "zelda3/dungeon/room_spatial.h"

#include <algorithm>
#include <vector>

#include "core/features.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
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
};

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
