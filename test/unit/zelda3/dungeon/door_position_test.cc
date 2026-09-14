#include "zelda3/dungeon/door_position.h"

#include <algorithm>
#include <array>
#include <iomanip>
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

TEST(DoorPositionManagerTest, SouthRenderBoundsStartBelowUsdasmAnchor) {
  // The ROM table anchor is load-bearing for encoding/door routing, but
  // RoomDraw_OneSidedShutters_South writes through row pointers y+1..y+3.
  EXPECT_EQ(DoorPositionManager::PositionToTileCoords(6, DoorDirection::South),
            (std::pair<int, int>{14, 58}));
  EXPECT_EQ(
      DoorPositionManager::PositionToRenderTileCoords(6, DoorDirection::South),
      (std::pair<int, int>{14, 59}));

  const auto [x, y, width, height] = DoorPositionManager::GetDoorEditorBounds(
      /*position=*/6, DoorDirection::South, DoorType::NormalDoor);
  EXPECT_EQ(x, 14 * DoorPositionManager::kTileSize);
  EXPECT_EQ(y, 59 * DoorPositionManager::kTileSize);
  EXPECT_EQ(width, 4 * DoorPositionManager::kTileSize);
  EXPECT_EQ(height, 3 * DoorPositionManager::kTileSize);
}

TEST(DoorPositionManagerTest, SouthExitEditorBoundsMatchUsdasmFootprints) {
  struct ExpectedBounds {
    DoorType type;
    int tile_x;
    int tile_y;
    int width_tiles;
    int height_tiles;
  };

  constexpr std::array<ExpectedBounds, 5> kCases = {{
      {DoorType::ExitLower, 46, 58, 4, 4},
      {DoorType::FancyDungeonExit, 43, 54, 10, 8},
      {DoorType::FancyDungeonExitLower, 43, 54, 10, 8},
      {DoorType::CaveExit, 46, 58, 4, 4},
      {DoorType::LitCaveExitLower, 46, 58, 4, 4},
  }};

  for (const auto& expected : kCases) {
    const auto [x, y, width, height] = DoorPositionManager::GetDoorEditorBounds(
        /*position=*/8, DoorDirection::South, expected.type);
    EXPECT_EQ(x, expected.tile_x * DoorPositionManager::kTileSize)
        << GetDoorTypeName(expected.type);
    EXPECT_EQ(y, expected.tile_y * DoorPositionManager::kTileSize)
        << GetDoorTypeName(expected.type);
    EXPECT_EQ(width, expected.width_tiles * DoorPositionManager::kTileSize)
        << GetDoorTypeName(expected.type);
    EXPECT_EQ(height, expected.height_tiles * DoorPositionManager::kTileSize)
        << GetDoorTypeName(expected.type);
  }
}

TEST(DoorPositionManagerTest,
     SouthExitOverridesAreDirectionalAndPreserveGenericDoors) {
  EXPECT_EQ(DoorPositionManager::GetDoorEditorBounds(
                /*position=*/8, DoorDirection::South, DoorType::NormalDoor),
            (std::tuple<int, int, int, int>{46 * 8, 59 * 8, 4 * 8, 3 * 8}));
  EXPECT_EQ(DoorPositionManager::GetDoorEditorBounds(
                /*position=*/8, DoorDirection::South, DoorType::BigKeyDoor),
            (std::tuple<int, int, int, int>{46 * 8, 59 * 8, 4 * 8, 3 * 8}));
  EXPECT_EQ(
      DoorPositionManager::GetDoorEditorBounds(
          /*position=*/8, DoorDirection::North, DoorType::FancyDungeonExit),
      (std::tuple<int, int, int, int>{46 * 8, 36 * 8, 4 * 8, 3 * 8}));
}

