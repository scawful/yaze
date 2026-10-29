#include "zelda3/dungeon/track_collision_generator.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/features.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze::zelda3 {
namespace {

class ScopedCustomObjectsFlag {
 public:
  explicit ScopedCustomObjectsFlag(bool enabled)
      : previous_(core::FeatureFlags::get().kEnableCustomObjects) {
    core::FeatureFlags::get().kEnableCustomObjects = enabled;
    DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  }

  ~ScopedCustomObjectsFlag() {
    core::FeatureFlags::get().kEnableCustomObjects = previous_;
    DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  }

 private:
  bool previous_;
};

int CountExpectedTilesInBounds(const RoomObject& obj) {
  const auto dims = DimensionService::Get().GetDimensions(obj);
  int count = 0;
  for (int dy = 0; dy < std::max(1, dims.height_tiles); ++dy) {
    for (int dx = 0; dx < std::max(1, dims.width_tiles); ++dx) {
      const int x = obj.x_ + dims.offset_x_tiles + dx;
      const int y = obj.y_ + dims.offset_y_tiles + dy;
      if (x >= 0 && x < 64 && y >= 0 && y < 64) {
        ++count;
      }
    }
  }
  return count;
}

CustomCollisionMap MakeCollisionMap(
    std::initializer_list<std::pair<int, uint8_t>> tiles) {
  CustomCollisionMap map;
  map.tiles.fill(0);
  for (const auto& [offset, value] : tiles) {
    map.tiles[static_cast<size_t>(offset)] = value;
  }
  map.has_data = tiles.size() != 0;
  return map;
}

uint32_t ReadCollisionPointerPc(const Rom& rom, int room_id) {
  const int pointer_offset = kCustomCollisionRoomPointers + (room_id * 3);
  const auto& data = rom.vector();
  const uint32_t snes_address = data[pointer_offset] |
                                (data[pointer_offset + 1] << 8) |
                                (data[pointer_offset + 2] << 16);
  return SnesToPc(snes_address);
}

void CopyCollisionPointer(Rom* rom, int source_room_id, int target_room_id) {
  const int source = kCustomCollisionRoomPointers + (source_room_id * 3);
  const int target = kCustomCollisionRoomPointers + (target_room_id * 3);
  for (int i = 0; i < 3; ++i) {
    rom->mutable_data()[target + i] = rom->vector()[source + i];
  }
}

void SetCollisionPointer(Rom* rom, int room_id, uint32_t pc_address) {
  const uint32_t snes_address = PcToSnes(pc_address);
  const int pointer_offset = kCustomCollisionRoomPointers + (room_id * 3);
  rom->mutable_data()[pointer_offset] = snes_address & 0xFF;
  rom->mutable_data()[pointer_offset + 1] = (snes_address >> 8) & 0xFF;
  rom->mutable_data()[pointer_offset + 2] = (snes_address >> 16) & 0xFF;
}

TEST(MinecartTrackCollisionSyncTest, StraightArtworkUsesItsOrientation) {
  // A 16 pixel piece covers four 8 pixel collision cells. Connectivity-based
  // generation incorrectly makes an isolated 2x2 straight into four corners.
  for (const auto& [subtype, expected] :
       std::array<std::pair<uint8_t, uint8_t>, 4>{
           {{0, 0xB0}, {7, 0xB0}, {1, 0xB1}, {6, 0xB1}}}) {
    SCOPED_TRACE(::testing::Message() << "subtype=" << +subtype);
    const std::vector<RoomObject> objects{RoomObject(0x31, 10, 12, subtype)};
    const CustomCollisionMap original;
    auto edit = SynchronizeMinecartTrackCollision({}, objects, original);
    ASSERT_TRUE(edit.ok()) << edit.status();
    EXPECT_EQ(edit->ownership.size(), 4);
    for (const int offset : {778, 779, 842, 843}) {
      EXPECT_EQ(edit->collision.tiles[offset], expected);
      EXPECT_TRUE(edit->collision.HasOverride(offset));
      ASSERT_TRUE(edit->ownership.contains(offset));
      EXPECT_FALSE(edit->ownership.at(offset).baseline_override);
      EXPECT_EQ(edit->ownership.at(offset).baseline_value, 0);
      EXPECT_EQ(edit->ownership.at(offset).generated_value, expected);
    }
    EXPECT_EQ(edit->collision.overrides.count(), 4);
    EXPECT_TRUE(edit->collision.has_data);
    EXPECT_FALSE(original.has_data);
    EXPECT_FALSE(original.HasOverrides());
  }
}

TEST(MinecartTrackCollisionSyncTest,
     MoveAndDeleteRestoreExactBaselinesAndPreserveAuthoredBehavior) {
  CustomCollisionMap original;
  original.SetOverride(650, 0);     // Explicit floor underneath the new piece.
  original.tiles[651] = 0xB3;       // Legacy nonzero override, without a bit.
  original.SetOverride(714, 0xB7);  // Authored stop must not be adopted.
  original.SetOverride(1200, 0);    // Unrelated explicit floor survives.
  original.SetOverride(1201, 0x20);
  const std::vector<RoomObject> start{RoomObject(0x31, 10, 10, 0)};
  const std::vector<RoomObject> moved{RoomObject(0x31, 20, 20, 0)};

  auto placed = SynchronizeMinecartTrackCollision({}, start, original);
  ASSERT_TRUE(placed.ok()) << placed.status();
  EXPECT_EQ(placed->ownership.size(), 3);
  EXPECT_FALSE(placed->ownership.contains(714));
  auto move = SynchronizeMinecartTrackCollision(start, moved, placed->collision,
                                                placed->ownership);
  ASSERT_TRUE(move.ok()) << move.status();
  EXPECT_EQ(move->ownership.size(), 4);
  for (const int offset : {650, 651, 714, 715, 1200, 1201}) {
    EXPECT_EQ(move->collision.tiles[offset], original.tiles[offset]);
    EXPECT_EQ(move->collision.overrides[offset], original.overrides[offset]);
  }
  auto removed = SynchronizeMinecartTrackCollision(moved, {}, move->collision,
                                                   move->ownership);
  ASSERT_TRUE(removed.ok()) << removed.status();
  EXPECT_EQ(removed->collision, original);
  EXPECT_EQ(removed->collision.overrides, original.overrides);
  EXPECT_TRUE(removed->ownership.empty());
}

TEST(MinecartTrackCollisionSyncTest,
     MatchingAuthoredRailsAreNeverAdoptedOrRemoved) {
  CustomCollisionMap original;
  for (const int offset : {650, 651, 714, 715}) {
    original.SetOverride(offset, 0xB0);
  }
  const std::vector<RoomObject> objects{RoomObject(0x31, 10, 10, 0)};
  auto placed = SynchronizeMinecartTrackCollision({}, objects, original);
  ASSERT_TRUE(placed.ok()) << placed.status();
  EXPECT_EQ(placed->collision, original);
  EXPECT_TRUE(placed->ownership.empty());
  auto removed = SynchronizeMinecartTrackCollision(
      objects, {}, placed->collision, placed->ownership);
  ASSERT_TRUE(removed.ok()) << removed.status();
  EXPECT_EQ(removed->collision, original);
  EXPECT_TRUE(removed->ownership.empty());
}

TEST(MinecartTrackCollisionSyncTest,
     ManualChangesInvalidateOwnershipWithoutRestoringOverThem) {
  const std::vector<RoomObject> objects{RoomObject(0x31, 10, 10, 0)};
  auto placed = SynchronizeMinecartTrackCollision({}, objects, {});
  ASSERT_TRUE(placed.ok()) << placed.status();
  auto manual = placed->collision;
  manual.SetOverride(650, 0xB9);
  manual.SetOverride(651, 0);
  manual.ClearOverride(714);
  auto removed =
      SynchronizeMinecartTrackCollision(objects, {}, manual, placed->ownership);
  ASSERT_TRUE(removed.ok()) << removed.status();
  EXPECT_EQ(removed->collision.tiles[650], 0xB9);
  EXPECT_TRUE(removed->collision.HasOverride(651));
  EXPECT_EQ(removed->collision.tiles[651], 0);
  EXPECT_FALSE(removed->collision.HasOverride(714));
  EXPECT_FALSE(removed->collision.HasOverride(715));
  EXPECT_TRUE(removed->ownership.empty());
}

TEST(MinecartTrackCollisionSyncTest, StopsJunctionsAndSwitchesRemainAuthored) {
  for (const uint8_t value : {0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
                              0xD0, 0xD1, 0xD2, 0xD3}) {
    SCOPED_TRACE(::testing::Message() << "behavior=" << +value);
    CustomCollisionMap original;
    original.SetOverride(650, value);
    const std::vector<RoomObject> objects{RoomObject(0x31, 10, 10, 0)};
    auto placed = SynchronizeMinecartTrackCollision({}, objects, original);
    ASSERT_TRUE(placed.ok()) << placed.status();
    EXPECT_EQ(placed->collision.tiles[650], value);
    EXPECT_FALSE(placed->ownership.contains(650));
    auto removed = SynchronizeMinecartTrackCollision(
        objects, {}, placed->collision, placed->ownership);
    ASSERT_TRUE(removed.ok()) << removed.status();
    EXPECT_EQ(removed->collision, original);
  }
}

TEST(MinecartTrackCollisionSyncTest,
     OrientationChangesRetainTheOriginalBaselineForUndoSnapshots) {
  CustomCollisionMap original;
  original.SetOverride(650, 0);
  const std::vector<RoomObject> horizontal{RoomObject(0x31, 10, 10, 0)};
  const std::vector<RoomObject> vertical{RoomObject(0x31, 10, 10, 1)};
  auto placed = SynchronizeMinecartTrackCollision({}, horizontal, original);
  ASSERT_TRUE(placed.ok()) << placed.status();
  auto rotated = SynchronizeMinecartTrackCollision(
      horizontal, vertical, placed->collision, placed->ownership);
  ASSERT_TRUE(rotated.ok()) << rotated.status();
  EXPECT_EQ(rotated->collision.tiles[650], 0xB1);
  EXPECT_EQ(rotated->ownership.at(650).generated_value, 0xB1);
  EXPECT_TRUE(rotated->ownership.at(650).baseline_override);
  EXPECT_EQ(rotated->ownership.at(650).baseline_value, 0);
  // The earlier value snapshots remain intact for a paired object Undo/Redo.
  EXPECT_EQ(placed->collision.tiles[650], 0xB0);
  EXPECT_EQ(placed->ownership.at(650).generated_value, 0xB0);
  auto removed = SynchronizeMinecartTrackCollision(
      vertical, {}, rotated->collision, rotated->ownership);
  ASSERT_TRUE(removed.ok()) << removed.status();
  EXPECT_EQ(removed->collision, original);
  EXPECT_EQ(removed->collision.overrides, original.overrides);
}

TEST(MinecartTrackCollisionSyncTest,
     UnrelatedEditsAndEquivalentArtworkDoNotRegenerateCollision) {
  CustomCollisionMap original;
  original.SetOverride(650, 0x20);
  original.SetOverride(999, 0);
  const MinecartTrackCollisionOwnership ownership{{1000, {false, 0, 0xB1}}};
  const std::vector<RoomObject> before{RoomObject(0x31, 10, 10, 0),
                                       RoomObject(0x31, 63, 63, 1, 1),
                                       RoomObject(0x01, 1, 1, 0)};
  const std::vector<RoomObject> after{RoomObject(0x01, 20, 20, 2),
                                      RoomObject(0x31, 63, 63, 6, 1),
                                      RoomObject(0x31, 10, 10, 7)};
  auto edit =
      SynchronizeMinecartTrackCollision(before, after, original, ownership);
  ASSERT_TRUE(edit.ok()) << edit.status();
  EXPECT_EQ(edit->collision, original);
  EXPECT_EQ(edit->collision.overrides, original.overrides);
  EXPECT_EQ(edit->ownership, ownership);

  auto added = after;
  added.emplace_back(0x31, 30, 30, 1);
  auto extension =
      SynchronizeMinecartTrackCollision(after, added, original, ownership);
  ASSERT_TRUE(extension.ok()) << extension.status();
  EXPECT_EQ(extension->collision.tiles[650], 0x20);
  EXPECT_FALSE(extension->collision.HasOverride(651));
  EXPECT_EQ(extension->collision.tiles[1950], 0xB1);
}

TEST(MinecartTrackCollisionSyncTest,
     ChangedLowerLayerAndOutOfBoundsPiecesFailWithoutClipping) {
  for (const auto& object :
       {RoomObject(0x31, 63, 10, 0), RoomObject(0x31, 10, 63, 0),
        RoomObject(0x31, 64, 10, 1), RoomObject(0x31, 10, 10, 0, 1),
        RoomObject(0x31, 10, 10, 0, 2)}) {
    const std::vector<RoomObject> invalid{object};
    EXPECT_FALSE(SynchronizeMinecartTrackCollision({}, invalid, {}).ok());
    EXPECT_FALSE(SynchronizeMinecartTrackCollision(invalid, {}, {}).ok());
  }
  const std::vector<RoomObject> edge{RoomObject(0x31, 62, 62, 0)};
  auto valid = SynchronizeMinecartTrackCollision({}, edge, {});
  ASSERT_TRUE(valid.ok()) << valid.status();
  EXPECT_EQ(valid->collision.tiles[4095], 0xB0);
}

TEST(MinecartTrackCollisionSyncTest,
     CornersAndCrossingsUseTheNewPlacementContractAndRestoreBaselines) {
  // New placements use the explicit canonical policy. Existing saved rooms
  // may have nonuniform authored corner cells and must not be regenerated.
  constexpr std::pair<uint8_t, uint8_t> cases[] = {
      {2, 0xB2}, {3, 0xB4},  {4, 0xB3},  {5, 0xB5},  {8, 0xB2},
      {9, 0xB4}, {10, 0xB3}, {11, 0xB5}, {12, 0xB6}, {14, 0xB6}};
  for (const auto& [subtype, expected] : cases) {
    SCOPED_TRACE(::testing::Message() << "subtype=" << +subtype);
    const std::vector<RoomObject> objects{RoomObject(0x31, 10, 10, subtype)};
    CustomCollisionMap original;
    original.SetOverride(650, 0);
    original.SetOverride(999, 0x20);
    auto placed = SynchronizeMinecartTrackCollision({}, objects, original);
    ASSERT_TRUE(placed.ok()) << placed.status();
    EXPECT_EQ(placed->ownership.size(), 4);
    for (const int offset : {650, 651, 714, 715}) {
      EXPECT_EQ(placed->collision.tiles[offset], expected);
      EXPECT_TRUE(placed->collision.HasOverride(offset));
    }
    auto removed = SynchronizeMinecartTrackCollision(
        objects, {}, placed->collision, placed->ownership);
    ASSERT_TRUE(removed.ok()) << removed.status();
    EXPECT_EQ(removed->collision, original);
    EXPECT_EQ(removed->collision.overrides, original.overrides);
    EXPECT_TRUE(removed->ownership.empty());

    // A real saved TL example has [B2,B0;B1,B2], not four equal cells.
    original.SetOverride(650, 0xB2);
    original.SetOverride(651, 0xB0);
    original.SetOverride(714, 0xB1);
    original.SetOverride(715, 0xB2);
    auto unchanged =
        SynchronizeMinecartTrackCollision(objects, objects, original);
    ASSERT_TRUE(unchanged.ok()) << unchanged.status();
    EXPECT_EQ(unchanged->collision, original);
  }
  const std::vector<RoomObject> decorations{RoomObject(0x31, 10, 10, 13),
                                            RoomObject(0x31, 12, 10, 15)};
  auto decorative = SynchronizeMinecartTrackCollision({}, decorations, {});
  ASSERT_TRUE(decorative.ok()) << decorative.status();
  EXPECT_FALSE(decorative->collision.HasOverrides());
  EXPECT_TRUE(decorative->ownership.empty());
}

TEST(MinecartRailCatalogTest, ExactBoundedMarkerSelectsExpandedProfile) {
  constexpr std::array<uint8_t, 8> marker = {'M', 'C', 'R', '2', 16, 13, 2, 0};
  std::vector<uint8_t> bytes(kMinecartRailProfileMarkerPc + marker.size(), 0);
  EXPECT_EQ(DetectMinecartRailProfile(bytes), MinecartRailProfile::kLegacy);
  std::copy(marker.begin(), marker.end(),
            bytes.begin() + kMinecartRailProfileMarkerPc);
  EXPECT_EQ(DetectMinecartRailProfile(bytes), MinecartRailProfile::kExpanded);
  EXPECT_EQ(DetectMinecartRailProfile(
                std::span<const uint8_t>(bytes).first(bytes.size() - 1)),
            MinecartRailProfile::kLegacy);
  for (size_t index = 0; index < marker.size(); ++index) {
    bytes[kMinecartRailProfileMarkerPc + index] ^= 1;
    EXPECT_EQ(DetectMinecartRailProfile(bytes), MinecartRailProfile::kLegacy);
    bytes[kMinecartRailProfileMarkerPc + index] ^= 1;
  }
}

TEST(MinecartRailCatalogTest, LegacyAndExpandedSlotsRemainExplicit) {
  EXPECT_EQ(MinecartRailDescriptors().size(), 14);
  EXPECT_EQ(MinecartRailDescriptors(MinecartRailProfile::kExpanded).size(), 22);
  ASSERT_NE(FindMinecartRailDescriptor(0x31, 12), nullptr);
  EXPECT_EQ(FindMinecartRailDescriptor(0x31, 12)->shape,
            MinecartRailShape::kCrossing);
  EXPECT_EQ(FindMinecartRailDescriptor(0x31, 13), nullptr);
  EXPECT_EQ(FindMinecartRailDescriptor(0x31, 15), nullptr);
  EXPECT_EQ(FindMinecartRailDescriptor(0x32, 5), nullptr);
  EXPECT_EQ(FindMinecartRailDescriptor(0x31, 16), nullptr);
  const auto* floor_cross =
      FindMinecartRailDescriptor(0x32, 5, MinecartRailProfile::kExpanded);
  ASSERT_NE(floor_cross, nullptr);
  EXPECT_EQ(floor_cross->shape, MinecartRailShape::kCrossing);
  EXPECT_EQ(floor_cross->style, MinecartRailStyle::kFloor);
  const auto* pit_cross =
      FindMinecartRailDescriptor(0x32, 7, MinecartRailProfile::kExpanded);
  ASSERT_NE(pit_cross, nullptr);
  EXPECT_EQ(pit_cross->style, MinecartRailStyle::kPit);
  for (const int subtype : {0, 1, 2, 3, 4, 6, 8, 13}) {
    EXPECT_EQ(FindMinecartRailDescriptor(0x32, subtype,
                                         MinecartRailProfile::kExpanded),
              nullptr);
  }
}

TEST(MinecartTrackCollisionSyncTest,
     ExpandedJunctionsOwnOnlyTheExistingEightPixelNodeAnchor) {
  for (const int family : {0x31, 0x32}) {
    for (int orientation = 0; orientation < 4; ++orientation) {
      const int subtype = (family == 0x31 ? 12 : 9) + orientation;
      SCOPED_TRACE(::testing::Message()
                   << "family=" << family << " subtype=" << subtype);
      const std::vector<RoomObject> objects{
          RoomObject(family, 10, 10, subtype)};
      CustomCollisionMap original;
      original.SetOverride(650, 0);
      original.SetOverride(651, 0x20);
      original.SetOverride(714, 0xB9);
      original.SetOverride(715, 0);
      auto placed = SynchronizeMinecartTrackCollision(
          {}, objects, original, {}, MinecartRailProfile::kExpanded);
      ASSERT_TRUE(placed.ok()) << placed.status();
      EXPECT_EQ(placed->ownership.size(), 1);
      EXPECT_EQ(placed->collision.tiles[650], 0xBB + orientation);
      for (const int offset : {651, 714, 715}) {
        EXPECT_EQ(placed->collision.tiles[offset], original.tiles[offset]);
        EXPECT_EQ(placed->collision.overrides[offset],
                  original.overrides[offset]);
      }
      auto removed = SynchronizeMinecartTrackCollision(
          objects, {}, placed->collision, placed->ownership,
          MinecartRailProfile::kExpanded);
      ASSERT_TRUE(removed.ok()) << removed.status();
      EXPECT_EQ(removed->collision, original);
      EXPECT_EQ(removed->collision.overrides, original.overrides);

      Room room;
      room.AddTileObject(objects.front());
      room.custom_collision() = original;
      room.custom_collision().ClearOverride(650);
      GeneratorOptions options;
      options.rail_profile = MinecartRailProfile::kExpanded;
      auto generated = GenerateTrackCollision(&room, options);
      ASSERT_TRUE(generated.ok()) << generated.status();
      EXPECT_EQ(generated->tiles_generated, 1);
      EXPECT_EQ(generated->collision_map, placed->collision);
      EXPECT_EQ(room.custom_collision(), original);
    }
  }
}

TEST(TrackCollisionGeneratorTest,
     CanonicalBatchDoesNotRegenerateAuthoredCornerStencilOrExplicitFloor) {
  Room room;
  room.AddTileObject(RoomObject(0x31, 10, 10, 2));
  CustomCollisionMap original;
  // Existing saved TL stencil from the audited Oracle room $87.
  original.SetOverride(650, 0xB2);
  original.SetOverride(651, 0xB0);
  original.SetOverride(714, 0xB1);
  original.SetOverride(715, 0xB2);
  original.SetOverride(999, 0);
  room.custom_collision() = original;
  auto generated = GenerateTrackCollision(&room);
  ASSERT_TRUE(generated.ok()) << generated.status();
  EXPECT_EQ(generated->collision_map, original);
  EXPECT_EQ(generated->collision_map.overrides, original.overrides);
  EXPECT_EQ(room.custom_collision(), original);

  room.custom_collision().SetOverride(650, 0);
  generated = GenerateTrackCollision(&room);
  ASSERT_TRUE(generated.ok()) << generated.status();
  EXPECT_TRUE(generated->collision_map.HasOverride(650));
  EXPECT_EQ(generated->collision_map.tiles[650], 0);
}

TEST(TrackCollisionGeneratorTest,
     CanonicalBatchUsesPlacementShapesAndPreservesAuthoredNodes) {
  for (const auto profile :
       {MinecartRailProfile::kLegacy, MinecartRailProfile::kExpanded}) {
    for (const auto& rail : MinecartRailDescriptors(profile)) {
      Room room;
      const std::vector<RoomObject> objects{
          RoomObject(rail.object_id, 10, 10, rail.subtype)};
      room.AddTileObject(objects.front());
      CustomCollisionMap original;
      original.SetOverride(650, 0xD2);
      original.SetOverride(999, 0);
      room.custom_collision() = original;
      auto placed =
          SynchronizeMinecartTrackCollision({}, objects, original, {}, profile);
      ASSERT_TRUE(placed.ok()) << placed.status();
      GeneratorOptions options;
      options.rail_profile = profile;
      auto generated = GenerateTrackCollision(&room, options);
      ASSERT_TRUE(generated.ok()) << generated.status();
      EXPECT_EQ(generated->collision_map, placed->collision);
      EXPECT_EQ(generated->collision_map.tiles[650], 0xD2);
      EXPECT_TRUE(generated->collision_map.HasOverride(999));
      EXPECT_EQ(generated->collision_map.tiles[999], 0);
      EXPECT_EQ(room.custom_collision(), original);
    }
  }
}

TEST(MinecartTrackCollisionSyncTest,
     ConflictingShapesAndNonRailCollisionFailAtomically) {
  const std::vector<RoomObject> before{RoomObject(0x31, 10, 10, 0)};
  auto placed = SynchronizeMinecartTrackCollision({}, before, {});
  ASSERT_TRUE(placed.ok()) << placed.status();
  auto overlap = before;
  overlap.emplace_back(0x31, 11, 10, 1);
  const auto collision_snapshot = placed->collision;
  const auto ownership_snapshot = placed->ownership;
  EXPECT_FALSE(SynchronizeMinecartTrackCollision(
                   before, overlap, placed->collision, placed->ownership)
                   .ok());
  EXPECT_EQ(placed->collision, collision_snapshot);
  EXPECT_EQ(placed->ownership, ownership_snapshot);

  const std::vector<RoomObject> corner{RoomObject(0x31, 10, 10, 2)};
  auto corner_overlap = corner;
  corner_overlap.emplace_back(0x31, 11, 10, 0);
  EXPECT_FALSE(
      SynchronizeMinecartTrackCollision(corner, corner_overlap, {}).ok());

  CustomCollisionMap obstacle;
  obstacle.SetOverride(715, 0x20);  // Last cell: no partial first-three stamp.
  auto refused = SynchronizeMinecartTrackCollision({}, before, obstacle);
  EXPECT_FALSE(refused.ok());
  EXPECT_FALSE(obstacle.HasOverride(650));
  EXPECT_EQ(obstacle.overrides.count(), 1);
  EXPECT_EQ(obstacle.tiles[715], 0x20);
}

TEST(MinecartTrackCollisionSyncTest,
     SameShapeOverlapKeepsCellsStillCoveredByAnotherPiece) {
  const std::vector<RoomObject> first{RoomObject(0x31, 10, 10, 0)};
  auto placed = SynchronizeMinecartTrackCollision({}, first, {});
  ASSERT_TRUE(placed.ok()) << placed.status();
  auto both = first;
  both.emplace_back(0x31, 11, 10, 7);
  auto extended = SynchronizeMinecartTrackCollision(
      first, both, placed->collision, placed->ownership);
  ASSERT_TRUE(extended.ok()) << extended.status();
  EXPECT_EQ(extended->ownership.size(), 6);
  const std::vector<RoomObject> second{both.back()};
  auto removed_first = SynchronizeMinecartTrackCollision(
      both, second, extended->collision, extended->ownership);
  ASSERT_TRUE(removed_first.ok()) << removed_first.status();
  EXPECT_EQ(removed_first->ownership.size(), 4);
  EXPECT_FALSE(removed_first->collision.HasOverride(650));
  EXPECT_EQ(removed_first->collision.tiles[651], 0xB0);
  EXPECT_EQ(removed_first->collision.tiles[652], 0xB0);
  auto removed_all = SynchronizeMinecartTrackCollision(
      second, {}, removed_first->collision, removed_first->ownership);
  ASSERT_TRUE(removed_all.ok()) << removed_all.status();
  EXPECT_FALSE(removed_all->collision.HasOverrides());
  EXPECT_TRUE(removed_all->ownership.empty());
  // Keep the empty custom-collision domain present for the normal clear save.
  EXPECT_TRUE(removed_all->collision.has_data);
}

TEST(TrackCollisionGeneratorTest, UsesDimensionServiceNotLegacyWidthHeight) {
  Room room;
  RoomObject track_obj(0x01, 10, 10, 0, 0);

  // Legacy width_/height_ can be stale; generator must ignore them.
  track_obj.width_ = 16;
  track_obj.height_ = 16;
  room.AddTileObject(track_obj);

  const int expected = CountExpectedTilesInBounds(track_obj);
  ASSERT_GT(expected, 0);

  GeneratorOptions options;
  options.track_object_id = 0x01;
  auto result_or = GenerateTrackCollision(&room, options);
  ASSERT_TRUE(result_or.ok()) << result_or.status();

  const auto& result = result_or.value();
  EXPECT_EQ(result.tiles_generated, expected);
}

TEST(TrackCollisionGeneratorTest,
     CanonicalTrackSubtypesFallbackToTwoByTwoWithoutCustomGeometry) {
  ScopedCustomObjectsFlag custom_objects(/*enabled=*/false);
  constexpr std::array<uint8_t, 14> kTrackSubtypes = {0, 1, 2, 3,  4,  5,  6,
                                                      7, 8, 9, 10, 11, 12, 14};

  const auto idx = [](int x, int y) {
    return static_cast<size_t>(y * 64 + x);
  };
  for (const uint8_t subtype : kTrackSubtypes) {
    SCOPED_TRACE(::testing::Message() << "subtype=" << +subtype);
    Room room;
    room.AddTileObject(RoomObject(0x31, 10, 10, subtype, 0));

    auto result_or = GenerateTrackCollision(&room, GeneratorOptions{});
    ASSERT_TRUE(result_or.ok()) << result_or.status();

    const auto& result = result_or.value();
    EXPECT_EQ(result.tiles_generated, 4);
    EXPECT_NE(result.collision_map.tiles[idx(10, 10)], 0);
    EXPECT_NE(result.collision_map.tiles[idx(11, 10)], 0);
    EXPECT_NE(result.collision_map.tiles[idx(10, 11)], 0);
    EXPECT_NE(result.collision_map.tiles[idx(11, 11)], 0);
  }
}

TEST(TrackCollisionGeneratorTest,
     CanonicalDecorativeSubtypesDoNotGenerateTrackCollision) {
  ScopedCustomObjectsFlag custom_objects(/*enabled=*/false);

  for (const uint8_t subtype : {13, 15}) {
    SCOPED_TRACE(::testing::Message() << "subtype=" << +subtype);
    Room room;
    room.AddTileObject(RoomObject(0x31, 10, 10, subtype, 0));

    auto result_or = GenerateTrackCollision(&room, GeneratorOptions{});
    ASSERT_TRUE(result_or.ok()) << result_or.status();
    EXPECT_EQ(result_or->tiles_generated, 0);
    EXPECT_FALSE(result_or->collision_map.has_data);
    EXPECT_TRUE(std::all_of(result_or->collision_map.tiles.begin(),
                            result_or->collision_map.tiles.end(),
                            [](uint8_t tile) { return tile == 0; }));
  }
}

TEST(TrackCollisionGeneratorTest,
     ConfiguredNoncanonicalTrackIdPreservesAllSubtypeBehavior) {
  Room room;
  room.AddTileObject(RoomObject(0x35, 10, 10, 13, 0));

  GeneratorOptions options;
  options.track_object_id = 0x35;
  auto result_or = GenerateTrackCollision(&room, options);
  ASSERT_TRUE(result_or.ok()) << result_or.status();

  EXPECT_GT(result_or->tiles_generated, 0);
  EXPECT_TRUE(result_or->collision_map.has_data);
}

TEST(TrackCollisionGeneratorTest,
     ConfiguredSingleTileTrackIdPreservesOneByOneGeometry) {
  Room room;
  room.AddTileObject(RoomObject(0x35, 10, 10, 0, 0));

  GeneratorOptions options;
  options.track_object_id = 0x35;
  auto result_or = GenerateTrackCollision(&room, options);
  ASSERT_TRUE(result_or.ok()) << result_or.status();

  const auto& result = result_or.value();
  const auto idx = [](int x, int y) {
    return static_cast<size_t>(y * 64 + x);
  };
  EXPECT_EQ(result.tiles_generated, 1);
  EXPECT_NE(result.collision_map.tiles[idx(10, 10)], 0);
  EXPECT_EQ(result.collision_map.tiles[idx(11, 10)], 0);
  EXPECT_EQ(result.collision_map.tiles[idx(10, 11)], 0);
}

TEST(TrackCollisionGeneratorTest, LegacyWidthHeightDoesNotChangeOutput) {
  Room room_a;
  RoomObject obj_a(0x31, 18, 22, 0, 0);
  obj_a.width_ = 16;
  obj_a.height_ = 16;
  room_a.AddTileObject(obj_a);

  Room room_b;
  RoomObject obj_b(0x31, 18, 22, 0, 0);
  obj_b.width_ = 1;
  obj_b.height_ = 1;
  room_b.AddTileObject(obj_b);

  auto res_a_or = GenerateTrackCollision(&room_a, GeneratorOptions{});
  auto res_b_or = GenerateTrackCollision(&room_b, GeneratorOptions{});

  ASSERT_TRUE(res_a_or.ok()) << res_a_or.status();
  ASSERT_TRUE(res_b_or.ok()) << res_b_or.status();

  const auto& res_a = res_a_or.value();
  const auto& res_b = res_b_or.value();

  EXPECT_EQ(res_a.tiles_generated, res_b.tiles_generated);
  EXPECT_EQ(res_a.stop_count, res_b.stop_count);
  EXPECT_EQ(res_a.corner_count, res_b.corner_count);
  EXPECT_EQ(res_a.switch_count, res_b.switch_count);
  EXPECT_EQ(res_a.collision_map.tiles, res_b.collision_map.tiles);
}

TEST(TrackCollisionGeneratorTest, TJunctionsMatchOracleClosedSideAndRoundTrip) {
  // Independent contract from minecart.asm CheckForPlayerInput:
  // $BB disallows up, $BC down, $BD right, $BE left. Names denote the
  // closed side, not the open stem. Use one-cell configured rail objects
  // to exercise generation from actual neighboring placements.
  struct Junction {
    int closed_dx;
    int closed_dy;
    uint8_t expected;
  };
  constexpr Junction cases[] = {
      {0, -1, 0xBB}, {0, 1, 0xBC}, {1, 0, 0xBD}, {-1, 0, 0xBE}};
  constexpr std::pair<int, int> directions[] = {
      {0, -1}, {0, 1}, {-1, 0}, {1, 0}};
  ScopedCustomObjectsFlag custom_objects(/*enabled=*/false);
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kCustomCollisionDataEnd, 0)).ok());
  GeneratorOptions options;
  options.track_object_id = 0x35;
  for (const auto& junction : cases) {
    SCOPED_TRACE(static_cast<int>(junction.expected));
    Room room;
    room.AddTileObject(RoomObject(0x35, 20, 20, 0, 0));
    for (const auto& [dx, dy] : directions) {
      if (dx != junction.closed_dx || dy != junction.closed_dy)
        room.AddTileObject(RoomObject(0x35, 20 + dx, 20 + dy, 0, 0));
    }
    const auto generated = GenerateTrackCollision(&room, options);
    ASSERT_TRUE(generated.ok()) << generated.status();
    ASSERT_EQ(generated->tiles_generated, 4);
    EXPECT_EQ(generated->collision_map.tiles[20 * 64 + 20], junction.expected);
    EXPECT_EQ(
        generated->collision_map
            .tiles[(20 + junction.closed_dy) * 64 + 20 + junction.closed_dx],
        0);
    ASSERT_TRUE(WriteTrackCollision(&rom, 0x89, generated->collision_map).ok());
    const auto reopened = LoadCustomCollisionMap(&rom, 0x89);
    ASSERT_TRUE(reopened.ok()) << reopened.status();
    EXPECT_EQ(reopened->tiles, generated->collision_map.tiles);
  }
}

