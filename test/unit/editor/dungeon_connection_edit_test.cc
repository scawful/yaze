#include "app/editor/dungeon/dungeon_connection_edit.h"

#include <array>
#include <tuple>

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

using Door = zelda3::Room::Door;
using zelda3::DoorDirection;
using zelda3::DoorType;

Door MakeDoor(DoorDirection direction, uint8_t position,
              DoorType type = DoorType::NormalDoor) {
  const auto [b1, b2] =
      zelda3::DoorPositionManager::EncodeDoorBytes(position, type, direction);
  return Door::FromRomBytes(b1, b2);
}

class DungeonConnectionMappingTest
    : public ::testing::TestWithParam<
          std::tuple<DoorDirection, int, DungeonConnectionLayer>> {};

TEST_P(DungeonConnectionMappingTest, UsesEnginePartnerTableAndPlansBothRooms) {
  const auto [direction, variant, layer] = GetParam();
  const bool negative =
      direction == DoorDirection::North || direction == DoorDirection::West;
  const uint8_t source_position = variant + (negative ? 0 : 6);
  const uint8_t target_position = variant + (negative ? 6 : 0);
  const auto opposite =
      static_cast<DoorDirection>(static_cast<unsigned>(direction) ^ 1);
  constexpr std::array<int, 4> kRoomOffsets{-16, 16, -1, 1};
  const int target_id = 0x55 + kRoomOffsets[static_cast<unsigned>(direction)];
  zelda3::Room source(0x55, nullptr);
  zelda3::Room target(target_id, nullptr);
  source.GetDoors() = {MakeDoor(direction, source_position)};
  source.ClearSaveDirtyState();
  target.ClearSaveDirtyState();
  const auto plan = PlanDungeonDoorConnection(source, target, {0x55, 0, layer});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(plan->changed());
  EXPECT_TRUE(plan->creates_return);
  EXPECT_EQ(plan->target_room_id, target_id);
  EXPECT_EQ(plan->target_door_index, 0);
  ASSERT_EQ(plan->target_after.size(), 1);
  const auto& return_door = plan->target_after[0];
  EXPECT_EQ(return_door.direction, opposite);
  EXPECT_EQ(return_door.position, target_position);
  EXPECT_EQ(return_door.type, layer == DungeonConnectionLayer::kUpper
                                  ? DoorType::NormalDoor
                                  : DoorType::NormalDoorLower);
  EXPECT_EQ(plan->source_after[0].type, return_door.type);
  EXPECT_EQ(return_door.EncodeBytes(),
            std::make_pair(return_door.byte1, return_door.byte2));
  EXPECT_EQ(source.GetDoors()[0].type, DoorType::NormalDoor);
  EXPECT_TRUE(target.GetDoors().empty());
  EXPECT_FALSE(source.object_stream_dirty());
  EXPECT_FALSE(target.object_stream_dirty());
}

INSTANTIATE_TEST_SUITE_P(
    AllOuterSlots, DungeonConnectionMappingTest,
    ::testing::Combine(::testing::Values(DoorDirection::North,
                                         DoorDirection::South,
                                         DoorDirection::West,
                                         DoorDirection::East),
                       ::testing::Range(0, 6),
                       ::testing::Values(DungeonConnectionLayer::kUpper,
                                         DungeonConnectionLayer::kLower)));

class DungeonConnectionEditTest : public ::testing::Test {
 protected:
  void SetUp() override {
    source_.GetDoors() = {MakeDoor(DoorDirection::East, 7)};
  }
  auto Plan(DungeonConnectionLayer layer = DungeonConnectionLayer::kUpper) {
    return PlanDungeonDoorConnection(source_, target_, {0x55, 0, layer});
  }
  zelda3::Room source_{0x55, nullptr};
  zelda3::Room target_{0x56, nullptr};
};