TEST(DoorPositionManagerTest, EastRenderBoundsStartRightOfUsdasmAnchor) {
  // RoomDraw_OneSidedShutters_East increments the table address by one word
  // before writing, just as the South routine skips its anchor row.
  EXPECT_EQ(DoorPositionManager::PositionToTileCoords(9, DoorDirection::East),
            (std::pair<int, int>{55, 15}));
  EXPECT_EQ(
      DoorPositionManager::PositionToRenderTileCoords(9, DoorDirection::East),
      (std::pair<int, int>{56, 15}));

  const auto [x, y, width, height] = DoorPositionManager::GetDoorEditorBounds(
      /*position=*/9, DoorDirection::East, DoorType::NormalDoor);
  EXPECT_EQ(x, 56 * DoorPositionManager::kTileSize);
  EXPECT_EQ(y, 15 * DoorPositionManager::kTileSize);
  EXPECT_EQ(width, 3 * DoorPositionManager::kTileSize);
  EXPECT_EQ(height, 4 * DoorPositionManager::kTileSize);
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

TEST(DoorPositionManagerTest, ValidPositionsAreLimitedToUsdasmDoorTables) {
  for (uint8_t pos = 0; pos < 12; ++pos) {
    EXPECT_TRUE(DoorPositionManager::IsValidPosition(pos, DoorDirection::North))
        << "north pos=" << static_cast<int>(pos);
    EXPECT_TRUE(DoorPositionManager::IsValidPosition(pos, DoorDirection::South))
        << "south pos=" << static_cast<int>(pos);
    EXPECT_TRUE(DoorPositionManager::IsValidPosition(pos, DoorDirection::West))
        << "west pos=" << static_cast<int>(pos);
    EXPECT_TRUE(DoorPositionManager::IsValidPosition(pos, DoorDirection::East))
        << "east pos=" << static_cast<int>(pos);
  }

  for (uint8_t pos : {uint8_t{12}, uint8_t{15}, uint8_t{31}}) {
    EXPECT_FALSE(
        DoorPositionManager::IsValidPosition(pos, DoorDirection::North))
        << "north pos=" << static_cast<int>(pos);
    EXPECT_FALSE(
        DoorPositionManager::IsValidPosition(pos, DoorDirection::South))
        << "south pos=" << static_cast<int>(pos);
    EXPECT_FALSE(DoorPositionManager::IsValidPosition(pos, DoorDirection::West))
        << "west pos=" << static_cast<int>(pos);
    EXPECT_FALSE(DoorPositionManager::IsValidPosition(pos, DoorDirection::East))
        << "east pos=" << static_cast<int>(pos);
  }
}

TEST(DoorPositionManagerTest, EditorBoundsUseNorthCurtainDoorFootprint) {
  const auto [x, y, width, height] = DoorPositionManager::GetDoorEditorBounds(
      /*position=*/0, DoorDirection::North, DoorType::CurtainDoor);

  EXPECT_EQ(x, 14 * DoorPositionManager::kTileSize);
  EXPECT_EQ(y, 4 * DoorPositionManager::kTileSize);
  EXPECT_EQ(width, 4 * DoorPositionManager::kTileSize);
  EXPECT_EQ(height, 4 * DoorPositionManager::kTileSize);
}

TEST(DoorPositionManagerTest,
     EditorBoundsKeepExplodingWallOnGenericAnchorSize) {
  const auto [x, y, width, height] = DoorPositionManager::GetDoorEditorBounds(
      /*position=*/0, DoorDirection::North, DoorType::ExplodingWall);

  EXPECT_EQ(x, 14 * DoorPositionManager::kTileSize);
  EXPECT_EQ(y, 4 * DoorPositionManager::kTileSize);
  EXPECT_EQ(width, 4 * DoorPositionManager::kTileSize);
  EXPECT_EQ(height, 3 * DoorPositionManager::kTileSize);
}

TEST(DoorTypesTest, PlaceableCatalogCoversSupportedRomHackVariants) {
  constexpr auto types = GetPlaceableDoorTypes();
  EXPECT_EQ(types.size(), 32U);

  for (size_t i = 0; i < types.size(); ++i) {
    EXPECT_EQ(std::count(types.begin(), types.end(), types[i]), 1)
        << "duplicate door type 0x" << std::hex << static_cast<int>(types[i]);
  }

  for (DoorType supported : {
           DoorType::ExitLower,
           DoorType::FancyDungeonExitLower,
           DoorType::LitCaveExitLower,
           DoorType::SmallKeyStairsUpLower,
           DoorType::SmallKeyStairsDownLower,
           DoorType::BombableCaveExit,
           DoorType::UnopenableBigKeyDoor,
           DoorType::NormalDoorOneSidedShutter,
           DoorType::DoubleSidedShutterLower,
           DoorType::ExplicitRoomDoor,
           DoorType::BottomShutterLower,
           DoorType::TopShutterLower,
       }) {
    EXPECT_NE(std::find(types.begin(), types.end(), supported), types.end())
        << "missing supported door type 0x" << std::hex
        << static_cast<int>(supported);
  }

  for (DoorType unsafe : {
           DoorType::UnusedCaveExit,
           DoorType::UnusableBottomShutter,
           DoorType::UnusedDoubleSidedShutter,
           DoorType::UnusableNormalDoor4C,
           DoorType::UnusableGlitchyDoor54,
           DoorType::UnusableGlitchyStairsDown66,
       }) {
    EXPECT_EQ(std::find(types.begin(), types.end(), unsafe), types.end())
        << "unsafe door type exposed in placement UI: 0x" << std::hex
        << static_cast<int>(unsafe);
  }
}

TEST(DoorTypesTest, RoomConnectionPredicateRejectsExitsAndControlMarkers) {
  EXPECT_TRUE(IsRoomConnectionDoorType(DoorType::NormalDoor));
  EXPECT_TRUE(IsRoomConnectionDoorType(DoorType::BigKeyDoor));

  for (const auto type : {
           DoorType::ExitLower,
           DoorType::FancyDungeonExit,
           DoorType::FancyDungeonExitLower,
           DoorType::CaveExit,
           DoorType::LitCaveExitLower,
           DoorType::ExitMarker,
           DoorType::DungeonSwapMarker,
           DoorType::LayerSwapMarker,
       }) {
    EXPECT_FALSE(IsRoomConnectionDoorType(type)) << GetDoorTypeName(type);
  }

  EXPECT_FALSE(IsExitDoorType(DoorType::DungeonSwapMarker));
  EXPECT_FALSE(IsExitDoorType(DoorType::LayerSwapMarker));
}

}  // namespace
}  // namespace yaze::zelda3
