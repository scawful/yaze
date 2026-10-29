#include "app/editor/overworld/canvas/canvas_navigation_manager.h"

#include <limits>
#include <memory>
#include <utility>

#include "app/editor/overworld/ui/ui_constants.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

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
    current_map_ = 0;
    current_world_ = 0;
    current_parent_ = 0;
    current_tile16_ = 0;

    ctx_.current_mode = &current_mode_;
    ctx_.current_map_lock = &current_map_lock_;
    ctx_.is_dragging_entity = &is_dragging_entity_;
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
  int current_map_;
  int current_world_;
  int current_parent_;
  int current_tile16_;
  std::unique_ptr<gui::TileSelectorWidget> blockset_selector_;
};

class CanvasMapTrackingTest : public CanvasNavigationManagerTest {
 protected:
  void SetUp() override {
    CanvasNavigationManagerTest::SetUp();
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    overworld_ = std::make_unique<zelda3::Overworld>(&rom_);
    auto& maps = const_cast<std::vector<zelda3::OverworldMap>&>(
        overworld_->overworld_maps());
    maps.reserve(zelda3::kNumOverworldMaps);
    for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
      maps.emplace_back(i, &rom_);
      maps.back().SetAsSmallMap(i);
    }
    ctx_.overworld = overworld_.get();
    ctx_.hovered_map = &hovered_map_;
    manager_.Initialize(ctx_, callbacks_);
  }
  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
  int hovered_map_ = -1;
};

TEST_F(CanvasMapTrackingTest, UnpinnedMapFollowsCursorInEveryEditMode) {
  int screen = 1;
  for (auto mode :
       {EditingMode::MOUSE, EditingMode::DRAW_TILE, EditingMode::FILL_TILE}) {
    current_mode_ = mode;
    ASSERT_EQ(manager_.TrackMapAtCanvasPosition(ImVec2(screen * 512 + 8, 8)),
              screen);
    EXPECT_EQ(current_map_, screen);
    EXPECT_EQ(hovered_map_, screen);
    EXPECT_EQ(current_parent_, screen);
    ++screen;
  }
}

TEST_F(CanvasMapTrackingTest, PinPreservesSelectionButHoverStillTracks) {
  current_map_lock_ = true;
  manager_.TrackMapAtCanvasPosition(ImVec2(520, 8));
  EXPECT_EQ(current_map_, 0);
  EXPECT_EQ(hovered_map_, 1);
  current_map_lock_ = false;
  manager_.TrackMapAtCanvasPosition(ImVec2(520, 8));
  EXPECT_EQ(current_map_, 1);
}

TEST_F(CanvasMapTrackingTest, ExplicitClickSelectsHoveredMapThroughPin) {
  current_map_lock_ = true;
  manager_.TrackMapAtCanvasPosition(ImVec2(520, 8));
  ASSERT_EQ(current_map_, 0);
  ASSERT_EQ(hovered_map_, 1);
  bool respected_pin = true;
  callbacks_.select_map_for_editing = [&](int map, bool respect_pin) {
    respected_pin = respect_pin;
    current_map_ = map;
  };
  manager_.Initialize(ctx_, callbacks_);
  EXPECT_TRUE(manager_.SelectMapUnderCursor());
  EXPECT_EQ(current_map_, 1);
  EXPECT_FALSE(respected_pin);
  EXPECT_TRUE(current_map_lock_);  // The pin now holds the clicked map.
}

TEST_F(CanvasMapTrackingTest, EntityDragPreservesSelectionUntilRelease) {
  is_dragging_entity_ = true;
  manager_.TrackMapAtCanvasPosition(ImVec2(520, 8));
  EXPECT_EQ(current_map_, 0);
  EXPECT_EQ(hovered_map_, 1);
  is_dragging_entity_ = false;
  manager_.TrackMapAtCanvasPosition(ImVec2(520, 8));
  EXPECT_EQ(current_map_, 1);
}

