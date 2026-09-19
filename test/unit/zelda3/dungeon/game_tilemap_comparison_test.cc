#include "zelda3/dungeon/game_tilemap_comparison.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {
namespace {

RoomTilemaps Filled(uint16_t bg1, uint16_t bg2) {
  RoomTilemaps maps;
  maps.bg1.assign(kRoomTilemapWords, bg1);
  maps.bg2.assign(kRoomTilemapWords, bg2);
  return maps;
}

TileOwners NoOwners() {
  TileOwners owners;
  owners.bg1.assign(kRoomTilemapWords, -1);
  owners.bg2.assign(kRoomTilemapWords, -1);
  return owners;
}

size_t At(int x, int y) {
  return static_cast<size_t>(y) * 64 + x;
}

TEST(GameTilemapComparisonTest, ParseReadsBg1ThenBg2LittleEndian) {
  std::vector<uint8_t> bytes(kGameRoomTilemapBytes, 0);
  bytes[0] = 0x34;  // BG1 (0,0) = 0x1234
  bytes[1] = 0x12;
  bytes[2 * (64 + 1)] = 0xCD;  // BG1 (1,1) = 0xABCD
  bytes[2 * (64 + 1) + 1] = 0xAB;
  bytes[kRoomTilemapWords * 2] = 0x01;  // BG2 (0,0) = 0x2001
  bytes[kRoomTilemapWords * 2 + 1] = 0x20;

  auto maps = ParseGameRoomTilemaps(bytes);
  ASSERT_TRUE(maps.ok()) << maps.status();
  EXPECT_EQ(maps->bg1[At(0, 0)], 0x1234);
  EXPECT_EQ(maps->bg1[At(1, 1)], 0xABCD);
  EXPECT_EQ(maps->bg2[At(0, 0)], 0x2001);
  EXPECT_EQ(maps->bg2[At(1, 1)], 0x0000);
}

TEST(GameTilemapComparisonTest, ParseRejectsWrongSize) {
  EXPECT_FALSE(ParseGameRoomTilemaps({}).ok());
  EXPECT_FALSE(
      ParseGameRoomTilemaps(std::vector<uint8_t>(kGameRoomTilemapBytes - 2))
          .ok());
}

TEST(GameTilemapComparisonTest, IdenticalMapsHaveNoDifferences) {
  const RoomTilemaps maps = Filled(0x0100, 0x0200);
  const auto check = CompareRoomTilemaps(0x001, maps, maps, NoOwners(), {});
  EXPECT_EQ(check.bg1_matching, static_cast<int>(kRoomTilemapWords));
  EXPECT_EQ(check.bg2_matching, static_cast<int>(kRoomTilemapWords));
  EXPECT_TRUE(check.differences.empty());
  EXPECT_EQ(check.unowned_differences, 0);
}

// A difference on a tile an object owns is charged to that placement; one on
// an unowned tile (layout, doors) is not.
TEST(GameTilemapComparisonTest, DifferencesAreChargedToTheOwningPlacement) {
  const std::vector<RoomObject> objects = {RoomObject(0x04C, 0, 0, 0, 0),
                                           RoomObject(0x001, 0, 0, 0, 0)};
  RoomTilemaps game = Filled(0x0100, 0x0200);
  RoomTilemaps yaze = game;
  TileOwners owners = NoOwners();
  owners.bg1[At(2, 3)] = 0;
  owners.bg1[At(3, 3)] = 0;
  owners.bg2[At(10, 10)] = 1;
  game.bg1[At(2, 3)] = 0x2100;    // priority differs, owned by object 0
  game.bg2[At(20, 20)] = 0x0201;  // tile differs, unowned

  const auto check = CompareRoomTilemaps(0x042, game, yaze, owners, objects);
  EXPECT_EQ(check.room_id, 0x042);
  ASSERT_EQ(check.differences.size(), 2u);
  EXPECT_EQ(check.differences[0].layer, 1);
  EXPECT_EQ(check.differences[0].x, 2);
  EXPECT_EQ(check.differences[0].y, 3);
  EXPECT_EQ(check.differences[0].owner, 0);
  EXPECT_EQ(check.differences[1].layer, 2);
  EXPECT_EQ(check.differences[1].owner, -1);
  EXPECT_EQ(check.unowned_differences, 1);

  ASSERT_EQ(check.placements.size(), 2u);
  EXPECT_EQ(check.placements[0].object_id, 0x04C);
  EXPECT_EQ(check.placements[0].tiles_owned, 2);
  EXPECT_EQ(check.placements[0].tiles_different, 1);
  EXPECT_EQ(check.placements[0].difference_bits, 0x2000);
  EXPECT_EQ(check.placements[1].object_id, 0x001);
  EXPECT_EQ(check.placements[1].tiles_owned, 1);
  EXPECT_EQ(check.placements[1].tiles_different, 0);
}

// RoomDraw_Chest skips closed chests in rooms whose tag1 or tag2 is 0x27,
// 0x3C, 0x3E or 0x29-0x32, so a capture of the room on load cannot judge them.
TEST(GameTilemapComparisonTest, HiddenChestTagsMatchRoomDrawChest) {
  const RoomObject chest(0xF99, 4, 4, 0, 0);
  for (int tag : {0x27, 0x29, 0x32, 0x3C, 0x3E}) {
    EXPECT_TRUE(GameHidesObjectOnRoomLoad(tag, 0, chest)) << tag;
    EXPECT_TRUE(GameHidesObjectOnRoomLoad(0, tag, chest)) << tag;
  }
  for (int tag : {0x00, 0x28, 0x33, 0x3D, 0x3F}) {
    EXPECT_FALSE(GameHidesObjectOnRoomLoad(tag, tag, chest)) << tag;
  }
  EXPECT_FALSE(
      GameHidesObjectOnRoomLoad(0x27, 0, RoomObject(0xF9A, 4, 4, 0, 0)));
}

TEST(GameTilemapComparisonTest, DescribesEachTileWordField) {
  EXPECT_EQ(DescribeTileWordDifference(0x0000), "none");
  EXPECT_EQ(DescribeTileWordDifference(0x2000), "priority");
  EXPECT_EQ(DescribeTileWordDifference(0x0001), "tile");
  EXPECT_EQ(DescribeTileWordDifference(0x0400), "palette");
  EXPECT_EQ(DescribeTileWordDifference(0x4000), "flip");
  EXPECT_EQ(DescribeTileWordDifference(0x8000), "flip");
  EXPECT_EQ(DescribeTileWordDifference(0x3401), "tile, palette, priority");
}

}  // namespace
}  // namespace yaze::zelda3