TEST(TrackCollisionGeneratorTest,
     WriteReusesUniqueCurrentBlobWhenReplacementFits) {
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kCustomCollisionDataEnd, 0x00))
          .ok());
  const CustomCollisionMap initial =
      MakeCollisionMap({{0, 0xB0}, {1, 0xB1}, {2, 0xB2}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, initial).ok());
  const uint32_t initial_pointer = ReadCollisionPointerPc(rom, 0x25);
  ASSERT_EQ(initial_pointer,
            static_cast<uint32_t>(kCustomCollisionDataPosition));

  const CustomCollisionMap replacement = MakeCollisionMap({{65, 0xB7}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, replacement).ok());

  EXPECT_EQ(ReadCollisionPointerPc(rom, 0x25), initial_pointer);
  auto loaded_or = LoadCustomCollisionMap(&rom, 0x25);
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status();
  EXPECT_EQ(loaded_or->tiles, replacement.tiles);

  constexpr size_t kInitialEncodedSize = 2 + (3 * 3) + 2;
  constexpr size_t kReplacementEncodedSize = 2 + 3 + 2;
  const size_t append_candidate = initial_pointer + kInitialEncodedSize;
  EXPECT_TRUE(std::all_of(
      rom.vector().begin() + append_candidate,
      rom.vector().begin() + append_candidate + kReplacementEncodedSize,
      [](uint8_t value) { return value == 0; }));
}

