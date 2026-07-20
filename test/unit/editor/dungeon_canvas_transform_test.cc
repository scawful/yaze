#include "app/editor/dungeon/dungeon_canvas_transform.h"

#include <array>

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

TEST(DungeonCanvasTransformTest, RoundTripsKnownTileAtSupportedScales) {
  constexpr std::array<float, 3> kScales = {0.5f, 1.0f, 2.0f};
  constexpr ImVec2 kCanvasOrigin(100.0f, 200.0f);
  constexpr ImVec2 kScrolling(48.0f, -24.0f);
  constexpr ImVec2 kKnownRoomPixel(140.0f, 236.0f);

  for (const float scale : kScales) {
    const DungeonCanvasTransform transform(kCanvasOrigin, kScrolling, scale);
    const ImVec2 screen = transform.RoomPixelsToScreen(kKnownRoomPixel);
    const ImVec2 room = transform.ScreenToRoomPixels(screen);
    const auto tile = transform.ScreenToRoomTiles(screen, 8);

    EXPECT_FLOAT_EQ(room.x, kKnownRoomPixel.x) << "scale=" << scale;
    EXPECT_FLOAT_EQ(room.y, kKnownRoomPixel.y) << "scale=" << scale;
    EXPECT_EQ(tile, std::make_pair(17, 29)) << "scale=" << scale;
  }
}

TEST(DungeonCanvasTransformTest, AccountsForPositiveAndNegativeScrolling) {
  constexpr ImVec2 kCanvasOrigin(80.0f, 120.0f);
  constexpr ImVec2 kRoomPixel(64.0f, 96.0f);
  constexpr std::array<ImVec2, 2> kScrolling = {ImVec2(40.0f, 24.0f),
                                                ImVec2(-56.0f, -32.0f)};
  constexpr std::array<float, 3> kScales = {0.5f, 1.0f, 2.0f};

  for (const ImVec2 scrolling : kScrolling) {
    for (const float scale : kScales) {
      const DungeonCanvasTransform transform(kCanvasOrigin, scrolling, scale);
      const ImVec2 screen = transform.RoomPixelsToScreen(kRoomPixel);

      EXPECT_FLOAT_EQ(screen.x,
                      kCanvasOrigin.x + scrolling.x + kRoomPixel.x * scale);
      EXPECT_FLOAT_EQ(screen.y,
                      kCanvasOrigin.y + scrolling.y + kRoomPixel.y * scale);
      EXPECT_EQ(transform.ScreenToRoomPixelCoordinates(screen),
                std::make_pair(64, 96));
    }
  }
}

TEST(DungeonCanvasTransformTest, RoomGeometryUsesScrolledOriginAtEveryScale) {
  constexpr ImVec2 kCanvasOrigin(80.0f, 120.0f);
  constexpr ImVec2 kRoomPixel(64.0f, 96.0f);
  constexpr ImVec2 kRoomSize(24.0f, 40.0f);
  constexpr std::array<ImVec2, 2> kScrolling = {ImVec2(40.0f, 24.0f),
                                                ImVec2(-56.0f, -32.0f)};
  constexpr std::array<float, 3> kScales = {0.5f, 1.0f, 2.0f};

  for (const ImVec2 scrolling : kScrolling) {
    for (const float scale : kScales) {
      const DungeonCanvasTransform transform(kCanvasOrigin, scrolling, scale);
      const ImVec2 screen_start = transform.RoomPixelsToScreen(kRoomPixel);
      const ImVec2 screen_size = transform.RoomSizeToScreen(kRoomSize);

      EXPECT_FLOAT_EQ(screen_start.x,
                      transform.room_origin_screen().x + kRoomPixel.x * scale);
      EXPECT_FLOAT_EQ(screen_start.y,
                      transform.room_origin_screen().y + kRoomPixel.y * scale);
      EXPECT_FLOAT_EQ(screen_size.x, kRoomSize.x * scale);
      EXPECT_FLOAT_EQ(screen_size.y, kRoomSize.y * scale);
    }
  }
}

TEST(DungeonCanvasTransformTest, PreservesOneXZeroScrollBehavior) {
  const DungeonCanvasTransform transform(ImVec2(25.0f, 40.0f), ImVec2(0, 0),
                                         1.0f);

  const ImVec2 screen = transform.RoomPixelsToScreen(ImVec2(88.0f, 104.0f));
  EXPECT_FLOAT_EQ(screen.x, 113.0f);
  EXPECT_FLOAT_EQ(screen.y, 144.0f);
  EXPECT_EQ(transform.ScreenToRoomPixelCoordinates(screen),
            std::make_pair(88, 104));
}

TEST(DungeonCanvasTransformTest, FloorsCoordinatesOutsideRoomOrigin) {
  const DungeonCanvasTransform transform(ImVec2(10.0f, 20.0f),
                                         ImVec2(-4.0f, 6.0f), 2.0f);

  EXPECT_EQ(transform.ScreenToRoomPixelCoordinates(ImVec2(5.0f, 25.0f)),
            std::make_pair(-1, -1));
  EXPECT_EQ(transform.ScreenToRoomTiles(ImVec2(5.0f, 25.0f), 8),
            std::make_pair(-1, -1));
}

TEST(DungeonCanvasTransformTest, FallsBackToOneXForInvalidScale) {
  const DungeonCanvasTransform transform(ImVec2(10.0f, 20.0f),
                                         ImVec2(5.0f, -5.0f), 0.0f);

  EXPECT_FLOAT_EQ(transform.scale(), 1.0f);
  const ImVec2 screen = transform.RoomPixelsToScreen(ImVec2(8.0f, 16.0f));
  EXPECT_FLOAT_EQ(screen.x, 23.0f);
  EXPECT_FLOAT_EQ(screen.y, 31.0f);
}

}  // namespace
}  // namespace yaze::editor
