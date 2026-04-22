#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

constexpr float kMinCanvasWidth = 420.0f;
constexpr float kMinSidebarWidth = 320.0f;
constexpr float kSplitterWidth = 8.0f;
constexpr float kCompactLeftWidth = 230.4f;
constexpr float kCompactRightWidth = 272.0f;

TEST(DungeonWorkbenchContentLayoutTest,
     PrefersCompactingAndHidingLeftPaneBeforeRightPane) {
  const auto full = ResolveDungeonWorkbenchResponsiveLayout(
      1076.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(full.show_left);
  EXPECT_TRUE(full.show_right);
  EXPECT_FALSE(full.compact_left);
  EXPECT_FALSE(full.compact_right);

  const auto compact_left = ResolveDungeonWorkbenchResponsiveLayout(
      1075.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(compact_left.show_left);
  EXPECT_TRUE(compact_left.show_right);
  EXPECT_TRUE(compact_left.compact_left);
  EXPECT_FALSE(compact_left.compact_right);

  const auto both_compact = ResolveDungeonWorkbenchResponsiveLayout(
      979.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(both_compact.show_left);
  EXPECT_TRUE(both_compact.show_right);
  EXPECT_TRUE(both_compact.compact_left);
  EXPECT_TRUE(both_compact.compact_right);

  const auto hide_left = ResolveDungeonWorkbenchResponsiveLayout(
      931.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_FALSE(hide_left.show_left);
  EXPECT_TRUE(hide_left.show_right);
  EXPECT_FALSE(hide_left.compact_left);
  EXPECT_TRUE(hide_left.compact_right);

  const auto hide_both = ResolveDungeonWorkbenchResponsiveLayout(
      699.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_FALSE(hide_both.show_left);
  EXPECT_FALSE(hide_both.show_right);
}

TEST(DungeonWorkbenchContentLayoutTest,
     PaneLayoutUsesSharedCompactWidthsAtResponsiveThresholds) {
  const auto compact_both = ResolveDungeonWorkbenchPaneLayout(
      938.4f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 280.0f, 320.0f,
      true, true);
  EXPECT_TRUE(compact_both.responsive.show_left);
  EXPECT_TRUE(compact_both.responsive.show_right);
  EXPECT_TRUE(compact_both.responsive.compact_left);
  EXPECT_TRUE(compact_both.responsive.compact_right);
  EXPECT_NEAR(compact_both.left_width, kCompactLeftWidth, 0.001f);
  EXPECT_NEAR(compact_both.right_width, kCompactRightWidth, 0.001f);
  EXPECT_NEAR(compact_both.center_width, 420.0f, 0.001f);

  const auto hide_left = ResolveDungeonWorkbenchPaneLayout(
      931.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 280.0f, 320.0f,
      true, true);
  EXPECT_FALSE(hide_left.responsive.show_left);
  EXPECT_TRUE(hide_left.responsive.show_right);
  EXPECT_NEAR(hide_left.left_width, 0.0f, 0.001f);
  EXPECT_NEAR(hide_left.right_width, kCompactRightWidth, 0.001f);
  EXPECT_NEAR(hide_left.center_width,
              931.0f - kCompactRightWidth - kSplitterWidth, 0.001f);
}

TEST(DungeonWorkbenchContentLayoutTest,
     OversizedPaneMemoryStillProtectsMinimumCanvasWidth) {
  const auto layout = ResolveDungeonWorkbenchPaneLayout(
      1000.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 500.0f,
      360.0f, true, true);
  EXPECT_TRUE(layout.responsive.show_left);
  EXPECT_TRUE(layout.responsive.show_right);
  EXPECT_TRUE(layout.responsive.compact_left);
  EXPECT_FALSE(layout.responsive.compact_right);
  EXPECT_NEAR(layout.left_width, kCompactLeftWidth, 0.001f);
  EXPECT_NEAR(layout.right_width, 333.6f, 0.001f);
  EXPECT_GE(layout.center_width, kMinCanvasWidth);
}

}  // namespace
}  // namespace yaze::editor