TEST_F(CanvasMapTrackingTest, RepeatedHoverDoesNotRepeatSelectionRefresh) {
  int selections = 0;
  callbacks_.select_map_for_editing = [&](int map, bool respect_pin) {
    EXPECT_TRUE(respect_pin);
    ++selections;
    current_map_ = map;
  };
  manager_.Initialize(ctx_, callbacks_);
  manager_.TrackMapAtCanvasPosition(ImVec2(520, 8));
  manager_.TrackMapAtCanvasPosition(ImVec2(530, 8));
  EXPECT_EQ(selections, 1);
}

TEST_F(CanvasMapTrackingTest, ZoomedCoordinatesRespectWorldAndScreenEdges) {
  canvas_.set_global_scale(0.5f);
  current_world_ = 1;
  manager_.TrackMapAtCanvasPosition(ImVec2(255.5f, 1));
  EXPECT_EQ(current_map_, 0x40);
  manager_.TrackMapAtCanvasPosition(ImVec2(256, 1));
  EXPECT_EQ(current_map_, 0x41);
  current_world_ = 2;
  manager_.TrackMapAtCanvasPosition(ImVec2(256, 256));
  EXPECT_EQ(current_map_, 0x89);
}

TEST_F(CanvasMapTrackingTest, ChildScreenKeepsPhysicalIdentityAndParentArea) {
  overworld_->mutable_overworld_map(9)->SetAsLargeMap(0, 3);
  manager_.TrackMapAtCanvasPosition(ImVec2(520, 520));
  EXPECT_EQ(current_map_, 9);
  EXPECT_EQ(hovered_map_, 9);
  EXPECT_EQ(current_parent_, 0);
}

TEST_F(CanvasMapTrackingTest, InvalidCoordinatesClearHoverWithoutRetargeting) {
  manager_.TrackMapAtCanvasPosition(ImVec2(520, 8));
  for (auto pos :
       {ImVec2(-0.5f, 8), ImVec2(8, -0.5f), ImVec2(4096, 8), ImVec2(8, 4096),
        ImVec2(std::numeric_limits<float>::quiet_NaN(), 8)}) {
    EXPECT_FALSE(manager_.TrackMapAtCanvasPosition(pos));
    EXPECT_EQ(hovered_map_, -1);
    EXPECT_EQ(current_map_, 1);
  }
}

TEST_F(CanvasMapTrackingTest, UnallocatedSpecialWorldRowsDoNotSelectAMap) {
  current_world_ = 2;
  manager_.TrackMapAtCanvasPosition(ImVec2(8, 8));
  ASSERT_EQ(current_map_, 0x80);
  EXPECT_FALSE(manager_.TrackMapAtCanvasPosition(ImVec2(8, 2048)));
  EXPECT_EQ(current_map_, 0x80);
  EXPECT_EQ(hovered_map_, -1);
}

TEST_F(CanvasMapTrackingTest, ProjectTerrainTracksWithoutChangingRomSelection) {
  callbacks_.displayed_map_count = [] {
    return 192;
  };
  callbacks_.is_project_terrain_map = [](int id) {
    return id >= 0xA0 && id < 0xC0;
  };
  manager_.Initialize(ctx_, callbacks_);
  current_world_ = 2;
  overworld_->set_current_map(0x80);
  overworld_->set_current_world(2);
  canvas_.set_global_scale(0.5f);

  for (int map : {0xA0, 0xA1, 0xA8, 0xBF}) {
    const int local = map - 0x80;
    EXPECT_EQ(manager_.TrackMapAtCanvasPosition(
                  ImVec2((local % 8) * 256 + 4, (local / 8) * 256 + 4)),
              map);
    EXPECT_EQ(current_map_, map);
    EXPECT_EQ(current_parent_, map);
    EXPECT_EQ(hovered_map_, map);
    EXPECT_EQ(overworld_->current_map_id(), 0x80);
    EXPECT_EQ(overworld_->overworld_map(map), nullptr);
  }
  current_map_lock_ = true;
  manager_.TrackMapAtCanvasPosition(ImVec2(4, 1028));
  EXPECT_EQ(current_map_, 0xBF);
  EXPECT_EQ(hovered_map_, 0xA0);
  EXPECT_TRUE(manager_.SelectMapUnderCursor());
  EXPECT_EQ(current_map_, 0xA0);
  EXPECT_EQ(overworld_->current_map_id(), 0x80);

  current_map_lock_ = false;
  manager_.TrackMapAtCanvasPosition(ImVec2(260, 4));
  EXPECT_EQ(current_map_, 0x81);
  EXPECT_EQ(overworld_->current_map_id(), 0x81);
  EXPECT_EQ(overworld_->inspected_map_count(), 160);
}