TEST(TrackCollisionGeneratorTest,
     WriteAppendsWhenReplacementExceedsCurrentBlob) {
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kCustomCollisionDataEnd, 0x00))
          .ok());
  ASSERT_TRUE(
      WriteTrackCollision(&rom, 0x25, MakeCollisionMap({{0, 0xB0}})).ok());
  const uint32_t initial_pointer = ReadCollisionPointerPc(rom, 0x25);

  const CustomCollisionMap replacement =
      MakeCollisionMap({{0, 0xB0}, {1, 0xB1}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, replacement).ok());

  constexpr uint32_t kInitialEncodedSize = 2 + 3 + 2;
  EXPECT_EQ(ReadCollisionPointerPc(rom, 0x25),
            initial_pointer + kInitialEncodedSize);
  auto loaded_or = LoadCustomCollisionMap(&rom, 0x25);
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status();
  EXPECT_EQ(loaded_or->tiles, replacement.tiles);
}

TEST(TrackCollisionGeneratorTest, WriteDoesNotReuseBlobSharedByAnotherRoom) {
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kCustomCollisionDataEnd, 0x00))
          .ok());
  const CustomCollisionMap shared = MakeCollisionMap({{0, 0xB0}, {1, 0xB1}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, shared).ok());
  const uint32_t shared_pointer = ReadCollisionPointerPc(rom, 0x25);
  CopyCollisionPointer(&rom, 0x25, 0x26);

  const CustomCollisionMap replacement = MakeCollisionMap({{65, 0xB7}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, replacement).ok());

  EXPECT_NE(ReadCollisionPointerPc(rom, 0x25), shared_pointer);
  EXPECT_EQ(ReadCollisionPointerPc(rom, 0x26), shared_pointer);
  auto shared_loaded_or = LoadCustomCollisionMap(&rom, 0x26);
  ASSERT_TRUE(shared_loaded_or.ok()) << shared_loaded_or.status();
  EXPECT_EQ(shared_loaded_or->tiles, shared.tiles);
  auto replacement_loaded_or = LoadCustomCollisionMap(&rom, 0x25);
  ASSERT_TRUE(replacement_loaded_or.ok()) << replacement_loaded_or.status();
  EXPECT_EQ(replacement_loaded_or->tiles, replacement.tiles);
}

