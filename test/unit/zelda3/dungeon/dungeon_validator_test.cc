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

}  // namespace
}  // namespace zelda3
}  // namespace yaze
