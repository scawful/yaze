#include "zelda3/dungeon/door_position.h"

#include <array>
#include <utility>

#include "gtest/gtest.h"

namespace yaze::zelda3 {
namespace {

TEST(DoorPositionManagerTest, PositionToTileCoordsMatchesUsdasmTables) {
  const std::array<std::pair<int, int>, 12> kNorthExpected = {
      std::pair{14, 4},  std::pair{30, 4},  std::pair{46, 4},
      std::pair{14, 7},  std::pair{30, 7},  std::pair{46, 7},
      std::pair{14, 36}, std::pair{30, 36}, std::pair{46, 36},
      std::pair{14, 39}, std::pair{30, 39}, std::pair{46, 39}};
  const std::array<std::pair<int, int>, 12> kSouthExpected = {
      std::pair{14, 26}, std::pair{30, 26}, std::pair{46, 26},
      std::pair{14, 23}, std::pair{30, 23}, std::pair{46, 23},
      std::pair{14, 58}, std::pair{30, 58}, std::pair{46, 58},
      std::pair{14, 55}, std::pair{30, 55}, std::pair{46, 55}};
  const std::array<std::pair<int, int>, 12> kWestExpected = {
      std::pair{2, 15},  std::pair{2, 31},  std::pair{2, 47},
      std::pair{5, 15},  std::pair{5, 31},  std::pair{5, 47},
      std::pair{34, 15}, std::pair{34, 31}, std::pair{34, 47},
      std::pair{37, 15}, std::pair{37, 31}, std::pair{37, 47}};
  const std::array<std::pair<int, int>, 12> kEastExpected = {
      std::pair{26, 15}, std::pair{26, 31}, std::pair{26, 47},
      std::pair{23, 15}, std::pair{23, 31}, std::pair{23, 47},
      std::pair{58, 15}, std::pair{58, 31}, std::pair{58, 47},
      std::pair{55, 15}, std::pair{55, 31}, std::pair{55, 47}};

  for (uint8_t pos = 0; pos < 12; ++pos) {
    EXPECT_EQ(
        DoorPositionManager::PositionToTileCoords(pos, DoorDirection::North),
        kNorthExpected[pos])
        << "north pos=" << static_cast<int>(pos);
    EXPECT_EQ(
        DoorPositionManager::PositionToTileCoords(pos, DoorDirection::South),
        kSouthExpected[pos])
        << "south pos=" << static_cast<int>(pos);
    EXPECT_EQ(
        DoorPositionManager::PositionToTileCoords(pos, DoorDirection::West),
        kWestExpected[pos])
        << "west pos=" << static_cast<int>(pos);
    EXPECT_EQ(
        DoorPositionManager::PositionToTileCoords(pos, DoorDirection::East),
        kEastExpected[pos])
        << "east pos=" << static_cast<int>(pos);
  }
}

TEST(DoorPositionManagerTest, VerticalSnapPositionsMatchUsdasmRows) {
  EXPECT_EQ(DoorPositionManager::GetSnapPositions(DoorDirection::West),
            (std::vector<int>{15, 31, 47}));
  EXPECT_EQ(DoorPositionManager::GetSnapPositions(DoorDirection::East),
            (std::vector<int>{15, 31, 47}));
}

TEST(DoorPositionManagerTest, SnapToNearestPositionUsesCorrectVerticalRows) {
  const int west_x = 2 * DoorPositionManager::kTileSize;
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          west_x, 15 * DoorPositionManager::kTileSize, DoorDirection::West),
      0);
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          west_x, 31 * DoorPositionManager::kTileSize, DoorDirection::West),
      1);
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          west_x, 47 * DoorPositionManager::kTileSize, DoorDirection::West),
      2);

  const int east_x = 61 * DoorPositionManager::kTileSize;
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          east_x, 15 * DoorPositionManager::kTileSize, DoorDirection::East),
      6);
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          east_x, 31 * DoorPositionManager::kTileSize, DoorDirection::East),
      7);
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          east_x, 47 * DoorPositionManager::kTileSize, DoorDirection::East),
      8);
}

}  // namespace
}  // namespace yaze::zelda3
