#include "zelda3/dungeon/door_position.h"

#include <array>
#include <utility>

#include "gtest/gtest.h"

namespace yaze::zelda3 {
namespace {

TEST(DoorPositionManagerTest, PositionToTileCoordsMatchesUsdasmTables) {
  const std::array<std::pair<int, int>, 12> kNorthExpected = {
      std::pair{14, 0},  std::pair{30, 0},  std::pair{46, 0},
      std::pair{14, 3},  std::pair{30, 3},  std::pair{46, 3},
      std::pair{14, 32}, std::pair{30, 32}, std::pair{46, 32},
      std::pair{14, 35}, std::pair{30, 35}, std::pair{46, 35}};
  const std::array<std::pair<int, int>, 12> kSouthExpected = {
      std::pair{14, 22}, std::pair{30, 22}, std::pair{46, 22},
      std::pair{14, 19}, std::pair{30, 19}, std::pair{46, 19},
      std::pair{14, 54}, std::pair{30, 54}, std::pair{46, 54},
      std::pair{14, 51}, std::pair{30, 51}, std::pair{46, 51}};
  const std::array<std::pair<int, int>, 12> kWestExpected = {
      std::pair{2, 11},  std::pair{2, 27},  std::pair{2, 43},
      std::pair{5, 11},  std::pair{5, 27},  std::pair{5, 43},
      std::pair{34, 11}, std::pair{34, 27}, std::pair{34, 43},
      std::pair{37, 11}, std::pair{37, 27}, std::pair{37, 43}};
  const std::array<std::pair<int, int>, 12> kEastExpected = {
      std::pair{26, 11}, std::pair{26, 27}, std::pair{26, 43},
      std::pair{23, 11}, std::pair{23, 27}, std::pair{23, 43},
      std::pair{58, 11}, std::pair{58, 27}, std::pair{58, 43},
      std::pair{55, 11}, std::pair{55, 27}, std::pair{55, 43}};

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
            (std::vector<int>{11, 27, 43}));
  EXPECT_EQ(DoorPositionManager::GetSnapPositions(DoorDirection::East),
            (std::vector<int>{11, 27, 43}));
}

TEST(DoorPositionManagerTest, SnapToNearestPositionUsesCorrectVerticalRows) {
  const int west_x = 2 * DoorPositionManager::kTileSize;
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          west_x, 11 * DoorPositionManager::kTileSize, DoorDirection::West),
      0);
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          west_x, 27 * DoorPositionManager::kTileSize, DoorDirection::West),
      1);
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          west_x, 43 * DoorPositionManager::kTileSize, DoorDirection::West),
      2);

  const int east_x = 61 * DoorPositionManager::kTileSize;
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          east_x, 11 * DoorPositionManager::kTileSize, DoorDirection::East),
      6);
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          east_x, 27 * DoorPositionManager::kTileSize, DoorDirection::East),
      7);
  EXPECT_EQ(
      DoorPositionManager::SnapToNearestPosition(
          east_x, 43 * DoorPositionManager::kTileSize, DoorDirection::East),
      8);
}

}  // namespace
}  // namespace yaze::zelda3