TEST_F(CanvasMapTrackingTest, DisplayCountAloneCannotSelectMissingRomMaps) {
  callbacks_.displayed_map_count = [] {
    return 192;
  };
  manager_.Initialize(ctx_, callbacks_);
  current_world_ = 2;
  EXPECT_FALSE(manager_.TrackMapAtCanvasPosition(ImVec2(8, 2048)));
  EXPECT_EQ(hovered_map_, -1);
  EXPECT_EQ(current_map_, 0);
}

TEST_F(CanvasMapTrackingTest, ProjectTerrainHoverSkipsRomBuildAndTextures) {
  ImGuiContext* context = ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(800, 700);
  io.DeltaTime = 1.0f / 60.0f;
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  auto bitmaps =
      std::make_unique<std::array<gfx::Bitmap, zelda3::kExpandedMapCount>>();
  ctx_.maps_bmp = bitmaps.get();
  ctx_.rom = &rom_;
  callbacks_.displayed_map_count = [] {
    return 192;
  };
  callbacks_.is_project_terrain_map = [](int id) {
    return id >= 0xA0 && id < 0xC0;
  };
  int texture_requests = 0;
  callbacks_.ensure_map_texture = [&](int) {
    ++texture_requests;
  };
  manager_.Initialize(ctx_, callbacks_);
  current_world_ = 2;
  overworld_->set_current_map(0x80);
  canvas_.set_global_scale(0.125f);
  // A0 is visible below the legacy four rows at this zoom. Exercise the
  // rendering path as well as tracking: it formerly dereferenced a null map.
  for (int frame = 0; frame < 3; ++frame) {
    io.MousePos = ImVec2(40, 300);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(700, 650));
    ImGui::Begin("Project terrain hover");
    canvas_.DrawBackground(ImVec2(4096, 4096));
    EXPECT_TRUE(manager_.CheckForCurrentMap().ok());
    if (frame == 2) {
      EXPECT_TRUE(canvas_.IsMouseHovering());
      EXPECT_EQ(current_map_, 0xA0);
      EXPECT_EQ(current_parent_, 0xA0);
    }
    ImGui::End();
    ImGui::Render();
  }
  EXPECT_EQ(texture_requests, 0);
  EXPECT_EQ(overworld_->current_map_id(), 0x80);
  EXPECT_EQ(overworld_->overworld_maps().size(), 160u);
  for (const auto& map : overworld_->overworld_maps()) {
    EXPECT_FALSE(map.is_built());
  }
  ImGui::DestroyContext(context);
}

