#include "app/gui/canvas/canvas_automation_api.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/gui/canvas.h"
#include "testing.h"

namespace yaze {
namespace test {

using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;

class CanvasAutomationAPITest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a test canvas with known dimensions
    canvas_ = std::make_unique<gui::Canvas>("TestCanvas", ImVec2(512, 512),
                                             gui::CanvasGridSize::k16x16);
    api_ = canvas_->GetAutomationAPI();
    ASSERT_NE(api_, nullptr);
  }

  std::unique_ptr<gui::Canvas> canvas_;
  gui::CanvasAutomationAPI* api_;
};

// ============================================================================
// Coordinate Conversion Tests
// ============================================================================

TEST_F(CanvasAutomationAPITest, TileToCanvas_BasicConversion) {
  // At 1.0x zoom, tile (0,0) should be at canvas (0,0)
  canvas_->set_global_scale(1.0f);
  
  ImVec2 pos = api_->TileToCanvas(0, 0);
  EXPECT_FLOAT_EQ(pos.x, 0.0f);
  EXPECT_FLOAT_EQ(pos.y, 0.0f);
  
  // Tile (1,0) should be at (16,0) for 16x16 grid
  pos = api_->TileToCanvas(1, 0);
  EXPECT_FLOAT_EQ(pos.x, 16.0f);
  EXPECT_FLOAT_EQ(pos.y, 0.0f);
  
  // Tile (0,1) should be at (0,16)
  pos = api_->TileToCanvas(0, 1);
  EXPECT_FLOAT_EQ(pos.x, 0.0f);
  EXPECT_FLOAT_EQ(pos.y, 16.0f);
  
  // Tile (10,10) should be at (160,160)
  pos = api_->TileToCanvas(10, 10);
  EXPECT_FLOAT_EQ(pos.x, 160.0f);
  EXPECT_FLOAT_EQ(pos.y, 160.0f);
}

TEST_F(CanvasAutomationAPITest, TileToCanvas_WithZoom) {
  // At 2.0x zoom, tile coordinates should scale
  canvas_->set_global_scale(2.0f);
  
  ImVec2 pos = api_->TileToCanvas(1, 1);
  EXPECT_FLOAT_EQ(pos.x, 32.0f);  // 1 * 16 * 2.0
  EXPECT_FLOAT_EQ(pos.y, 32.0f);
  
  // At 0.5x zoom, tile coordinates should scale down
  canvas_->set_global_scale(0.5f);
  pos = api_->TileToCanvas(10, 10);
  EXPECT_FLOAT_EQ(pos.x, 80.0f);  // 10 * 16 * 0.5
  EXPECT_FLOAT_EQ(pos.y, 80.0f);
}

TEST_F(CanvasAutomationAPITest, CanvasToTile_BasicConversion) {
  canvas_->set_global_scale(1.0f);
  
  // Canvas (0,0) should be tile (0,0)
  ImVec2 tile = api_->CanvasToTile(ImVec2(0, 0));
  EXPECT_FLOAT_EQ(tile.x, 0.0f);
  EXPECT_FLOAT_EQ(tile.y, 0.0f);
  
  // Canvas (16,16) should be tile (1,1)
  tile = api_->CanvasToTile(ImVec2(16, 16));
  EXPECT_FLOAT_EQ(tile.x, 1.0f);
  EXPECT_FLOAT_EQ(tile.y, 1.0f);
  
  // Canvas (160,160) should be tile (10,10)
  tile = api_->CanvasToTile(ImVec2(160, 160));
  EXPECT_FLOAT_EQ(tile.x, 10.0f);
  EXPECT_FLOAT_EQ(tile.y, 10.0f);
}

TEST_F(CanvasAutomationAPITest, CanvasToTile_WithZoom) {
  // At 2.0x zoom
  canvas_->set_global_scale(2.0f);
  
  ImVec2 tile = api_->CanvasToTile(ImVec2(32, 32));
  EXPECT_FLOAT_EQ(tile.x, 1.0f);  // 32 / (16 * 2.0)
  EXPECT_FLOAT_EQ(tile.y, 1.0f);
  
  // At 0.5x zoom
  canvas_->set_global_scale(0.5f);
  tile = api_->CanvasToTile(ImVec2(8, 8));
  EXPECT_FLOAT_EQ(tile.x, 1.0f);  // 8 / (16 * 0.5)
  EXPECT_FLOAT_EQ(tile.y, 1.0f);
}

TEST_F(CanvasAutomationAPITest, CoordinateRoundTrip) {
  canvas_->set_global_scale(1.0f);
  
  // Test round-trip conversion
  for (int i = 0; i < 32; ++i) {
    ImVec2 canvas_pos = api_->TileToCanvas(i, i);
    ImVec2 tile_pos = api_->CanvasToTile(canvas_pos);
    
    EXPECT_FLOAT_EQ(tile_pos.x, static_cast<float>(i));
    EXPECT_FLOAT_EQ(tile_pos.y, static_cast<float>(i));
  }
}

