#include "zelda3/dungeon/dungeon_validator.h"

#include <vector>

#include <gtest/gtest.h>

#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace {

TEST(DungeonValidatorTest, RejectsBothBgObjectsInBg3Layer) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom);
  ASSERT_TRUE(room.AddObject(
                   RoomObject(/*id=*/0x03, /*x=*/4, /*y=*/4, /*size=*/0,
                              /*layer=*/2))
                  .ok());

  DungeonValidator validator;
  const auto result = validator.ValidateRoom(room);

  EXPECT_FALSE(result.is_valid);
  bool found_error = false;
  for (const auto& e : result.errors) {
    if (e.find("Both-BGs") != std::string::npos) {
      found_error = true;
      break;
    }
  }
  EXPECT_TRUE(found_error);
}

TEST(DungeonValidatorTest, RejectsBg3ObjectOverflow) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom);
  for (int i = 0; i < 129; ++i) {
    ASSERT_TRUE(room.AddObject(
                     RoomObject(/*id=*/0x21, /*x=*/i % 64, /*y=*/(i / 64) % 64,
                                /*size=*/0, /*layer=*/2))
                    .ok());
  }

  DungeonValidator validator;
  const auto result = validator.ValidateRoom(room);

  EXPECT_FALSE(result.is_valid);
  bool found_error = false;
  for (const auto& e : result.errors) {
    if (e.find("Too many BG3 objects") != std::string::npos) {
      found_error = true;
      break;
    }
  }
  EXPECT_TRUE(found_error);
}

// Happy-path: a room with a single ordinary BG1 object at a valid position
// must be reported as valid with no errors or warnings.
TEST(DungeonValidatorTest, AcceptsValidRoomWithNormalObject) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom);
  // Object 0x21 is a plain floor tile (not Both-BGs, not a chest). Layer 0=BG1.
  ASSERT_TRUE(room.AddObject(
                   RoomObject(/*id=*/0x21, /*x=*/4, /*y=*/4, /*size=*/0,
                              /*layer=*/0))
                  .ok());

  DungeonValidator validator;
  const auto result = validator.ValidateRoom(room);

  EXPECT_TRUE(result.is_valid);
  EXPECT_TRUE(result.errors.empty());
}

// Out-of-bounds gate: Room::AddObject enforces coordinate bounds before the
// object reaches DungeonValidator. This test documents that the enforcement
// lives at the AddObject layer (x=64 rejected at add-time), so objects from
// that API never reach the validator's bounds check at dungeon_validator.cc:72.
// (The validator check applies to ROM-parsed objects that bypass AddObject.)
TEST(DungeonValidatorTest, RoomAddObjectRejectsOutOfBoundsCoordinates) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom);
  RoomObject obj(/*id=*/0x21, /*x=*/4, /*y=*/4, /*size=*/0, /*layer=*/0);
  obj.set_x(64);  // x=64 exceeds the 64-tile grid width

  // AddObject validates bounds and must reject this object.
  const auto status = room.AddObject(obj);
  EXPECT_FALSE(status.ok())
      << "Expected AddObject to reject x=64 as out-of-bounds";

  // Room must remain empty — no objects with invalid coords were accepted.
  EXPECT_TRUE(room.GetTileObjects().empty())
      << "Room should have no objects after an out-of-bounds AddObject";
}

// Too many chests: 7 objects in ID range 0xF9-0xFD exceeds kMaxChests=6.
TEST(DungeonValidatorTest, RejectsTooManyChestObjects) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom);
  // Add kMaxChests+1 = 7 chest objects (IDs 0xF9–0xFD cycle).
  for (int i = 0; i < 7; ++i) {
    const int16_t chest_id = static_cast<int16_t>(0xF9 + (i % 5));
    ASSERT_TRUE(
        room.AddObject(RoomObject(chest_id, /*x=*/i * 4, /*y=*/0, /*size=*/0,
                                  /*layer=*/0))
            .ok());
  }

  DungeonValidator validator;
  const auto result = validator.ValidateRoom(room);

  EXPECT_FALSE(result.is_valid);
  bool found_chest_err = false;
  for (const auto& err : result.errors) {
    if (err.find("Too many chests") != std::string::npos) {
      found_chest_err = true;
      break;
    }
  }
  EXPECT_TRUE(found_chest_err) << "Expected chest overflow error, got: "
                                << (result.errors.empty() ? "(none)"
                                                          : result.errors[0]);
}

}  // namespace
}  // namespace zelda3
}  // namespace yaze
