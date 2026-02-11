#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "zelda3/dungeon/custom_collision.h"

namespace yaze {
namespace zelda3 {
namespace {

#if defined(YAZE_WITH_JSON)

TEST(CustomCollisionJsonTest, JsonDumpThenLoadRoundTrip) {
  std::vector<CustomCollisionRoomEntry> rooms;
  {
    CustomCollisionRoomEntry r;
    r.room_id = 0x27;
    // Include duplicates; last wins during normalization in dump/load.
    r.tiles.push_back(CustomCollisionTileEntry{10, 0x08});
    r.tiles.push_back(CustomCollisionTileEntry{4095, 0x1B});
    r.tiles.push_back(CustomCollisionTileEntry{10, 0x09});
    rooms.push_back(std::move(r));
  }
  {
    CustomCollisionRoomEntry r;
    r.room_id = 0x25;
    r.tiles.push_back(CustomCollisionTileEntry{0, 0x02});
    r.tiles.push_back(CustomCollisionTileEntry{42, 0x00});  // dropped on dump
    r.tiles.push_back(CustomCollisionTileEntry{1, 0x0E});
    rooms.push_back(std::move(r));
  }

  auto json_or = DumpCustomCollisionRoomsToJsonString(rooms);
  ASSERT_TRUE(json_or.ok()) << json_or.status().message();

  auto loaded_or = LoadCustomCollisionRoomsFromJsonString(*json_or);
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status().message();
  auto loaded = std::move(loaded_or.value());
  ASSERT_EQ(loaded.size(), 2u);

  EXPECT_EQ(loaded[0].room_id, 0x25);
  ASSERT_EQ(loaded[0].tiles.size(), 2u);
  EXPECT_EQ(loaded[0].tiles[0].offset, 0u);
  EXPECT_EQ(loaded[0].tiles[0].value, 0x02u);
  EXPECT_EQ(loaded[0].tiles[1].offset, 1u);
  EXPECT_EQ(loaded[0].tiles[1].value, 0x0Eu);

  EXPECT_EQ(loaded[1].room_id, 0x27);
  ASSERT_EQ(loaded[1].tiles.size(), 2u);
  EXPECT_EQ(loaded[1].tiles[0].offset, 10u);
  EXPECT_EQ(loaded[1].tiles[0].value, 0x09u);
  EXPECT_EQ(loaded[1].tiles[1].offset, 4095u);
  EXPECT_EQ(loaded[1].tiles[1].value, 0x1Bu);
}

TEST(CustomCollisionJsonTest, JsonLoadRejectsDuplicateRoom) {
  constexpr const char* kJson = R"json(
{
  "version": 1,
  "rooms": [
    { "room_id": "0x25", "tiles": [ [ 0, 2 ] ] },
    { "room_id": "0x25", "tiles": [ [ 1, 8 ] ] }
  ]
}
)json";

  auto rooms_or = LoadCustomCollisionRoomsFromJsonString(kJson);
  EXPECT_FALSE(rooms_or.ok());
}

TEST(CustomCollisionJsonTest, JsonLoadRejectsOutOfRangeOffset) {
  constexpr const char* kJson = R"json(
{
  "version": 1,
  "rooms": [
    { "room_id": 37, "tiles": [ [ 4096, 8 ] ] }
  ]
}
)json";

  auto rooms_or = LoadCustomCollisionRoomsFromJsonString(kJson);
  EXPECT_FALSE(rooms_or.ok());
}

#else

TEST(CustomCollisionJsonTest, JsonHelpersReturnUnimplementedWhenJsonDisabled) {
  CustomCollisionRoomEntry r;
  r.room_id = 0x25;
  r.tiles.push_back(CustomCollisionTileEntry{0, 0x02});

  auto dump_or = DumpCustomCollisionRoomsToJsonString({r});
  EXPECT_EQ(dump_or.status().code(), absl::StatusCode::kUnimplemented);

  auto load_or = LoadCustomCollisionRoomsFromJsonString("{}");
  EXPECT_EQ(load_or.status().code(), absl::StatusCode::kUnimplemented);
}

#endif  // YAZE_WITH_JSON

}  // namespace
}  // namespace zelda3
}  // namespace yaze