// ============================================================================
// Bounds Checking Tests
// ============================================================================

TEST_F(CanvasAutomationAPITest, IsInBounds_ValidCoordinates) {
  EXPECT_TRUE(api_->IsInBounds(0, 0));
  EXPECT_TRUE(api_->IsInBounds(10, 10));
  EXPECT_TRUE(api_->IsInBounds(31, 31));  // 512/16 = 32 tiles, so 31 is max
}

TEST_F(CanvasAutomationAPITest, IsInBounds_InvalidCoordinates) {
  EXPECT_FALSE(api_->IsInBounds(-1, 0));
  EXPECT_FALSE(api_->IsInBounds(0, -1));
  EXPECT_FALSE(api_->IsInBounds(-1, -1));
  EXPECT_FALSE(api_->IsInBounds(32, 0));   // Out of bounds
  EXPECT_FALSE(api_->IsInBounds(0, 32));
  EXPECT_FALSE(api_->IsInBounds(100, 100));
}

// ============================================================================
// Tile Operations Tests
// ============================================================================

TEST_F(CanvasAutomationAPITest, SetTileAt_WithCallback) {
  // Set up a tile paint callback
  std::vector<std::tuple<int, int, int>> painted_tiles;
  api_->SetTilePaintCallback([&](int x, int y, int tile_id) {
    painted_tiles.push_back({x, y, tile_id});
    return true;
  });
  
  // Paint some tiles
  EXPECT_TRUE(api_->SetTileAt(5, 5, 42));
  EXPECT_TRUE(api_->SetTileAt(10, 10, 100));
  
  ASSERT_EQ(painted_tiles.size(), 2);
  EXPECT_EQ(painted_tiles[0], std::make_tuple(5, 5, 42));
  EXPECT_EQ(painted_tiles[1], std::make_tuple(10, 10, 100));
}

TEST_F(CanvasAutomationAPITest, SetTileAt_OutOfBounds) {
  bool callback_called = false;
  api_->SetTilePaintCallback([&](int x, int y, int tile_id) {
    callback_called = true;
    return true;
  });
  
  // Out of bounds tiles should return false without calling callback
  EXPECT_FALSE(api_->SetTileAt(-1, 0, 42));
  EXPECT_FALSE(api_->SetTileAt(0, -1, 42));
  EXPECT_FALSE(api_->SetTileAt(100, 100, 42));
  
  EXPECT_FALSE(callback_called);
}

TEST_F(CanvasAutomationAPITest, GetTileAt_WithCallback) {
  // Set up a tile query callback
  api_->SetTileQueryCallback([](int x, int y) {
    return x * 100 + y;  // Simple deterministic value
  });
  
  EXPECT_EQ(api_->GetTileAt(0, 0), 0);
  EXPECT_EQ(api_->GetTileAt(1, 0), 100);
  EXPECT_EQ(api_->GetTileAt(0, 1), 1);
  EXPECT_EQ(api_->GetTileAt(5, 7), 507);
}

TEST_F(CanvasAutomationAPITest, GetTileAt_OutOfBounds) {
  api_->SetTileQueryCallback([](int x, int y) { return 42; });
  
  EXPECT_EQ(api_->GetTileAt(-1, 0), -1);
  EXPECT_EQ(api_->GetTileAt(0, -1), -1);
  EXPECT_EQ(api_->GetTileAt(100, 100), -1);
}

TEST_F(CanvasAutomationAPITest, SetTiles_BatchOperation) {
  std::vector<std::tuple<int, int, int>> painted_tiles;
  api_->SetTilePaintCallback([&](int x, int y, int tile_id) {
    painted_tiles.push_back({x, y, tile_id});
    return true;
  });
  
  std::vector<std::tuple<int, int, int>> tiles_to_paint = {
    {0, 0, 10},
    {1, 0, 11},
    {2, 0, 12},
    {0, 1, 20},
    {1, 1, 21}
  };
  
  EXPECT_TRUE(api_->SetTiles(tiles_to_paint));
  EXPECT_EQ(painted_tiles.size(), 5);
}

// ============================================================================
// Selection Tests
// ============================================================================

TEST_F(CanvasAutomationAPITest, SelectTile) {
  api_->SelectTile(5, 5);
  
  auto selection = api_->GetSelection();
  EXPECT_TRUE(selection.has_selection);
  EXPECT_EQ(selection.selected_tiles.size(), 1);
}