TEST_F(DungeonConnectionEditTest, ExistingPairWithSameLayerIsExactNoOp) {
  target_.GetDoors() = {MakeDoor(DoorDirection::West, 1)};
  // Preserve raw auxiliary bits and unmodified bytes as part of history.
  source_.GetDoors()[0].byte1 |= 0x04;
  target_.GetDoors()[0].byte1 |= 0x08;
  const auto plan = Plan();
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_FALSE(plan->changed());
  EXPECT_FALSE(plan->creates_return);
  EXPECT_TRUE(SameDungeonDoors(plan->source_before, plan->source_after));
  EXPECT_TRUE(SameDungeonDoors(plan->target_before, plan->target_after));
}

TEST_F(DungeonConnectionEditTest, UpdatesExistingPairTogetherWithoutMovingIt) {
  target_.GetDoors() = {MakeDoor(DoorDirection::South, 6),
                        MakeDoor(DoorDirection::West, 1)};
  const auto plan = Plan(DungeonConnectionLayer::kLower);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(plan->changed());
  EXPECT_FALSE(plan->creates_return);
  EXPECT_EQ(plan->target_door_index, 1);
  EXPECT_EQ(plan->source_after[0].type, DoorType::NormalDoorLower);
  EXPECT_EQ(plan->target_after[1].type, DoorType::NormalDoorLower);
  EXPECT_EQ(plan->source_after[0].position, 7);
  EXPECT_EQ(plan->target_after[1].position, 1);
  EXPECT_EQ(plan->target_after[0].type, DoorType::NormalDoor);
  EXPECT_EQ(plan->target_after[1].byte2, 2);
  EXPECT_EQ(source_.GetDoors()[0].type, DoorType::NormalDoor);
  EXPECT_EQ(target_.GetDoors()[1].type, DoorType::NormalDoor);
}

TEST_F(DungeonConnectionEditTest, MatchesExactLaneRatherThanFirstOpposingDoor) {
  target_.GetDoors() = {MakeDoor(DoorDirection::West, 0),
                        MakeDoor(DoorDirection::West, 2)};
  const auto plan = Plan();
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(plan->creates_return);
  EXPECT_EQ(plan->target_door_index, 2);
  EXPECT_EQ(plan->target_after[2].position, 1);
}

TEST_F(DungeonConnectionEditTest, RejectsDuplicateTargetPassages) {
  target_.GetDoors() = {MakeDoor(DoorDirection::West, 1),
                        MakeDoor(DoorDirection::West, 1)};
  EXPECT_FALSE(Plan().ok());
  EXPECT_EQ(target_.GetDoors().size(), 2);
}

TEST_F(DungeonConnectionEditTest, RejectsAlternateDepthInSameTargetLane) {
  target_.GetDoors() = {MakeDoor(DoorDirection::West, 4)};
  EXPECT_FALSE(Plan().ok());
}

TEST_F(DungeonConnectionEditTest, RejectsSourceMarkerInEitherWallDepth) {
  for (auto type : {DoorType::ExitMarker, DoorType::DungeonSwapMarker,
                    DoorType::LayerSwapMarker}) {
    for (uint8_t position : {7, 10}) {
      source_.GetDoors().resize(1);
      source_.GetDoors().push_back(
          MakeDoor(DoorDirection::East, position, type));
      EXPECT_FALSE(Plan().ok());
    }
  }
}

TEST_F(DungeonConnectionEditTest,
       VanillaRoom55ExitMarkerBlocksOnlyItsSouthPassage) {
  // Vanilla room 055 has a normal south slot-6 door plus a slot-6 exit marker.
  // USDASM $01BF2A..$01BF3E changes that doorway to collision 8E; the engine
  // leaves for the overworld at $02B7A7..$02B7AE instead of entering room 065.
  source_.GetDoors() = {MakeDoor(DoorDirection::South, 6),
                        MakeDoor(DoorDirection::South, 6, DoorType::ExitMarker),
                        MakeDoor(DoorDirection::East, 7)};
  zelda3::Room south(0x65, nullptr);
  const auto before = source_.GetDoors();
  for (const auto layer :
       {DungeonConnectionLayer::kUpper, DungeonConnectionLayer::kLower}) {
    const auto exit =
        PlanDungeonDoorConnection(source_, south, {0x55, 0, layer});
    ASSERT_FALSE(exit.ok());
    EXPECT_EQ(exit.status().code(), absl::StatusCode::kFailedPrecondition);
    EXPECT_TRUE(south.GetDoors().empty());
    EXPECT_TRUE(SameDungeonDoors(source_.GetDoors(), before));

    const auto internal =
        PlanDungeonDoorConnection(source_, target_, {0x55, 2, layer});
    ASSERT_TRUE(internal.ok()) << internal.status();
    EXPECT_EQ(internal->target_room_id, 0x56);
    EXPECT_TRUE(internal->creates_return);
  }
}