TEST_F(CanvasNavigationManagerTest, ProjectTerrainCenterAndFitUseEightRows) {
  ImGuiContext* context = ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(1400, 1000);
  io.DeltaTime = 1.0f / 60.0f;
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  callbacks_.displayed_map_count = [] {
    return 192;
  };
  manager_.Initialize(ctx_, callbacks_);
  current_world_ = 2;
  manager_.CenterOnMap(0xBF);
  for (int frame = 0; frame < 3; ++frame) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1300, 900));
    ImGui::Begin("Project terrain navigation");
    manager_.BeginCanvasViewport();
    ImGui::BeginChild("Map viewport", ImVec2(1100, 700), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    manager_.EndCanvasViewport(false);
    ImGui::Dummy(ImVec2(4096, 4096));
    if (frame == 2) {
      const auto* window = ImGui::GetCurrentWindow();
      // BF reaches the bottom edge; centering clamps to the content bounds.
      EXPECT_NEAR(window->Scroll.y + window->InnerRect.GetHeight(), 4096.0f,
                  1.0f);
      manager_.ZoomToFit();
      EXPECT_NEAR(canvas_.global_scale(),
                  window->InnerRect.GetHeight() / 4096.0f, 0.001f);
    }
    ImGui::EndChild();
    ImGui::End();
    ImGui::Render();
  }
  ImGui::DestroyContext(context);
}

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
// Sticky wheel pan and zoom anchoring (pure math, no ImGui frame needed)
// ===========================================================================

TEST(StickyWheelPanTest, MouseNotchMovesOneConsistentStep) {
  StickyWheelPan pan;
  // One notch down at zoom 1: 64px in 8px detents.
  const ImVec2 first = pan.Consume(ImVec2(0, -1), 1 / 60.f, 64.f, 8.f);
  const ImVec2 second = pan.Consume(ImVec2(0, -1), 1 / 60.f, 64.f, 8.f);
  EXPECT_FLOAT_EQ(first.y, 64.f);
  EXPECT_FLOAT_EQ(second.y, 64.f);
  EXPECT_FLOAT_EQ(first.x, 0.f);
  // Wheel up scrolls back toward the origin.
  EXPECT_FLOAT_EQ(pan.Consume(ImVec2(0, 1), 1 / 60.f, 64.f, 8.f).y, -64.f);
}

TEST(StickyWheelPanTest, TrackpadDeltasMoveInWholeDetents) {
  StickyWheelPan pan;
  float moved = 0.f;
  // 0.05 units * 64px = 3.2px per frame: nothing moves until a full detent.
  moved += pan.Consume(ImVec2(0, -0.05f), 1 / 60.f, 64.f, 8.f).y;
  moved += pan.Consume(ImVec2(0, -0.05f), 1 / 60.f, 64.f, 8.f).y;
  EXPECT_FLOAT_EQ(moved, 0.f);
  moved += pan.Consume(ImVec2(0, -0.05f), 1 / 60.f, 64.f, 8.f).y;
  EXPECT_FLOAT_EQ(moved, 8.f);
  // Diagonal input moves both axes (ImGui's default picks only one).
  const ImVec2 both = pan.Consume(ImVec2(-1, -1), 1 / 60.f, 64.f, 8.f);
  EXPECT_GT(both.x, 0.f);
  EXPECT_GT(both.y, 0.f);
}

TEST(StickyWheelPanTest, MomentumTailAndIdleResidualAreDropped) {
  StickyWheelPan pan;
  // Below the deadzone: ignored entirely.
  EXPECT_FLOAT_EQ(pan.Consume(ImVec2(0, -0.01f), 1 / 60.f, 64.f, 8.f).y, 0.f);
  // Leave a 6.4px residual, then stop long enough for it to be discarded.
  EXPECT_FLOAT_EQ(pan.Consume(ImVec2(0, -0.1f), 1 / 60.f, 64.f, 8.f).y, 0.f);
  for (int i = 0; i < 10; ++i) {
    pan.Consume(ImVec2(0, 0), 1 / 60.f, 64.f, 8.f);
  }
  EXPECT_FLOAT_EQ(pan.residual.y, 0.f);
  // A reversal starts from zero instead of paying back leftover travel.
  pan.Consume(ImVec2(0, -0.1f), 1 / 60.f, 64.f, 8.f);
  EXPECT_FLOAT_EQ(pan.Consume(ImVec2(0, 0.2f), 1 / 60.f, 64.f, 8.f).y, -8.f);
}