TEST_F(CanvasAutomationAPITest, SelectTileRect) {
  api_->SelectTileRect(5, 5, 9, 9);
  
  auto selection = api_->GetSelection();
  EXPECT_TRUE(selection.has_selection);
  
  // 5x5 rectangle = 25 tiles
  EXPECT_EQ(selection.selected_tiles.size(), 25);
  
  // Check first and last tiles
  EXPECT_FLOAT_EQ(selection.selected_tiles[0].x, 5.0f);
  EXPECT_FLOAT_EQ(selection.selected_tiles[0].y, 5.0f);
  EXPECT_FLOAT_EQ(selection.selected_tiles[24].x, 9.0f);
  EXPECT_FLOAT_EQ(selection.selected_tiles[24].y, 9.0f);
}

TEST_F(CanvasAutomationAPITest, SelectTileRect_SwappedCoordinates) {
  // Should handle coordinates in any order
  api_->SelectTileRect(9, 9, 5, 5);  // Reversed
  
  auto selection = api_->GetSelection();
  EXPECT_TRUE(selection.has_selection);
  EXPECT_EQ(selection.selected_tiles.size(), 25);
}

TEST_F(CanvasAutomationAPITest, ClearSelection) {
  api_->SelectTileRect(5, 5, 10, 10);
  
  auto selection = api_->GetSelection();
  EXPECT_TRUE(selection.has_selection);
  
  api_->ClearSelection();
  
  selection = api_->GetSelection();
  EXPECT_FALSE(selection.has_selection);
  EXPECT_EQ(selection.selected_tiles.size(), 0);
}

TEST_F(CanvasAutomationAPITest, SelectTile_OutOfBounds) {
  api_->SelectTile(-1, 0);
  auto selection = api_->GetSelection();
  EXPECT_FALSE(selection.has_selection);
  
  api_->SelectTile(100, 100);
  selection = api_->GetSelection();
  EXPECT_FALSE(selection.has_selection);
}

// ============================================================================
// View Operations Tests
// ============================================================================

TEST_F(CanvasAutomationAPITest, SetZoom_ValidRange) {
  api_->SetZoom(1.0f);
  EXPECT_FLOAT_EQ(api_->GetZoom(), 1.0f);
  
  api_->SetZoom(2.0f);
  EXPECT_FLOAT_EQ(api_->GetZoom(), 2.0f);
  
  api_->SetZoom(0.5f);
  EXPECT_FLOAT_EQ(api_->GetZoom(), 0.5f);
}

TEST_F(CanvasAutomationAPITest, SetZoom_Clamping) {
  // Should clamp to 0.25 - 4.0 range
  api_->SetZoom(10.0f);
  EXPECT_LE(api_->GetZoom(), 4.0f);
  
  api_->SetZoom(0.1f);
  EXPECT_GE(api_->GetZoom(), 0.25f);
  
  api_->SetZoom(-1.0f);
  EXPECT_GE(api_->GetZoom(), 0.25f);
}

TEST_F(CanvasAutomationAPITest, ScrollToTile_ValidTile) {
  // Should not crash when scrolling to valid tiles
  api_->ScrollToTile(0, 0, true);
  api_->ScrollToTile(10, 10, false);
  api_->ScrollToTile(15, 15, true);
  
  // Just verify no crash - actual scroll behavior depends on ImGui state
}

TEST_F(CanvasAutomationAPITest, ScrollToTile_OutOfBounds) {
  // Should handle out of bounds gracefully
  api_->ScrollToTile(-1, 0, true);
  api_->ScrollToTile(100, 100, true);
  
  // Should not crash
}

TEST_F(CanvasAutomationAPITest, CenterOn_ValidTile) {
  // Should not crash when centering on valid tiles
  api_->CenterOn(10, 10);
  api_->CenterOn(0, 0);
  api_->CenterOn(20, 20);
  
  // Verify scroll position changed (should be non-zero after centering on non-origin)
  ImVec2 scroll = canvas_->scrolling();
  // Scroll values will depend on canvas size, just verify they're set
}

TEST_F(CanvasAutomationAPITest, CenterOn_OutOfBounds) {
  api_->CenterOn(-1, 0);
  api_->CenterOn(100, 100);
  
  // Should not crash
}

// ============================================================================
// Query Operations Tests
// ============================================================================

TEST_F(CanvasAutomationAPITest, GetDimensions) {
  canvas_->set_global_scale(1.0f);
  
  auto dims = api_->GetDimensions();
  EXPECT_EQ(dims.tile_size, 16);  // 16x16 grid
  EXPECT_EQ(dims.width_tiles, 32);  // 512 / 16
  EXPECT_EQ(dims.height_tiles, 32);
}

TEST_F(CanvasAutomationAPITest, GetDimensions_WithZoom) {
  canvas_->set_global_scale(2.0f);
  
  auto dims = api_->GetDimensions();
  EXPECT_EQ(dims.tile_size, 16);
  EXPECT_EQ(dims.width_tiles, 16);   // 512 / (16 * 2.0)
  EXPECT_EQ(dims.height_tiles, 16);
}

