#include "app/gui/core/layout_helpers.h"

#include <gtest/gtest.h>

namespace yaze::gui {
namespace {

TEST(WindowClampTest, NoClampWhenInsideViewport) {
  const ImVec2 pos(100.0f, 120.0f);
  const ImVec2 size(200.0f, 160.0f);
  const ImVec2 rect_pos(0.0f, 0.0f);
  const ImVec2 rect_size(800.0f, 600.0f);

  const auto result =
      LayoutHelpers::ClampWindowToRect(pos, size, rect_pos, rect_size);

  EXPECT_FALSE(result.clamped);
  EXPECT_FLOAT_EQ(result.pos.x, pos.x);
  EXPECT_FLOAT_EQ(result.pos.y, pos.y);
}

TEST(WindowClampTest, ClampsWhenRightOffscreen) {
  const ImVec2 pos(900.0f, 50.0f);
  const ImVec2 size(200.0f, 150.0f);
  const ImVec2 rect_pos(0.0f, 0.0f);
  const ImVec2 rect_size(800.0f, 600.0f);

  const auto result =
      LayoutHelpers::ClampWindowToRect(pos, size, rect_pos, rect_size, 32.0f);

  EXPECT_TRUE(result.clamped);
  EXPECT_FLOAT_EQ(result.pos.x, 800.0f - 32.0f);
  EXPECT_FLOAT_EQ(result.pos.y, pos.y);
}

TEST(WindowClampTest, ClampsWhenLeftOffscreen) {
  const ImVec2 pos(-400.0f, 50.0f);
  const ImVec2 size(200.0f, 150.0f);
  const ImVec2 rect_pos(0.0f, 0.0f);
  const ImVec2 rect_size(800.0f, 600.0f);

  const auto result =
      LayoutHelpers::ClampWindowToRect(pos, size, rect_pos, rect_size, 32.0f);

  EXPECT_TRUE(result.clamped);
  EXPECT_FLOAT_EQ(result.pos.x, 32.0f - 200.0f);
  EXPECT_FLOAT_EQ(result.pos.y, pos.y);
}

TEST(WindowClampTest, TinyViewportReducesMinVisible) {
  const ImVec2 pos(100.0f, 100.0f);
  const ImVec2 size(50.0f, 50.0f);
  const ImVec2 rect_pos(0.0f, 0.0f);
  const ImVec2 rect_size(10.0f, 10.0f);

  const auto result =
      LayoutHelpers::ClampWindowToRect(pos, size, rect_pos, rect_size, 32.0f);

  EXPECT_TRUE(result.clamped);
  EXPECT_FLOAT_EQ(result.pos.x, 5.0f);
  EXPECT_FLOAT_EQ(result.pos.y, 5.0f);
}

}  // namespace
}  // namespace yaze::gui
