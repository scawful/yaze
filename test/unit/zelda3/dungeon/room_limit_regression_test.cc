// Room-level regression tests for dungeon limit behavior.
//
// Verifies Room::HasExceededLimits() and Room::GetExceededLimitDetails()
// correctness with canonical limits from dungeon_limits.h.

#include "zelda3/dungeon/dungeon_limits.h"
#include "zelda3/dungeon/room.h"

#include <gtest/gtest.h>

namespace yaze::zelda3 {
namespace {

// Helper: add N sprites to a room.
void AddSprites(Room& room, int count) {
  for (int i = 0; i < count; ++i) {
    room.GetSprites().emplace_back(
        static_cast<uint8_t>(i % 256),
        static_cast<uint8_t>(i % 32),
        static_cast<uint8_t>(i % 32), 0, 0);
  }
}

// Helper: add N tile objects to a room.
void AddTileObjects(Room& room, int count) {
  for (int i = 0; i < count; ++i) {
    room.AddTileObject(RoomObject(0x01, i % 64, (i / 64) % 64, 0, 0));
  }
}

// Helper: add N doors to a room.
void AddDoors(Room& room, int count) {
  for (int i = 0; i < count; ++i) {
    Room::Door door;
    door.position = static_cast<uint8_t>(i % 32);
    door.type = DoorType::NormalDoor;
    door.direction = DoorDirection::North;
    room.AddDoor(door);
  }
}

// ============================================================================
// HasExceededLimits — non-hard-limit categories
// ============================================================================

TEST(RoomLimitRegressionTest, AtHardLimitsDoesNotExceed) {
  // At exact hard limits, the room should not report an exceeded state.
  // This guards against off-by-one regressions in the room-level helpers.
  Room room;
  AddSprites(room, static_cast<int>(kMaxTotalSprites));  // At limit, not over
  AddTileObjects(room, static_cast<int>(kMaxTileObjects));  // At limit, not over

  EXPECT_FALSE(room.HasExceededLimits());
  EXPECT_TRUE(room.GetExceededLimitDetails().empty());
}

TEST(RoomLimitRegressionTest, OneOverLimitTriggersExceeded) {
  Room room;
  AddSprites(room, static_cast<int>(kMaxTotalSprites) + 1);

  EXPECT_TRUE(room.HasExceededLimits());
  auto details = room.GetExceededLimitDetails();
  ASSERT_EQ(details.size(), 1u);
  EXPECT_EQ(details[0].limit, DungeonLimit::kSprites);
  EXPECT_EQ(details[0].current, static_cast<int>(kMaxTotalSprites) + 1);
  EXPECT_EQ(details[0].maximum, static_cast<int>(kMaxTotalSprites));
}

// ============================================================================
// GetExceededLimitDetails — label stability
// ============================================================================

TEST(RoomLimitRegressionTest, ExceededDetailsLabelsAreStable) {
  Room room;
  // Use sprites and doors which are counted directly by GetLimitedObjectCounts.
  // (Tile objects use subtype traversal and may not match raw AddTileObject count.)
  AddSprites(room, static_cast<int>(kMaxTotalSprites) + 1);
  AddDoors(room, static_cast<int>(kMaxDoors) + 1);

  auto details = room.GetExceededLimitDetails();
  ASSERT_GE(details.size(), 2u);

  // Verify labels match GetDungeonLimitLabel() for each entry.
  for (const auto& info : details) {
    EXPECT_STREQ(info.label, GetDungeonLimitLabel(info.limit));
    EXPECT_GT(info.current, info.maximum);
  }
}

TEST(RoomLimitRegressionTest, EmptyRoomHasNoExceededLimits) {
  Room room;
  EXPECT_FALSE(room.HasExceededLimits());
  EXPECT_TRUE(room.GetExceededLimitDetails().empty());
}

TEST(RoomLimitRegressionTest, ExactlyAtLimitDoesNotExceed) {
  Room room;
  AddDoors(room, static_cast<int>(kMaxDoors));

  EXPECT_FALSE(room.HasExceededLimits());
  EXPECT_TRUE(room.GetExceededLimitDetails().empty());
}

// ============================================================================
// GetLimitedObjectCounts — hard-limit categories counted
// ============================================================================

TEST(RoomLimitRegressionTest, LimitedCountsIncludeSpriteAndDoorCategories) {
  Room room;
  AddSprites(room, 3);
  AddTileObjects(room, 5);
  AddDoors(room, 2);

  auto counts = room.GetLimitedObjectCounts();
  EXPECT_EQ(counts[DungeonLimit::kSprites], 3);
  EXPECT_EQ(counts[DungeonLimit::kDoors], 2);
  // Tile objects are not counted as a raw vector-size hard limit in
  // GetLimitedObjectCounts() today (room.cc uses subtype traversal).
  // This assertion documents current behavior to prevent accidental drift.
  EXPECT_EQ(counts[DungeonLimit::kTileObjects], 0);
}

}  // namespace
}  // namespace yaze::zelda3