TEST(TrackCollisionGeneratorTest,
     WriteDoesNotReuseBlobContainingAnotherRoomPointer) {
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kCustomCollisionDataEnd, 0x00))
          .ok());

  // Room 0x25 uses a rectangle whose four payload bytes also form a valid
  // empty single-tile blob for room 0x26. Both streams decode, but their
  // physical spans overlap and therefore cannot be edited in place safely.
  const uint32_t outer_start = kCustomCollisionDataPosition;
  ASSERT_TRUE(rom.WriteVector(outer_start, {0x00, 0x00, 0x04, 0x01, 0xF0, 0xF0,
                                            0xFF, 0xFF, 0xFF, 0xFF})
                  .ok());
  SetCollisionPointer(&rom, 0x25, outer_start);
  SetCollisionPointer(&rom, 0x26, outer_start + 4);

  const CustomCollisionMap replacement = MakeCollisionMap({{65, 0xB7}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, replacement).ok());

  constexpr uint32_t kOuterEncodedSize = 10;
  EXPECT_EQ(ReadCollisionPointerPc(rom, 0x25), outer_start + kOuterEncodedSize);
  EXPECT_EQ(ReadCollisionPointerPc(rom, 0x26), outer_start + 4);
  auto interior_loaded_or = LoadCustomCollisionMap(&rom, 0x26);
  ASSERT_TRUE(interior_loaded_or.ok()) << interior_loaded_or.status();
  EXPECT_TRUE(std::all_of(interior_loaded_or->tiles.begin(),
                          interior_loaded_or->tiles.end(),
                          [](uint8_t value) { return value == 0; }));
  auto replacement_loaded_or = LoadCustomCollisionMap(&rom, 0x25);
  ASSERT_TRUE(replacement_loaded_or.ok()) << replacement_loaded_or.status();
  EXPECT_EQ(replacement_loaded_or->tiles, replacement.tiles);
}

TEST(TrackCollisionGeneratorTest, ExplicitFloorAndClearRoundTrip) {
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kCustomCollisionDataEnd, 0x00))
          .ok());
  CustomCollisionMap map;
  map.SetOverride(494, 0x00);
  map.SetOverride(495, 0xFF);
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x83, map).ok());
  auto loaded = LoadCustomCollisionMap(&rom, 0x83);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(*loaded, map);
  EXPECT_TRUE(loaded->HasOverride(494));
  EXPECT_FALSE(loaded->HasOverride(493));

  map.ClearOverride(494);
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x83, map).ok());
  loaded = LoadCustomCollisionMap(&rom, 0x83);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(*loaded, map);
  EXPECT_FALSE(loaded->HasOverride(494));
  EXPECT_TRUE(loaded->HasOverride(495));
  EXPECT_EQ(loaded->tiles[495], 0xFF);
}

}  // namespace
}  // namespace yaze::zelda3
