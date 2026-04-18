#include "app/gui/widgets/tile_selector_widget.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"
#include "testing.h"

namespace yaze {
namespace test {

using ::testing::Eq;
using ::testing::NotNull;

/**
 * @brief Test fixture for TileSelectorWidget tests.
 *
 * Creates and destroys ImGui context for tests that need it.
 * Tests that call ImGui functions (like Render) require the context,
 * while pure logic tests (like TileOrigin calculations) do not.
 */
class TileSelectorWidgetTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create ImGui context for tests that need it (e.g., Render tests)
    // This is required because Canvas and TileSelectorWidget use ImGui functions
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);

    // Initialize minimal ImGui IO for testing
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1920, 1080);
    io.DeltaTime = 1.0f / 60.0f;

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

  void TearDown() override {
    // Clean up canvas before destroying ImGui context
    canvas_.reset();

    // Destroy ImGui context
    if (imgui_context_) {
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  ImGuiContext* imgui_context_ = nullptr;
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

TEST_F(TileSelectorWidgetTest, MaxTileIdReflectsTileCount) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);
  EXPECT_EQ(widget.GetMaxTileId(), 63);

  widget.SetTileCount(1);
  EXPECT_EQ(widget.GetMaxTileId(), 0);

  widget.SetTileCount(0);
  EXPECT_EQ(widget.GetMaxTileId(), 0);
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

TEST_F(TileSelectorWidgetTest, JumpToTileFromInputSucceeds) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.AttachCanvas(canvas_.get());
  widget.SetTileCount(64);
  widget.SetSelectedTile(0);

  auto result = widget.JumpToTileFromInput("1A");
  EXPECT_EQ(result, gui::TileSelectorWidget::JumpToTileResult::kSuccess);
  EXPECT_EQ(widget.GetSelectedTileID(), 0x1A);
}

TEST_F(TileSelectorWidgetTest, JumpToTileFromInputRejectsInvalidFormat) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.AttachCanvas(canvas_.get());
  widget.SetTileCount(64);
  widget.SetSelectedTile(7);

  auto result = widget.JumpToTileFromInput("GG");
  EXPECT_EQ(result, gui::TileSelectorWidget::JumpToTileResult::kInvalidFormat);
  EXPECT_EQ(widget.GetSelectedTileID(), 7);
}

TEST_F(TileSelectorWidgetTest, JumpToTileFromInputRejectsOutOfRange) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.AttachCanvas(canvas_.get());
  widget.SetTileCount(64);
  widget.SetSelectedTile(9);

  auto result = widget.JumpToTileFromInput("FF");
  EXPECT_EQ(result, gui::TileSelectorWidget::JumpToTileResult::kOutOfRange);
  EXPECT_EQ(widget.GetSelectedTileID(), 9);
}

TEST_F(TileSelectorWidgetTest, JumpToTileFromInputRejectsEmptyString) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.AttachCanvas(canvas_.get());
  widget.SetTileCount(64);
  widget.SetSelectedTile(12);

  auto result = widget.JumpToTileFromInput("");
  EXPECT_EQ(result, gui::TileSelectorWidget::JumpToTileResult::kInvalidFormat);
  EXPECT_EQ(widget.GetSelectedTileID(), 12);
}

TEST_F(TileSelectorWidgetTest, JumpToTileFromInputSupportsDecimalPrefix) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.AttachCanvas(canvas_.get());
  widget.SetTileCount(128);
  widget.SetSelectedTile(0);

  auto result = widget.JumpToTileFromInput("d:26");
  EXPECT_EQ(result, gui::TileSelectorWidget::JumpToTileResult::kSuccess);
  EXPECT_EQ(widget.GetSelectedTileID(), 26);
}

TEST_F(TileSelectorWidgetTest,
       JumpToTileFromInputDefaultNumericRemainsHexForCompatibility) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.AttachCanvas(canvas_.get());
  widget.SetTileCount(128);
  widget.SetSelectedTile(0);

  auto result = widget.JumpToTileFromInput("10");
  EXPECT_EQ(result, gui::TileSelectorWidget::JumpToTileResult::kSuccess);
  EXPECT_EQ(widget.GetSelectedTileID(), 0x10);
}

TEST_F(TileSelectorWidgetTest, JumpToTileFromInputDecimalOutOfRange) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.AttachCanvas(canvas_.get());
  widget.SetTileCount(64);
  widget.SetSelectedTile(3);

  auto result = widget.JumpToTileFromInput("d:999");
  EXPECT_EQ(result, gui::TileSelectorWidget::JumpToTileResult::kOutOfRange);
  EXPECT_EQ(widget.GetSelectedTileID(), 3);
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

