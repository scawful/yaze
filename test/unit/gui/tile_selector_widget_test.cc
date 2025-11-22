#include "app/gui/widgets/tile_selector_widget.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas.h"
#include "testing.h"

namespace yaze {
namespace test {

using ::testing::Eq;
using ::testing::NotNull;

class TileSelectorWidgetTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a test canvas
    canvas_ = std::make_unique<gui::Canvas>("TestCanvas", ImVec2(512, 512),
                                            gui::CanvasGridSize::k16x16);

    // Create a test config
    config_.tile_size = 16;
    config_.display_scale = 2.0f;
    config_.tiles_per_row = 8;
    config_.total_tiles = 64;  // 8x8 grid
    config_.draw_offset = {2.0f, 0.0f};
    config_.show_tile_ids = false;
    config_.highlight_color = {1.0f, 0.85f, 0.35f, 1.0f};
  }

  std::unique_ptr<gui::Canvas> canvas_;
  gui::TileSelectorWidget::Config config_;
};

// Test basic construction
TEST_F(TileSelectorWidgetTest, Construction) {
  gui::TileSelectorWidget widget("test_widget");
  EXPECT_EQ(widget.GetSelectedTileID(), 0);
}

// Test construction with config
TEST_F(TileSelectorWidgetTest, ConstructionWithConfig) {
  gui::TileSelectorWidget widget("test_widget", config_);
  EXPECT_EQ(widget.GetSelectedTileID(), 0);
}

// Test canvas attachment
TEST_F(TileSelectorWidgetTest, AttachCanvas) {
  gui::TileSelectorWidget widget("test_widget");
  widget.AttachCanvas(canvas_.get());
  // No crash means success
}

// Test tile count setting
TEST_F(TileSelectorWidgetTest, SetTileCount) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(128);
  // Verify selection is clamped when tile count changes
  widget.SetSelectedTile(100);
  EXPECT_EQ(widget.GetSelectedTileID(), 100);

  // Setting tile count lower should clamp selection
  widget.SetTileCount(50);
  EXPECT_EQ(widget.GetSelectedTileID(), 0);  // Should reset to 0
}

// Test selected tile setting
TEST_F(TileSelectorWidgetTest, SetSelectedTile) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);

  widget.SetSelectedTile(10);
  EXPECT_EQ(widget.GetSelectedTileID(), 10);

  widget.SetSelectedTile(63);
  EXPECT_EQ(widget.GetSelectedTileID(), 63);

  // Out of bounds should be ignored
  widget.SetSelectedTile(64);
  EXPECT_EQ(widget.GetSelectedTileID(), 63);  // Should remain unchanged

  widget.SetSelectedTile(-1);
  EXPECT_EQ(widget.GetSelectedTileID(), 63);  // Should remain unchanged
}

// Test tile origin calculation
TEST_F(TileSelectorWidgetTest, TileOrigin) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);

  // Test first tile (0,0)
  auto origin = widget.TileOrigin(0);
  EXPECT_FLOAT_EQ(origin.x, config_.draw_offset.x);
  EXPECT_FLOAT_EQ(origin.y, config_.draw_offset.y);

  // Test tile at (1,0)
  origin = widget.TileOrigin(1);
  float expected_x =
      config_.draw_offset.x + (config_.tile_size * config_.display_scale);
  EXPECT_FLOAT_EQ(origin.x, expected_x);
  EXPECT_FLOAT_EQ(origin.y, config_.draw_offset.y);

  // Test tile at (0,1) - first tile of second row
  origin = widget.TileOrigin(8);
  expected_x = config_.draw_offset.x;
  float expected_y =
      config_.draw_offset.y + (config_.tile_size * config_.display_scale);
  EXPECT_FLOAT_EQ(origin.x, expected_x);
  EXPECT_FLOAT_EQ(origin.y, expected_y);

  // Test invalid tile ID
  origin = widget.TileOrigin(64);
  EXPECT_FLOAT_EQ(origin.x, -1.0f);
  EXPECT_FLOAT_EQ(origin.y, -1.0f);
}

// Test render without atlas (should not crash)
TEST_F(TileSelectorWidgetTest, RenderWithoutAtlas) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.AttachCanvas(canvas_.get());

  gfx::Bitmap atlas;
  auto result = widget.Render(atlas, false);

  EXPECT_FALSE(result.tile_clicked);
  EXPECT_FALSE(result.tile_double_clicked);
  EXPECT_FALSE(result.selection_changed);
  EXPECT_EQ(result.selected_tile, -1);
}

// Test programmatic selection for AI/automation
TEST_F(TileSelectorWidgetTest, ProgrammaticSelection) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.AttachCanvas(canvas_.get());
  widget.SetTileCount(64);

  // Simulate AI/automation selecting tiles programmatically
  for (int i = 0; i < 64; ++i) {
    widget.SetSelectedTile(i);
    EXPECT_EQ(widget.GetSelectedTileID(), i);

    auto origin = widget.TileOrigin(i);
    int expected_col = i % config_.tiles_per_row;
    int expected_row = i / config_.tiles_per_row;
    float expected_x = config_.draw_offset.x +
                       expected_col * config_.tile_size * config_.display_scale;
    float expected_y = config_.draw_offset.y +
                       expected_row * config_.tile_size * config_.display_scale;

    EXPECT_FLOAT_EQ(origin.x, expected_x);
    EXPECT_FLOAT_EQ(origin.y, expected_y);
  }
}

// Test scroll to tile
TEST_F(TileSelectorWidgetTest, ScrollToTile) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.AttachCanvas(canvas_.get());
  widget.SetTileCount(64);

  // Scroll to various tiles (should not crash)
  widget.ScrollToTile(0);
  widget.ScrollToTile(10);
  widget.ScrollToTile(63);

  // Invalid tile should not crash
  widget.ScrollToTile(-1);
  widget.ScrollToTile(64);
}

// Test different configs
TEST_F(TileSelectorWidgetTest, DifferentConfigs) {
  // Test with 16x16 grid
  gui::TileSelectorWidget::Config large_config;
  large_config.tile_size = 8;
  large_config.display_scale = 1.0f;
  large_config.tiles_per_row = 16;
  large_config.total_tiles = 256;
  large_config.draw_offset = {0.0f, 0.0f};

  gui::TileSelectorWidget large_widget("large_widget", large_config);
  large_widget.SetTileCount(256);

  for (int i = 0; i < 256; ++i) {
    large_widget.SetSelectedTile(i);
    EXPECT_EQ(large_widget.GetSelectedTileID(), i);
  }
}

}  // namespace test
}  // namespace yaze