TEST_F(CanvasAutomationAPITest, GetVisibleRegion) {
  canvas_->set_global_scale(1.0f);
  canvas_->set_scrolling(ImVec2(0, 0));
  
  auto region = api_->GetVisibleRegion();
  
  // At origin with no scroll, should start at (0,0)
  EXPECT_GE(region.min_x, 0);
  EXPECT_GE(region.min_y, 0);
  
  // Should have valid bounds
  EXPECT_GE(region.max_x, region.min_x);
  EXPECT_GE(region.max_y, region.min_y);
}

TEST_F(CanvasAutomationAPITest, IsTileVisible_AtOrigin) {
  canvas_->set_global_scale(1.0f);
  canvas_->set_scrolling(ImVec2(0, 0));
  
  // Tiles at origin should be visible
  EXPECT_TRUE(api_->IsTileVisible(0, 0));
  EXPECT_TRUE(api_->IsTileVisible(1, 1));
  
  // Tiles far away might not be visible (depends on canvas size)
  // We just verify the method doesn't crash
  api_->IsTileVisible(50, 50);
}

TEST_F(CanvasAutomationAPITest, IsTileVisible_OutOfBounds) {
  // Out of bounds tiles should return false
  EXPECT_FALSE(api_->IsTileVisible(-1, 0));
  EXPECT_FALSE(api_->IsTileVisible(0, -1));
  EXPECT_FALSE(api_->IsTileVisible(100, 100));
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(CanvasAutomationAPITest, CompleteWorkflow) {
  // Simulate a complete automation workflow
  
  // 1. Set zoom level
  api_->SetZoom(1.0f);
  EXPECT_FLOAT_EQ(api_->GetZoom(), 1.0f);
  
  // 2. Select a tile region
  api_->SelectTileRect(0, 0, 4, 4);
  auto selection = api_->GetSelection();
  EXPECT_EQ(selection.selected_tiles.size(), 25);
  
  // 3. Query tile data with callback
  api_->SetTileQueryCallback([](int x, int y) {
    return x + y * 100;
  });
  
  EXPECT_EQ(api_->GetTileAt(2, 3), 302);
  
  // 4. Paint tiles with callback
  std::vector<std::tuple<int, int, int>> painted;
  api_->SetTilePaintCallback([&](int x, int y, int tile_id) {
    painted.push_back({x, y, tile_id});
    return true;
  });
  
  std::vector<std::tuple<int, int, int>> tiles = {
    {0, 0, 10}, {1, 0, 11}, {2, 0, 12}
  };
  EXPECT_TRUE(api_->SetTiles(tiles));
  EXPECT_EQ(painted.size(), 3);
  
  // 5. Clear selection
  api_->ClearSelection();
  selection = api_->GetSelection();
  EXPECT_FALSE(selection.has_selection);
}

TEST_F(CanvasAutomationAPITest, DifferentGridSizes) {
  // Test with 8x8 grid
  auto canvas_8x8 = std::make_unique<gui::Canvas>(
      "Test8x8", ImVec2(512, 512), gui::CanvasGridSize::k8x8);
  auto api_8x8 = canvas_8x8->GetAutomationAPI();
  
  auto dims = api_8x8->GetDimensions();
  EXPECT_EQ(dims.tile_size, 8);
  EXPECT_EQ(dims.width_tiles, 64);  // 512 / 8
  
  // Test with 32x32 grid
  auto canvas_32x32 = std::make_unique<gui::Canvas>(
      "Test32x32", ImVec2(512, 512), gui::CanvasGridSize::k32x32);
  auto api_32x32 = canvas_32x32->GetAutomationAPI();
  
  dims = api_32x32->GetDimensions();
  EXPECT_EQ(dims.tile_size, 32);
  EXPECT_EQ(dims.width_tiles, 16);  // 512 / 32
}

TEST_F(CanvasAutomationAPITest, MultipleZoomLevels) {
  float zoom_levels[] = {0.25f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f};
  
  for (float zoom : zoom_levels) {
    api_->SetZoom(zoom);
    float actual_zoom = api_->GetZoom();
    
    // Should be clamped to valid range
    EXPECT_GE(actual_zoom, 0.25f);
    EXPECT_LE(actual_zoom, 4.0f);
    
    // Coordinate conversion should still work
    ImVec2 canvas_pos = api_->TileToCanvas(10, 10);
    ImVec2 tile_pos = api_->CanvasToTile(canvas_pos);
    
    EXPECT_FLOAT_EQ(tile_pos.x, 10.0f);
    EXPECT_FLOAT_EQ(tile_pos.y, 10.0f);
  }
}

}  // namespace test
}  // namespace yaze