TEST_F(TileSelectorWidgetTest, GridContentSizeMatchesConfigGeometry) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);

  const ImVec2 content_size = widget.GetGridContentSize();
  EXPECT_FLOAT_EQ(content_size.x, 260.0f);
  EXPECT_FLOAT_EQ(content_size.y, 256.0f);
}

TEST_F(TileSelectorWidgetTest, PreferredViewportWidthLeavesRoomForControls) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);

  EXPECT_GE(widget.GetPreferredViewportWidth(), 332.0f);
  EXPECT_GE(widget.GetPreferredViewportWidth(), widget.GetGridContentSize().x);
}

// Test render without atlas (should not crash)
// NOTE: This test requires a full ImGui frame context which is complex to set up
// in a unit test without SDL/renderer backends. We test the early return path
// where canvas_ is nullptr instead.
TEST_F(TileSelectorWidgetTest, RenderWithoutCanvas) {
  gui::TileSelectorWidget widget("test_widget", config_);
  // Do NOT attach canvas - this tests the early return path

  gfx::Bitmap atlas;
  auto result = widget.Render(atlas, false);

  // With no canvas attached, Render should return early with default result
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

// ============================================================================
// Range Filter Tests
// ============================================================================

TEST_F(TileSelectorWidgetTest, RangeFilterDefaultInactive) {
  gui::TileSelectorWidget widget("test_widget", config_);
  EXPECT_FALSE(widget.has_active_range_filter());
}

TEST_F(TileSelectorWidgetTest, SetRangeFilterActivatesFilter) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);

  widget.SetRangeFilter(10, 30);
  EXPECT_TRUE(widget.has_active_range_filter());
  EXPECT_EQ(widget.filter_range_min(), 10);
  EXPECT_EQ(widget.filter_range_max(), 30);
}

TEST_F(TileSelectorWidgetTest, ClearRangeFilterDeactivates) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);

  widget.SetRangeFilter(10, 30);
  EXPECT_TRUE(widget.has_active_range_filter());

  widget.ClearRangeFilter();
  EXPECT_FALSE(widget.has_active_range_filter());
}

TEST_F(TileSelectorWidgetTest, RangeFilterClampsToTileCount) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);

  widget.SetRangeFilter(0, 100);  // Max exceeds total
  EXPECT_TRUE(widget.has_active_range_filter());
  EXPECT_EQ(widget.filter_range_min(), 0);
  EXPECT_EQ(widget.filter_range_max(), 63);  // Clamped to total_tiles - 1
}

TEST_F(TileSelectorWidgetTest, RangeFilterRejectsInvertedRange) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);

  widget.SetRangeFilter(30, 10);  // min > max
  EXPECT_FALSE(widget.has_active_range_filter());
}

TEST_F(TileSelectorWidgetTest, RangeFilterNegativeMinClampedToZero) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);

  widget.SetRangeFilter(-5, 20);
  EXPECT_TRUE(widget.has_active_range_filter());
  EXPECT_EQ(widget.filter_range_min(), 0);
  EXPECT_EQ(widget.filter_range_max(), 20);
}

TEST_F(TileSelectorWidgetTest, RangeFilterSingleTile) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);

  widget.SetRangeFilter(32, 32);
  EXPECT_TRUE(widget.has_active_range_filter());
  EXPECT_EQ(widget.filter_range_min(), 32);
  EXPECT_EQ(widget.filter_range_max(), 32);
}

// SetRangeFilter with both values > total_tiles_ should not activate the
// filter (both clamp, then min > max, so early return).
TEST_F(TileSelectorWidgetTest, RangeFilterBothOutOfBoundsDoesNotActivate) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);  // tiles 0..63

  widget.SetRangeFilter(100, 200);
  EXPECT_FALSE(widget.has_active_range_filter());
}

// SetRangeFilter with min in range and max out of range should activate with
// clamped max (not the same as both-out-of-bounds case).
TEST_F(TileSelectorWidgetTest, RangeFilterMinInRangeMaxOutClamps) {
  gui::TileSelectorWidget widget("test_widget", config_);
  widget.SetTileCount(64);  // tiles 0..63

  widget.SetRangeFilter(10, 200);
  EXPECT_TRUE(widget.has_active_range_filter());
  EXPECT_EQ(widget.filter_range_min(), 10);
  EXPECT_EQ(widget.filter_range_max(), 63);  // clamped to total_tiles - 1
}

}  // namespace test
}  // namespace yaze