TEST_F(DungeonConnectionEditTest, RejectsTargetMarkerInEitherWallDepth) {
  for (auto type : {DoorType::ExitMarker, DoorType::DungeonSwapMarker,
                    DoorType::LayerSwapMarker}) {
    for (uint8_t position : {1, 4}) {
      target_.GetDoors() = {MakeDoor(DoorDirection::West, position, type)};
      EXPECT_FALSE(Plan().ok());
    }
  }
}

TEST_F(DungeonConnectionEditTest, RejectsEveryNonOrdinarySourceType) {
  for (int type = 0; type <= 255; ++type) {
    if (type == 0 || type == 2) {
      continue;
    }
    source_.GetDoors()[0].type = static_cast<DoorType>(type);
    EXPECT_FALSE(Plan().ok()) << type;
  }
}

TEST_F(DungeonConnectionEditTest, RejectsEveryNonOrdinaryReturnType) {
  for (int type = 0; type <= 255; ++type) {
    if (type == 0 || type == 2) {
      continue;
    }
    target_.GetDoors() = {
        MakeDoor(DoorDirection::West, 1, static_cast<DoorType>(type))};
    EXPECT_FALSE(Plan().ok()) << type;
  }
}

TEST_F(DungeonConnectionEditTest, RejectsInternalSeamsInAllDirections) {
  for (const auto direction : {DoorDirection::North, DoorDirection::South,
                               DoorDirection::West, DoorDirection::East}) {
    const bool negative =
        direction == DoorDirection::North || direction == DoorDirection::West;
    for (uint8_t offset = 0; offset < 6; ++offset) {
      EXPECT_FALSE(DungeonConnectionTargetRoom(
                       0x55, MakeDoor(direction, offset + (negative ? 6 : 0)))
                       .ok());
    }
  }
}

TEST_F(DungeonConnectionEditTest, RejectsRoomTableEdgesAndPageCrossings) {
  const std::array<std::tuple<int, DoorDirection, uint8_t>, 12> invalid{{
      {-1, DoorDirection::East, 6},
      {zelda3::kNumberOfRooms, DoorDirection::West, 0},
      {0x00, DoorDirection::North, 0},
      {0x10, DoorDirection::West, 0},
      {0x0F, DoorDirection::East, 6},
      {0xF0, DoorDirection::South, 6},
      {0xFF, DoorDirection::South, 6},
      {0x100, DoorDirection::North, 0},
      {0x10F, DoorDirection::East, 6},
      {0x110, DoorDirection::West, 0},
      {zelda3::kNumberOfRooms - 1, DoorDirection::South, 6},
      {zelda3::kNumberOfRooms - 1, DoorDirection::East, 6},
  }};
  for (const auto& [room_id, direction, position] : invalid) {
    EXPECT_FALSE(
        DungeonConnectionTargetRoom(room_id, MakeDoor(direction, position))
            .ok())
        << room_id;
  }
  EXPECT_EQ(
      *DungeonConnectionTargetRoom(0x110, MakeDoor(DoorDirection::North, 0)),
      0x100);
  EXPECT_EQ(
      *DungeonConnectionTargetRoom(0x100, MakeDoor(DoorDirection::South, 6)),
      0x110);
}

