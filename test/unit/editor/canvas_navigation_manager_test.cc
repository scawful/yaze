#include "app/editor/overworld/canvas_navigation_manager.h"

#include <memory>

#include "app/editor/overworld/ui_constants.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

// ---------------------------------------------------------------------------
// Test fixture for CanvasNavigationManager.
//
// Wires a real gui::Canvas for zoom/scale tests and lightweight stubs for
// callbacks. The Overworld pointer is left null in zoom-only tests since
// ZoomIn/ZoomOut/ResetOverworldView never dereference it.
// ---------------------------------------------------------------------------
class CanvasNavigationManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    canvas_.Init("test_nav_canvas", ImVec2(4096, 4096));
    // Set initial scale to 1.0
    canvas_.set_global_scale(1.0f);

    ctx_.ow_map_canvas = &canvas_;

    // Mode / lock state defaults
    current_mode_ = EditingMode::MOUSE;
    current_map_lock_ = false;
    is_dragging_entity_ = false;
    show_map_properties_panel_ = false;
    current_map_ = 0;
    current_world_ = 0;
    current_parent_ = 0;
    current_tile16_ = 0;

    ctx_.current_mode = &current_mode_;
    ctx_.current_map_lock = &current_map_lock_;
    ctx_.is_dragging_entity = &is_dragging_entity_;
    ctx_.show_map_properties_panel = &show_map_properties_panel_;
    ctx_.current_map = &current_map_;
    ctx_.current_world = &current_world_;
    ctx_.current_parent = &current_parent_;
    ctx_.current_tile16 = &current_tile16_;
    ctx_.blockset_selector = &blockset_selector_;

    manager_.Initialize(ctx_, callbacks_);
  }

  gui::Canvas canvas_;
  CanvasNavigationContext ctx_{};
  CanvasNavigationCallbacks callbacks_{};  // All null by default.
  CanvasNavigationManager manager_;

  // Mutable state backing
  EditingMode current_mode_;
  bool current_map_lock_;
  bool is_dragging_entity_;
  bool show_map_properties_panel_;
  int current_map_;
  int current_world_;
  int current_parent_;
  int current_tile16_;
  std::unique_ptr<gui::TileSelectorWidget> blockset_selector_;
};

// ===========================================================================
// ZoomIn / ZoomOut
// ===========================================================================

TEST_F(CanvasNavigationManagerTest, ZoomInIncrementsByStep) {
  float before = canvas_.global_scale();
  manager_.ZoomIn();
  EXPECT_FLOAT_EQ(canvas_.global_scale(), before + kOverworldZoomStep);
}

TEST_F(CanvasNavigationManagerTest, ZoomOutDecrementsByStep) {
  // Start at a scale > min so we can zoom out.
  canvas_.set_global_scale(2.0f);
  manager_.Initialize(ctx_, callbacks_);

  float before = canvas_.global_scale();
  manager_.ZoomOut();
  EXPECT_FLOAT_EQ(canvas_.global_scale(), before - kOverworldZoomStep);
}

TEST_F(CanvasNavigationManagerTest, ZoomInClampsToMax) {
  canvas_.set_global_scale(kOverworldMaxZoom);
  manager_.Initialize(ctx_, callbacks_);

  manager_.ZoomIn();
  EXPECT_FLOAT_EQ(canvas_.global_scale(), kOverworldMaxZoom);
}

TEST_F(CanvasNavigationManagerTest, ZoomOutClampsToMin) {
  canvas_.set_global_scale(kOverworldMinZoom);
  manager_.Initialize(ctx_, callbacks_);

  manager_.ZoomOut();
  EXPECT_FLOAT_EQ(canvas_.global_scale(), kOverworldMinZoom);
}

TEST_F(CanvasNavigationManagerTest, ZoomInFromJustBelowMaxClampsExactly) {
  float near_max = kOverworldMaxZoom - kOverworldZoomStep / 2.0f;
  canvas_.set_global_scale(near_max);
  manager_.Initialize(ctx_, callbacks_);

  manager_.ZoomIn();
  EXPECT_FLOAT_EQ(canvas_.global_scale(), kOverworldMaxZoom);
}

TEST_F(CanvasNavigationManagerTest, ZoomOutFromJustAboveMinClampsExactly) {
  float near_min = kOverworldMinZoom + kOverworldZoomStep / 2.0f;
  canvas_.set_global_scale(near_min);
  manager_.Initialize(ctx_, callbacks_);

  manager_.ZoomOut();
  EXPECT_FLOAT_EQ(canvas_.global_scale(), kOverworldMinZoom);
}

TEST_F(CanvasNavigationManagerTest, MultipleZoomInStepsAccumulate) {
  canvas_.set_global_scale(1.0f);
  manager_.Initialize(ctx_, callbacks_);

  manager_.ZoomIn();
  manager_.ZoomIn();
  manager_.ZoomIn();
  EXPECT_FLOAT_EQ(canvas_.global_scale(), 1.0f + 3.0f * kOverworldZoomStep);
}

// ===========================================================================
// NOTE: ResetOverworldView, CenterOverworldView, HandleOverworldPan,
// and CheckForMousePan all call ImGui::SetScrollX/Y or IsMouseDragging
// which require an active ImGui window context. Those are exercised in
// integration tests with a live ImGui frame, not here.
// ===========================================================================

// ===========================================================================
// ClampOverworldScroll - documented no-op
// ===========================================================================

TEST_F(CanvasNavigationManagerTest, ClampScrollDoesNotCrash) {
  manager_.ClampOverworldScroll();
}

// ===========================================================================
// HandleOverworldZoom - documented no-op
// ===========================================================================

TEST_F(CanvasNavigationManagerTest, HandleOverworldZoomDoesNotCrash) {
  manager_.HandleOverworldZoom();
}

// ===========================================================================
// ScrollBlocksetCanvasToCurrentTile - null-safety
// ===========================================================================

TEST_F(CanvasNavigationManagerTest,
       ScrollBlocksetCanvasToCurrentTileNoWidget) {
  // blockset_selector_ is nullptr (default) -- should not crash.
  ASSERT_EQ(blockset_selector_, nullptr);
  manager_.ScrollBlocksetCanvasToCurrentTile();
}

// ===========================================================================
// UpdateBlocksetSelectorState - null-safety
// ===========================================================================

TEST_F(CanvasNavigationManagerTest, UpdateBlocksetSelectorStateNoWidget) {
  // blockset_selector_ is nullptr -- should not crash.
  ASSERT_EQ(blockset_selector_, nullptr);
  manager_.UpdateBlocksetSelectorState();
}

// ===========================================================================
// Initialize
// ===========================================================================

TEST_F(CanvasNavigationManagerTest, InitializeSetsContext) {
  // After Initialize, zoom operations should work correctly -- we already
  // verified this above. This test explicitly checks a second Initialize
  // re-wires context (e.g. different canvas).
  gui::Canvas other_canvas;
  other_canvas.Init("other_canvas", ImVec2(128, 128));
  other_canvas.set_global_scale(2.5f);

  CanvasNavigationContext other_ctx = ctx_;
  other_ctx.ow_map_canvas = &other_canvas;

  manager_.Initialize(other_ctx, callbacks_);
  manager_.ZoomIn();
  EXPECT_FLOAT_EQ(other_canvas.global_scale(), 2.5f + kOverworldZoomStep);
  // Original canvas unchanged.
  EXPECT_FLOAT_EQ(canvas_.global_scale(), 1.0f);
}

}  // namespace
}  // namespace yaze::editor
