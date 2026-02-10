#include "gtest/gtest.h"

#include <set>
#include <utility>
#include <vector>

#include "app/editor/dungeon/interaction/paint_util.h"

namespace yaze::editor::paint_util {
namespace {

std::vector<std::pair<int, int>> CollectLine(int x0, int y0, int x1, int y1) {
  std::vector<std::pair<int, int>> pts;
  ForEachPointOnLine(x0, y0, x1, y1, [&](int x, int y) { pts.emplace_back(x, y); });
  return pts;
}

}  // namespace

TEST(PaintUtilTest, ForEachPointOnLine_IncludesEndpointsHorizontal) {
  const auto pts = CollectLine(0, 0, 3, 0);
  ASSERT_EQ(pts.size(), 4u);
  EXPECT_EQ(pts.front(), (std::pair<int, int>{0, 0}));
  EXPECT_EQ(pts.back(), (std::pair<int, int>{3, 0}));
  EXPECT_EQ(pts[1], (std::pair<int, int>{1, 0}));
  EXPECT_EQ(pts[2], (std::pair<int, int>{2, 0}));
}

TEST(PaintUtilTest, ForEachPointOnLine_WorksReverseDirection) {
  const auto pts = CollectLine(3, 0, 0, 0);
  ASSERT_EQ(pts.size(), 4u);
  EXPECT_EQ(pts.front(), (std::pair<int, int>{3, 0}));
  EXPECT_EQ(pts.back(), (std::pair<int, int>{0, 0}));
}

TEST(PaintUtilTest, ForEachPointOnLine_IncludesEndpointsDiagonal) {
  const auto pts = CollectLine(0, 0, 3, 3);
  ASSERT_EQ(pts.size(), 4u);
  EXPECT_EQ(pts.front(), (std::pair<int, int>{0, 0}));
  EXPECT_EQ(pts.back(), (std::pair<int, int>{3, 3}));
  EXPECT_EQ(pts[1], (std::pair<int, int>{1, 1}));
  EXPECT_EQ(pts[2], (std::pair<int, int>{2, 2}));
}

TEST(PaintUtilTest, ForEachPointInSquareBrush_ClampsToBounds) {
  std::set<std::pair<int, int>> pts;
  ForEachPointInSquareBrush(/*cx=*/0, /*cy=*/0, /*radius=*/1,
                            /*min_x=*/0, /*min_y=*/0, /*max_x=*/3, /*max_y=*/3,
                            [&](int x, int y) { pts.emplace(x, y); });
  // Radius=1 centered at (0,0) would be [-1..1] in each axis, clamped to [0..3]
  // => (0,0), (1,0), (0,1), (1,1)
  EXPECT_EQ(pts.size(), 4u);
  EXPECT_TRUE(pts.contains({0, 0}));
  EXPECT_TRUE(pts.contains({1, 0}));
  EXPECT_TRUE(pts.contains({0, 1}));
  EXPECT_TRUE(pts.contains({1, 1}));
}

TEST(PaintUtilTest, LinePlusBrush_CoversExpectedTiles) {
  // Paint a short vertical stroke at x=2 from y=1..3 with radius 1 (3x3).
  std::set<std::pair<int, int>> painted;
  ForEachPointOnLine(/*x0=*/2, /*y0=*/1, /*x1=*/2, /*y1=*/3, [&](int x, int y) {
    ForEachPointInSquareBrush(x, y, /*radius=*/1,
                              /*min_x=*/0, /*min_y=*/0, /*max_x=*/4, /*max_y=*/4,
                              [&](int bx, int by) { painted.emplace(bx, by); });
  });

  // The stroke covers y=1,2,3; with radius 1 it covers x=1..3, y=0..4.
  // Within bounds [0..4], that should include all points where x in [1,3].
  for (int y = 0; y <= 4; ++y) {
    EXPECT_TRUE(painted.contains({1, y}));
    EXPECT_TRUE(painted.contains({2, y}));
    EXPECT_TRUE(painted.contains({3, y}));
  }
  EXPECT_FALSE(painted.contains({0, 0}));
  EXPECT_FALSE(painted.contains({4, 4}));
}

}  // namespace yaze::editor::paint_util