TEST(ScrollForZoomAtAnchorTest, KeepsPointUnderAnchorFixed) {
  const ImVec2 scroll(100.f, 50.f);
  const ImVec2 anchor(200.f, 150.f);
  const ImVec2 zoomed = ScrollForZoomAtAnchor(scroll, anchor, 1.f, 2.f);
  // Content point under the anchor: (scroll + anchor) / scale.
  EXPECT_FLOAT_EQ((zoomed.x + anchor.x) / 2.f, (scroll.x + anchor.x) / 1.f);
  EXPECT_FLOAT_EQ((zoomed.y + anchor.y) / 2.f, (scroll.y + anchor.y) / 1.f);
  const ImVec2 same = ScrollForZoomAtAnchor(scroll, anchor, 1.f, 0.f);
  EXPECT_FLOAT_EQ(same.x, scroll.x);
}

TEST_F(CanvasNavigationManagerTest, ResetAndFitRequestsDoNotNeedAFrame) {
  canvas_.set_global_scale(2.5f);
  manager_.ResetOverworldView();
  EXPECT_FLOAT_EQ(canvas_.global_scale(), 1.0f);
  // Without a recorded viewport, fit leaves the scale alone.
  manager_.ZoomToFit();
  EXPECT_FLOAT_EQ(canvas_.global_scale(), 1.0f);
  manager_.CenterOverworldView();
  manager_.CenterOnMap(-1);
}

// ===========================================================================
// ScrollBlocksetCanvasToCurrentTile - null-safety
// ===========================================================================

TEST_F(CanvasNavigationManagerTest, ScrollBlocksetCanvasToCurrentTileNoWidget) {
  // blockset_selector_ is nullptr (default) -- should not crash.
  ASSERT_EQ(blockset_selector_, nullptr);
  manager_.ScrollBlocksetCanvasToCurrentTile();
}

