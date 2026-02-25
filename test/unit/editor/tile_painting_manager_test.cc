#include "app/editor/overworld/tile_painting_manager.h"

#include <memory>

#include "app/editor/overworld/ui_constants.h"
#include "app/gui/canvas/canvas.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

// ---------------------------------------------------------------------------
// Test fixture providing minimal TilePaintingDependencies wiring.
//
// TilePaintingManager's public mode-toggling methods (ToggleBrushTool,
// ActivateFillTool) only dereference deps_.current_mode and call
// deps_.ow_map_canvas->SetUsageMode(). We wire those two pointers with a
// real Canvas + a local EditingMode. All other pointers are left null because
// mode toggling never touches them.
// ---------------------------------------------------------------------------
class TilePaintingManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    canvas_.Init("test_canvas", ImVec2(512, 512));
    current_mode_ = EditingMode::MOUSE;

    deps_.ow_map_canvas = &canvas_;
    deps_.current_mode = &current_mode_;

    callbacks_ = {};  // All callbacks null -- mode-toggle doesn't invoke them.
    manager_ =
        std::make_unique<TilePaintingManager>(deps_, callbacks_);
  }

  gui::Canvas canvas_;
  EditingMode current_mode_;
  TilePaintingDependencies deps_{};
  TilePaintingCallbacks callbacks_{};
  std::unique_ptr<TilePaintingManager> manager_;
};

// ===========================================================================
// ToggleBrushTool
// ===========================================================================

TEST_F(TilePaintingManagerTest, ToggleBrushFromMouseSwitchesToDrawTile) {
  current_mode_ = EditingMode::MOUSE;
  manager_->ToggleBrushTool();
  EXPECT_EQ(current_mode_, EditingMode::DRAW_TILE);
}

TEST_F(TilePaintingManagerTest, ToggleBrushFromDrawTileSwitchesToMouse) {
  current_mode_ = EditingMode::DRAW_TILE;
  manager_->ToggleBrushTool();
  EXPECT_EQ(current_mode_, EditingMode::MOUSE);
}

TEST_F(TilePaintingManagerTest, ToggleBrushFromFillTileSwitchesToDrawTile) {
  // Non-DRAW_TILE mode should switch to DRAW_TILE.
  current_mode_ = EditingMode::FILL_TILE;
  manager_->ToggleBrushTool();
  EXPECT_EQ(current_mode_, EditingMode::DRAW_TILE);
}

TEST_F(TilePaintingManagerTest, ToggleBrushRoundTrips) {
  current_mode_ = EditingMode::MOUSE;
  manager_->ToggleBrushTool();
  EXPECT_EQ(current_mode_, EditingMode::DRAW_TILE);
  manager_->ToggleBrushTool();
  EXPECT_EQ(current_mode_, EditingMode::MOUSE);
}

// ===========================================================================
// ActivateFillTool
// ===========================================================================

TEST_F(TilePaintingManagerTest, ActivateFillFromDrawTileSwitchesToFill) {
  current_mode_ = EditingMode::DRAW_TILE;
  manager_->ActivateFillTool();
  EXPECT_EQ(current_mode_, EditingMode::FILL_TILE);
}

TEST_F(TilePaintingManagerTest, ActivateFillFromFillSwitchesToDrawTile) {
  current_mode_ = EditingMode::FILL_TILE;
  manager_->ActivateFillTool();
  EXPECT_EQ(current_mode_, EditingMode::DRAW_TILE);
}

TEST_F(TilePaintingManagerTest, ActivateFillFromMouseSwitchesToFill) {
  current_mode_ = EditingMode::MOUSE;
  manager_->ActivateFillTool();
  EXPECT_EQ(current_mode_, EditingMode::FILL_TILE);
}

TEST_F(TilePaintingManagerTest, ActivateFillRoundTrips) {
  current_mode_ = EditingMode::DRAW_TILE;
  manager_->ActivateFillTool();
  EXPECT_EQ(current_mode_, EditingMode::FILL_TILE);
  manager_->ActivateFillTool();
  EXPECT_EQ(current_mode_, EditingMode::DRAW_TILE);
}

}  // namespace
}  // namespace yaze::editor
