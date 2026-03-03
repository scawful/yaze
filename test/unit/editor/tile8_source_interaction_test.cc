#include "app/editor/overworld/tile8_source_interaction.h"

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

TEST(Tile8SourceInteractionTest, UsageHighlightRequiresHoverAndRightMouseDown) {
  EXPECT_TRUE(ComputeTile8UsageHighlight(/*source_hovered=*/true,
                                         /*right_mouse_down=*/true));
  EXPECT_FALSE(ComputeTile8UsageHighlight(/*source_hovered=*/false,
                                          /*right_mouse_down=*/true));
  EXPECT_FALSE(ComputeTile8UsageHighlight(/*source_hovered=*/true,
                                          /*right_mouse_down=*/false));
}

TEST(Tile8SourceInteractionTest, ComputesTileIndexFromMouseCoordinates) {
  // 128px wide source bitmap = 16 tile8 per row.
  const int tile8 = ComputeTile8IndexFromCanvasMouse(
      /*mouse_x=*/40.0f, /*mouse_y=*/56.0f,
      /*source_bitmap_width_px=*/128, /*max_tile_count=*/256,
      /*display_scale=*/4.0f);

  // tile_x = floor(40/32)=1, tile_y=floor(56/32)=1 => 1 + (1*16) = 17.
  EXPECT_EQ(tile8, 17);
}

TEST(Tile8SourceInteractionTest, OutOfRangeOrInvalidInputsReturnNegativeOne) {
  EXPECT_EQ(ComputeTile8IndexFromCanvasMouse(
                /*mouse_x=*/-1.0f, /*mouse_y=*/0.0f,
                /*source_bitmap_width_px=*/128, /*max_tile_count=*/256,
                /*display_scale=*/4.0f),
            -1);

  EXPECT_EQ(ComputeTile8IndexFromCanvasMouse(
                /*mouse_x=*/0.0f, /*mouse_y=*/0.0f,
                /*source_bitmap_width_px=*/0, /*max_tile_count=*/256,
                /*display_scale=*/4.0f),
            -1);

  EXPECT_EQ(ComputeTile8IndexFromCanvasMouse(
                /*mouse_x=*/4096.0f, /*mouse_y=*/4096.0f,
                /*source_bitmap_width_px=*/128, /*max_tile_count=*/64,
                /*display_scale=*/4.0f),
            -1);
}

}  // namespace
}  // namespace yaze::editor