TEST_F(CanvasNavigationManagerTest, StartupCenterWaitsForMeasuredViewport) {
  ImGuiContext* context = ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(800, 600);
  io.DeltaTime = 1.0f / 60.0f;
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  manager_.CenterOnMap(0x39);
  for (int frame = 0; frame < 3; ++frame) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(600, 500));
    ImGui::Begin("Startup map navigation");
    manager_.BeginCanvasViewport();
    ImGui::BeginChild("Map viewport", ImVec2(320, 240), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    manager_.EndCanvasViewport(false);
    ImGui::Dummy(ImVec2(4096, 4096));
    if (frame == 2) {
      const auto* window = ImGui::GetCurrentWindow();
      EXPECT_NEAR(window->Scroll.x + window->InnerRect.GetWidth() * 0.5f,
                  768.0f, 1.0f);
      EXPECT_NEAR(window->Scroll.y + window->InnerRect.GetHeight() * 0.5f,
                  3840.0f, 1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
    ImGui::Render();
  }
  ImGui::DestroyContext(context);
}

// Exercise the actual BeginChild scroll and extent handling without a renderer.
class CanvasWorldViewportTest : public CanvasNavigationManagerTest {
 protected:
  void SetUp() override {
    CanvasNavigationManagerTest::SetUp();
    imgui_context_ = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  void TearDown() override { ImGui::DestroyContext(imgui_context_); }

  ImVec2 DrawFrame(std::optional<ImVec2> scroll = std::nullopt,
                   ImVec2 size = ImVec2(320, 240)) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(700, 550));
    ImGui::Begin("World viewport memory");
    manager_.BeginCanvasViewport();
    ImGui::BeginChild("Map viewport", size, true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar |
                          ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    manager_.EndCanvasViewport(false);
    ImGui::Dummy(
        ImVec2(4096 * canvas_.global_scale(), 4096 * canvas_.global_scale()));
    const ImVec2 result = ImGui::GetCurrentWindow()->Scroll;
    if (scroll) {
      ImGui::SetScrollX(scroll->x);
      ImGui::SetScrollY(scroll->y);
    }
    ImGui::EndChild();
    ImGui::End();
    ImGui::Render();
    return result;
  }

  ImVec2 Settle(ImVec2 size = ImVec2(320, 240)) {
    DrawFrame(std::nullopt, size);
    DrawFrame(std::nullopt, size);
    return DrawFrame(std::nullopt, size);
  }

  void ScrollTo(ImVec2 scroll) {
    DrawFrame(scroll);
    const ImVec2 actual = Settle();
    EXPECT_FLOAT_EQ(actual.x, scroll.x);
    EXPECT_FLOAT_EQ(actual.y, scroll.y);
  }

  ImGuiContext* imgui_context_ = nullptr;
};

TEST_F(CanvasWorldViewportTest, EachWorldRestoresItsOwnScroll) {
  Settle();
  ScrollTo(ImVec2(3000, 2800));
  current_world_ = 1;
  ImVec2 scroll = Settle();
  EXPECT_FLOAT_EQ(scroll.x, 0);
  EXPECT_FLOAT_EQ(scroll.y, 0);
  ScrollTo(ImVec2(40, 80));
  current_world_ = 2;
  Settle();
  ScrollTo(ImVec2(1600, 2200));

  for (const auto& entry :
       {std::pair{0, ImVec2(3000, 2800)}, std::pair{1, ImVec2(40, 80)},
        std::pair{2, ImVec2(1600, 2200)}, std::pair{0, ImVec2(3000, 2800)}}) {
    current_world_ = entry.first;
    scroll = Settle();
    EXPECT_FLOAT_EQ(scroll.x, entry.second.x);
    EXPECT_FLOAT_EQ(scroll.y, entry.second.y);
  }
}

TEST_F(CanvasWorldViewportTest, SharedZoomPreservesEachWorldMapPosition) {
  Settle();
  ScrollTo(ImVec2(1000, 1200));
  current_world_ = 1;
  Settle();
  manager_.ZoomIn();
  Settle();
  ScrollTo(ImVec2(40, 80));
  const float scale = canvas_.global_scale();

  current_world_ = 0;
  const ImVec2 scroll = Settle();
  EXPECT_FLOAT_EQ(canvas_.global_scale(), scale);
  EXPECT_NEAR(scroll.x, 1000 * scale, 1.0f);
  EXPECT_NEAR(scroll.y, 1200 * scale, 1.0f);
  current_world_ = 1;
  const ImVec2 restored = Settle();
  EXPECT_FLOAT_EQ(restored.x, 40);
  EXPECT_FLOAT_EQ(restored.y, 80);
}

TEST_F(CanvasWorldViewportTest, ExplicitMapJumpOverridesWorldRestoration) {
  Settle();
  ScrollTo(ImVec2(3000, 2800));
  current_world_ = 1;
  current_map_ = 0x49;
  manager_.CenterOnMap(current_map_);
  const ImVec2 scroll = Settle();
  EXPECT_GT(scroll.x, 500);
  EXPECT_LT(scroll.x, 800);
  EXPECT_GT(scroll.y, 500);
  EXPECT_LT(scroll.y, 800);
  current_world_ = 0;
  EXPECT_FLOAT_EQ(Settle().x, 3000);
}

TEST_F(CanvasWorldViewportTest, ResetAffectsOnlyCurrentWorld) {
  Settle();
  ScrollTo(ImVec2(3000, 2800));
  current_world_ = 1;
  Settle();
  ScrollTo(ImVec2(40, 80));
  manager_.ResetOverworldView();
  const ImVec2 reset = Settle();
  EXPECT_FLOAT_EQ(reset.x, 0);
  EXPECT_FLOAT_EQ(reset.y, 0);
  current_world_ = 0;
  EXPECT_FLOAT_EQ(Settle().x, 3000);
  current_world_ = 1;
  EXPECT_FLOAT_EQ(Settle().x, 0);
}

TEST_F(CanvasWorldViewportTest, RestoreUsesNewMeasuredViewportBounds) {
  Settle();
  ScrollTo(ImVec2(3790, 3870));
  current_world_ = 1;
  Settle();
  current_world_ = 0;
  const ImVec2 larger = Settle(ImVec2(600, 450));
  EXPECT_GT(larger.x, 3400);
  EXPECT_LT(larger.x, 3600);
  EXPECT_GT(larger.y, 3600);
  EXPECT_LT(larger.y, 3800);
}

TEST_F(CanvasWorldViewportTest, ReinitializationClearsSessionMemory) {
  Settle();
  ScrollTo(ImVec2(3000, 2800));
  current_world_ = 1;
  Settle();
  ScrollTo(ImVec2(40, 80));
  manager_.Initialize(ctx_, callbacks_);
  const ImVec2 initial = Settle();
  EXPECT_FLOAT_EQ(initial.x, 0);
  EXPECT_FLOAT_EQ(initial.y, 0);
  current_world_ = 0;
  const ImVec2 scroll = Settle();
  EXPECT_FLOAT_EQ(scroll.x, 0);
  EXPECT_FLOAT_EQ(scroll.y, 0);
  current_world_ = 1;
  const ImVec2 returned = Settle();
  EXPECT_FLOAT_EQ(returned.x, 0);
  EXPECT_FLOAT_EQ(returned.y, 0);
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

// ---------------------------------------------------------------------------
// IsMapSelectClick: which clicks make the map under the cursor current.
// ---------------------------------------------------------------------------

TEST(MapSelectClickTest, PaintToolsSelectOnPlainRightClick) {
  for (EditingMode mode : {EditingMode::DRAW_TILE, EditingMode::FILL_TILE}) {
    MapClickInput input;
    input.mode = mode;
    input.right_clicked = true;
    EXPECT_TRUE(IsMapSelectClick(input));
    input.entity_hovered = true;  // Entities do not intercept paint tools.
    EXPECT_TRUE(IsMapSelectClick(input));
    input.shift = true;  // Shift+right-click opens the map menu instead.
    EXPECT_FALSE(IsMapSelectClick(input));

    MapClickInput left;
    left.mode = mode;
    left.left_released = true;  // Left-click paints; it never selects.
    EXPECT_FALSE(IsMapSelectClick(left));
  }
}

TEST(MapSelectClickTest, SelectToolSelectsOnLeftReleaseWithoutPan) {
  MapClickInput input;
  input.mode = EditingMode::MOUSE;
  input.left_released = true;
  input.left_press_owned = true;
  EXPECT_TRUE(IsMapSelectClick(input));
  input.left_dragged = true;  // A drag pans the view; keep the selection.
  EXPECT_FALSE(IsMapSelectClick(input));
  input.left_dragged = false;
  input.entity_hovered = true;  // Clicks on entities select the entity.
  EXPECT_FALSE(IsMapSelectClick(input));

  MapClickInput right;
  right.mode = EditingMode::MOUSE;
  right.right_clicked = true;  // Right-click opens the menu in Select mode.
  EXPECT_FALSE(IsMapSelectClick(right));
}

// A release selects only when its press also landed on the canvas, on the
// same map: not after a press that dismissed a popup or menu, or that began on
// another window or map, even without passing the drag threshold.
TEST(MapSelectClickTest, SelectToolReleaseNeedsCanvasOwnedPress) {
  MapClickInput input;
  input.mode = EditingMode::MOUSE;
  input.left_released = true;
  input.left_press_owned = false;
  EXPECT_FALSE(IsMapSelectClick(input));
  input.left_press_owned = true;
  EXPECT_TRUE(IsMapSelectClick(input));

  // Paint tools select on the right press itself; press ownership is moot.
  MapClickInput paint;
  paint.mode = EditingMode::DRAW_TILE;
  paint.right_clicked = true;
  EXPECT_TRUE(IsMapSelectClick(paint));
}

}  // namespace
}  // namespace yaze::editor
