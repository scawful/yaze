#include "zelda3/dungeon/dungeon_validator.h"

#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace {

bool HasWarningContaining(const ValidationResult& result,
                          std::string_view needle) {
  for (const auto& warning : result.warnings) {
    if (warning.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

TEST(DungeonValidatorTest, AcceptsBothBgObjectsInOverlayStreams) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom);
  ASSERT_TRUE(
      room.AddObject(RoomObject(/*id=*/0x03, /*x=*/4, /*y=*/4, /*size=*/0,
                                /*layer=*/1))
          .ok());
  ASSERT_TRUE(
      room.AddObject(RoomObject(/*id=*/0x108, /*x=*/8, /*y=*/8, /*size=*/0,
                                /*layer=*/2))
          .ok());

  DungeonValidator validator;
  const auto result = validator.ValidateRoom(room);

  EXPECT_TRUE(result.is_valid);
  EXPECT_TRUE(result.errors.empty());
}

TEST(DungeonValidatorTest, AcceptsMoreThan128ObjectsInOverlayStream) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom);
  for (int i = 0; i < 129; ++i) {
    ASSERT_TRUE(room.AddObject(RoomObject(/*id=*/0x21, /*x=*/i % 64,
                                          /*y=*/(i / 64) % 64,
                                          /*size=*/0, /*layer=*/2))
                    .ok());
  }

  DungeonValidator validator;
  const auto result = validator.ValidateRoom(room);

  EXPECT_TRUE(result.is_valid);
  EXPECT_TRUE(result.errors.empty());
}

TEST(DungeonValidatorTest, RejectsSpecialTableObjectsWithSelectorTwo) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  for (const auto [id, option] :
       {std::pair<int16_t, ObjectOption>{0x150, ObjectOption::Torch},
        std::pair<int16_t, ObjectOption>{0x0E00, ObjectOption::Block}}) {
    Room room(/*room_id=*/0, &rom);
    RoomObject object(id, /*x=*/4, /*y=*/4, /*size=*/0, /*layer=*/2);
    object.set_options(option);
    ASSERT_TRUE(room.AddObject(object).ok());

    const auto result = DungeonValidator().ValidateRoom(room);
    EXPECT_FALSE(result.is_valid);
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("invalid layer selector 2"),
              std::string::npos);
  }
}

// Happy-path: a room with a single ordinary BG1 object at a valid position
// must be reported as valid with no errors or warnings.
TEST(DungeonValidatorTest, AcceptsValidRoomWithNormalObject) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom);
  // Object 0x21 is a plain floor tile (not Both-BGs, not a chest). Layer 0=BG1.
  ASSERT_TRUE(
      room.AddObject(RoomObject(/*id=*/0x21, /*x=*/4, /*y=*/4, /*size=*/0,
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
  EXPECT_TRUE(found_chest_err)
      << "Expected chest overflow error, got: "
      << (result.errors.empty() ? "(none)" : result.errors[0]);
}

TEST(DungeonValidatorTest, AcceptsStatefulChestsBeforeBigKeyLocks) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom);
  for (int i = 0; i < 4; ++i) {
    const int16_t chest_id = i % 2 == 0 ? 0xF99 : 0xFB1;
    ASSERT_TRUE(room.AddObject(RoomObject(chest_id, /*x=*/i * 4, /*y=*/0,
                                          /*size=*/0, /*layer=*/0))
                    .ok());
  }
  for (int i = 0; i < 2; ++i) {
    ASSERT_TRUE(room.AddObject(RoomObject(0xF98, /*x=*/i * 4, /*y=*/8,
                                          /*size=*/0, /*layer=*/2))
                    .ok());
  }

  const auto result = DungeonValidator().ValidateRoom(room);

  EXPECT_TRUE(result.is_valid);
  EXPECT_TRUE(result.errors.empty());
  EXPECT_TRUE(result.warnings.empty());
}