TEST_F(DungeonConnectionEditTest, RejectsInvalidDirectionAndPosition) {
  auto door = MakeDoor(DoorDirection::East, 6);
  door.direction = static_cast<DoorDirection>(4);
  EXPECT_FALSE(DungeonConnectionTargetRoom(0x55, door).ok());
  door.direction = DoorDirection::East;
  door.position = 12;
  EXPECT_FALSE(DungeonConnectionTargetRoom(0x55, door).ok());
  door.position = 255;
  EXPECT_FALSE(DungeonConnectionTargetRoom(0x55, door).ok());
}

TEST_F(DungeonConnectionEditTest, RejectsStaleSourceIdentityOrIndex) {
  EXPECT_FALSE(PlanDungeonDoorConnection(source_, target_, {0x54, 0}).ok());
  EXPECT_FALSE(PlanDungeonDoorConnection(source_, target_, {0x55, 1}).ok());
  EXPECT_FALSE(
      PlanDungeonDoorConnection(
          source_, target_, {0x55, 0, static_cast<DungeonConnectionLayer>(2)})
          .ok());
}

TEST_F(DungeonConnectionEditTest, RejectsWrongRoomOrForeignRom) {
  zelda3::Room wrong_room(0x57, nullptr);
  EXPECT_FALSE(PlanDungeonDoorConnection(source_, wrong_room, {0x55, 0}).ok());
  Rom foreign_rom;
  target_.SetRom(&foreign_rom);
  EXPECT_FALSE(Plan().ok());
  source_.SetRom(&foreign_rom);
  const auto plan = Plan();
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->rom, &foreign_rom);
}

TEST_F(DungeonConnectionEditTest, RejectsGrowthAtDoorCapacity) {
  target_.GetDoors().assign(16, MakeDoor(DoorDirection::South, 6));
  const auto plan = Plan();
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(target_.GetDoors().size(), 16);
  EXPECT_EQ(source_.GetDoors().size(), 1);
}

TEST_F(DungeonConnectionEditTest, CreatesSixteenthDoorAtCapacityBoundary) {
  target_.GetDoors().assign(15, MakeDoor(DoorDirection::South, 6));
  const auto plan = Plan();
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->target_after.size(), 16);
}

TEST_F(DungeonConnectionEditTest,
       NonGrowthEditPreservesOverCapacityLegacyData) {
  target_.GetDoors().assign(17, MakeDoor(DoorDirection::South, 6));
  target_.GetDoors()[0] = MakeDoor(DoorDirection::West, 1);
  source_.GetDoors().resize(18, MakeDoor(DoorDirection::South, 6));
  const auto plan = Plan(DungeonConnectionLayer::kLower);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->target_after.size(), 17);
  EXPECT_EQ(plan->source_after.size(), 18);
}

TEST_F(DungeonConnectionEditTest, PreservesUnrelatedUnknownAndInternalRecords) {
  auto unknown = MakeDoor(DoorDirection::South, 3);
  unknown.position = 0xFF;
  unknown.type = static_cast<DoorType>(0xEF);
  source_.GetDoors().push_back(unknown);
  source_.GetDoors().push_back(
      MakeDoor(DoorDirection::East, 1, DoorType::LayerSwapMarker));
  target_.GetDoors() = {
      unknown, MakeDoor(DoorDirection::West, 7, DoorType::DungeonSwapMarker)};
  const auto plan = Plan();
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(SameDungeonDoors(plan->source_before, plan->source_after));
  auto remaining = plan->target_after;
  remaining.pop_back();
  EXPECT_TRUE(SameDungeonDoors(target_.GetDoors(), remaining));
}

TEST_F(DungeonConnectionEditTest, DoorEqualityIncludesRawBytesAndOrder) {
  const std::vector<Door> doors{MakeDoor(DoorDirection::East, 6),
                                MakeDoor(DoorDirection::South, 8)};
  auto changed = doors;
  changed[0].byte1 ^= 4;
  EXPECT_FALSE(SameDungeonDoors(doors, changed));
  changed = doors;
  changed[0].byte2 ^= 2;
  EXPECT_FALSE(SameDungeonDoors(doors, changed));
  changed = doors;
  std::swap(changed[0], changed[1]);
  EXPECT_FALSE(SameDungeonDoors(doors, changed));
}

}  // namespace
}  // namespace yaze::editor
