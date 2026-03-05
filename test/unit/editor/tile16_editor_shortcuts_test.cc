#include "app/editor/overworld/tile16_editor_shortcuts.h"

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

TEST(Tile16EditorShortcutsTest, PlainDigitsOneThroughFourFocusQuadrants) {
  for (int i = 0; i < 4; ++i) {
    const auto shortcut = ResolveTile16NumericShortcut(
        /*ctrl_held=*/false, /*number_index=*/i);
    ASSERT_TRUE(shortcut.quadrant_focus.has_value());
    EXPECT_EQ(*shortcut.quadrant_focus, i);
    EXPECT_FALSE(shortcut.palette_id.has_value());
  }
}

TEST(Tile16EditorShortcutsTest, PlainDigitsFiveThroughEightDoNotMutateState) {
  for (int i = 4; i < 8; ++i) {
    const auto shortcut = ResolveTile16NumericShortcut(
        /*ctrl_held=*/false, /*number_index=*/i);
    EXPECT_FALSE(shortcut.quadrant_focus.has_value());
    EXPECT_FALSE(shortcut.palette_id.has_value());
  }
}

TEST(Tile16EditorShortcutsTest, CtrlDigitsMapToPaletteRowsOneThroughEight) {
  for (int i = 0; i < 8; ++i) {
    const auto shortcut = ResolveTile16NumericShortcut(
        /*ctrl_held=*/true, /*number_index=*/i);
    EXPECT_FALSE(shortcut.quadrant_focus.has_value());
    ASSERT_TRUE(shortcut.palette_id.has_value());
    EXPECT_EQ(*shortcut.palette_id, static_cast<uint8_t>(i));
  }
}

TEST(Tile16EditorShortcutsTest, InvalidDigitsAreIgnored) {
  auto negative = ResolveTile16NumericShortcut(
      /*ctrl_held=*/false, /*number_index=*/-1);
  EXPECT_FALSE(negative.quadrant_focus.has_value());
  EXPECT_FALSE(negative.palette_id.has_value());

  auto oversized = ResolveTile16NumericShortcut(
      /*ctrl_held=*/true, /*number_index=*/8);
  EXPECT_FALSE(oversized.quadrant_focus.has_value());
  EXPECT_FALSE(oversized.palette_id.has_value());
}

}  // namespace
}  // namespace yaze::editor
