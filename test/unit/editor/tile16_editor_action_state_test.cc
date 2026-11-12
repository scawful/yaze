#include "app/editor/overworld/tile16/tile16_editor_action_state.h"

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

TEST(Tile16EditorActionStateTest, CompactActionColumnsPreferReadableLabels) {
  EXPECT_EQ(ComputeTile16CompactActionColumnCount(920.0f), 7);
  EXPECT_EQ(ComputeTile16CompactActionColumnCount(760.0f), 5);
  EXPECT_EQ(ComputeTile16CompactActionColumnCount(460.0f), 3);
  EXPECT_EQ(ComputeTile16CompactActionColumnCount(320.0f), 2);
  EXPECT_EQ(ComputeTile16CompactActionColumnCount(220.0f), 1);
}

TEST(Tile16EditorActionStateTest, ActionRowCountHandlesNarrowLayouts) {
  EXPECT_EQ(ComputeTile16ActionRowCount(/*action_count=*/7,
                                        /*column_count=*/7),
            1);
  EXPECT_EQ(ComputeTile16ActionRowCount(/*action_count=*/7,
                                        /*column_count=*/5),
            2);
  EXPECT_EQ(ComputeTile16ActionRowCount(/*action_count=*/7,
                                        /*column_count=*/2),
            4);
  EXPECT_EQ(ComputeTile16ActionRowCount(/*action_count=*/0,
                                        /*column_count=*/2),
            0);
  EXPECT_EQ(ComputeTile16ActionRowCount(/*action_count=*/3,
                                        /*column_count=*/0),
            3);
}

TEST(Tile16EditorActionStateTest, PaletteButtonSizeStaysUsableWhenNarrow) {
  EXPECT_FLOAT_EQ(ComputeTile16PaletteButtonSize(260.0f), 32.0f);
  EXPECT_FLOAT_EQ(ComputeTile16PaletteButtonSize(144.0f), 32.0f);
  EXPECT_FLOAT_EQ(ComputeTile16PaletteButtonSize(80.0f), 24.0f);
  EXPECT_FLOAT_EQ(ComputeTile16PaletteButtonSize(0.0f), 24.0f);
}

}  // namespace
}  // namespace yaze::editor