TEST(DungeonValidatorTest, WarnsForEachStatefulChestVariantAfterLock) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  for (const int16_t chest_id : {int16_t{0xF99}, int16_t{0xFB1}}) {
    Room room(/*room_id=*/0, &rom);
    ASSERT_TRUE(room.AddObject(RoomObject(0xF98, /*x=*/0, /*y=*/0,
                                          /*size=*/0, /*layer=*/0))
                    .ok());
    ASSERT_TRUE(room.AddObject(RoomObject(chest_id, /*x=*/8, /*y=*/0,
                                          /*size=*/0, /*layer=*/0))
                    .ok());

    const auto result = DungeonValidator().ValidateRoom(room);

    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_TRUE(HasWarningContaining(result, "appears after big-key lock"));
  }
}

TEST(DungeonValidatorTest, UsesEncodedListOrderForRoomEventWarnings) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room invalid_room(/*room_id=*/0, &rom);
  // The chest is first in the editor vector, but list 1 is encoded after the
  // list-0 lock. A raw vector scan would miss this ordering hazard.
  ASSERT_TRUE(invalid_room
                  .AddObject(RoomObject(0xF99, /*x=*/8, /*y=*/0,
                                        /*size=*/0, /*layer=*/1))
                  .ok());
  ASSERT_TRUE(invalid_room
                  .AddObject(RoomObject(0xF98, /*x=*/0, /*y=*/0,
                                        /*size=*/0, /*layer=*/0))
                  .ok());

  const auto invalid_result = DungeonValidator().ValidateRoom(invalid_room);

  EXPECT_TRUE(invalid_result.is_valid);
  EXPECT_TRUE(invalid_result.errors.empty());
  EXPECT_TRUE(
      HasWarningContaining(invalid_result, "appears after big-key lock"));

  Room valid_room(/*room_id=*/0, &rom);
  // Conversely, raw vector order has the lock first, but the primary-list
  // chest is encoded before the list-2 lock and must remain valid.
  ASSERT_TRUE(valid_room
                  .AddObject(RoomObject(0xF98, /*x=*/0, /*y=*/0,
                                        /*size=*/0, /*layer=*/2))
                  .ok());
  ASSERT_TRUE(valid_room
                  .AddObject(RoomObject(0xF99, /*x=*/8, /*y=*/0,
                                        /*size=*/0, /*layer=*/0))
                  .ok());

  const auto valid_result = DungeonValidator().ValidateRoom(valid_room);

  EXPECT_TRUE(valid_result.is_valid);
  EXPECT_TRUE(valid_result.errors.empty());
  EXPECT_TRUE(valid_result.warnings.empty());
}

TEST(DungeonValidatorTest, FixedOpenChestsDoNotConsumeRoomEventSlots) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom);
  ASSERT_TRUE(room.AddObject(RoomObject(0xF98, /*x=*/0, /*y=*/0,
                                        /*size=*/0, /*layer=*/0))
                  .ok());
  for (int i = 0; i < 8; ++i) {
    const int16_t open_chest_id = i % 2 == 0 ? 0xF9A : 0xFB2;
    ASSERT_TRUE(room.AddObject(RoomObject(open_chest_id, /*x=*/i * 4,
                                          /*y=*/8, /*size=*/0, /*layer=*/0))
                    .ok());
  }

  const auto result = DungeonValidator().ValidateRoom(room);

  EXPECT_TRUE(result.is_valid);
  EXPECT_TRUE(result.errors.empty());
  EXPECT_TRUE(result.warnings.empty());
}

TEST(DungeonValidatorTest, WarnsWhenRoomEventConsumersExceedSixSlots) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom);
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(room.AddObject(RoomObject(0xF99, /*x=*/i * 4, /*y=*/0,
                                          /*size=*/0, /*layer=*/0))
                    .ok());
  }
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(room.AddObject(RoomObject(0xF98, /*x=*/i * 4, /*y=*/8,
                                          /*size=*/0, /*layer=*/0))
                    .ok());
  }

  const auto result = DungeonValidator().ValidateRoom(room);

  EXPECT_TRUE(result.is_valid);
  EXPECT_TRUE(result.errors.empty());
  EXPECT_TRUE(HasWarningContaining(result, "engine supports at most 6"));
  EXPECT_FALSE(HasWarningContaining(result, "appears after big-key lock"));
}

}  // namespace
}  // namespace zelda3
}  // namespace yaze
