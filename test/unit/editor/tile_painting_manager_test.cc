#include "app/editor/overworld/painting/tile_painting_manager.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/overworld/ui/ui_constants.h"
#include "app/gfx/resource/arena.h"
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
    manager_ = std::make_unique<TilePaintingManager>(deps_, callbacks_);
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

// Exercise real canvas input and the production mutation path with synthetic
// world tiles. No ROM file or renderer is needed for these gesture tests.
class TilePaintingManagerGestureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gfx::Arena::Get().ClearTextureQueue();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigInputTrickleEventQueue = false;
    io.Fonts->AddFontDefault();
    unsigned char* font_pixels = nullptr;
    int font_width = 0;
    int font_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

    canvas_.Init("PaintGestureCanvas", ImVec2(256, 256));
    canvas_.GetConfig().is_draggable = false;
    overworld_ = std::make_unique<zelda3::Overworld>(&rom_);
    auto& tiles = *overworld_->mutable_map_tiles();
    tiles.light_world.assign(256, std::vector<uint16_t>(256, 0));
    tiles.dark_world = tiles.light_world;
    tiles.special_world = tiles.light_world;
    tiles.light_world[0][0] = 1;
    tiles.light_world[1][0] = 2;
    initial_tiles_ = tiles;

    blockset_.tile_size = {16, 16};
    blockset_.map_size = {5, 1};
    std::vector<uint8_t> atlas_pixels(80 * 16);
    for (int y = 0; y < 16; ++y) {
      for (int x = 0; x < 80; ++x) {
        atlas_pixels[y * 80 + x] = (x / 16) * 0x11;
      }
    }
    blockset_.atlas.Create(80, 16, 8, atlas_pixels);
    ASSERT_TRUE(blockset_.atlas.is_active());
    maps_ =
        std::make_unique<std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>>();
    initial_pixels_.assign(512 * 512, 0);
    InitializeMapBitmap(0);

    TilePaintingDependencies deps;
    deps.ow_map_canvas = &canvas_;
    deps.overworld = overworld_.get();
    deps.maps_bmp = maps_.get();
    deps.tile16_blockset = &blockset_;
    deps.current_tile16 = &current_tile_;
    deps.selected_tile16_ids = &selected_ids_;
    deps.current_map = &current_map_;
    deps.current_world = &current_world_;
    deps.current_mode = &current_mode_;
    deps.rom = &rom_;
    TilePaintingCallbacks callbacks;
    callbacks.create_undo_point = [this](int map, int world, int x, int y,
                                         int old_id) {
      undo_points_.push_back({map, world, x, y, old_id});
    };
    callbacks.finalize_paint_operation = [this] {
      ++finalize_count_;
    };
    callbacks.refresh_overworld_map_on_demand = [this](int map) {
      ++refresh_count_;
      refreshed_maps_.push_back(map);
    };
    callbacks.scroll_blockset_to_current_tile = [] {
    };
    manager_ = std::make_unique<TilePaintingManager>(deps, callbacks);

    // Establish the window before delivering input to its canvas item.
    Frame(ImVec2(-100, -100));
    Frame(ImVec2(-100, -100));
  }

  void TearDown() override {
    gfx::Arena::Get().ClearTextureQueue();
    manager_.reset();
    ImGui::DestroyContext(context_);
  }

  void Frame(ImVec2 mouse, bool left_down = false, bool right_down = false,
             const std::function<void()>& after_draw = {}) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(mouse.x, mouse.y);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, left_down);
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, right_down);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(80, 40), ImGuiCond_Always);
    ImGui::SetNextWindowSize(host_size_, ImGuiCond_Always);
    ImGui::Begin("PaintGestureHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    canvas_.DrawBackground(ImVec2(256, 256));
    manager_->CheckForOverworldEdits();
    if (after_draw) {
      after_draw();
    }
    ImGui::End();
    ImGui::Render();
  }

  ImVec2 At(float x, float y) const {
    return ImVec2(canvas_.zero_point().x + x, canvas_.zero_point().y + y);
  }

  ImVec2 Outside() const { return At(-60, 112); }

  void InitializeMapBitmap(int map) {
    (*maps_)[map].Create(512, 512, 8, initial_pixels_);
    ASSERT_TRUE((*maps_)[map].is_active());
    (*maps_)[map].set_modified(false);
  }

  void SetView(float scale, ImVec2 world_origin) {
    canvas_.set_global_scale(scale);
    canvas_.set_scrolling(
        ImVec2(-world_origin.x * scale, -world_origin.y * scale));
  }

  ImVec2 AtWorld(float x, float y) const {
    return At(x * canvas_.global_scale() + canvas_.scrolling().x,
              y * canvas_.global_scale() + canvas_.scrolling().y);
  }

  // Populate four distinct IDs and select them through the same right-drag
  // used by the editor. This supports both a 2x2 pattern and a 4x1 strip.
  void SelectFourTileBrush(int width = 2, int height = 2) {
    ASSERT_EQ(width * height, 4);
    auto& world = overworld_->mutable_map_tiles()->light_world;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        world[x][y] = y * width + x + 1;
      }
    }
    initial_tiles_ = *overworld_->mutable_map_tiles();
    const ImVec2 start = AtWorld(8, 8);
    const ImVec2 end = AtWorld((width - 1) * 16 + 8, (height - 1) * 16 + 8);
    Frame(start);
    Frame(start, false, true);
    Frame(end, false, true);
    Frame(end);
    ASSERT_TRUE(canvas_.select_rect_active());
    ASSERT_EQ(selected_ids_, (std::vector<int>{1, 2, 3, 4}));
    ExpectBrush(width, height, {1, 2, 3, 4});
    ExpectUnchanged();
  }

  void ExpectBrush(int width, int height, const std::vector<int>& ids) {
    const auto* brush = manager_->selection_brush();
    ASSERT_NE(brush, nullptr);
    EXPECT_EQ(brush->width, width);
    EXPECT_EQ(brush->height, height);
    EXPECT_EQ(brush->tile_ids, ids);
  }

  absl::Status PasteAt(ImVec2 mouse, const TileBrush& brush) {
    absl::Status result = absl::UnknownError("Paste callback did not run");
    Frame(mouse, false, false, [&] { result = manager_->PasteBrush(brush); });
    return result;
  }

  void ClickWorld(float x, float y) {
    const ImVec2 mouse = AtWorld(x, y);
    Frame(mouse);
    Frame(mouse, true);
    ASSERT_TRUE(canvas_.IsMouseHovering());
    Frame(mouse);
  }

  void ExpectPreview(float x, float y, float last_x, float last_y) {
    const auto points = canvas_.selected_points();
    ASSERT_EQ(points.size(), 2);
    EXPECT_FLOAT_EQ(points[0].x, x);
    EXPECT_FLOAT_EQ(points[0].y, y);
    EXPECT_FLOAT_EQ(points[1].x, last_x);
    EXPECT_FLOAT_EQ(points[1].y, last_y);
  }

  void SelectTwoTileBrush() {
    Frame(At(8, 8));
    Frame(At(8, 8), false, true);
    Frame(At(24, 8), false, true);
    Frame(At(24, 8));
    ASSERT_TRUE(canvas_.select_rect_active());
    ASSERT_EQ(selected_ids_, (std::vector<int>{1, 2}));
    ExpectUnchanged();
  }

  void ExpectUnchanged() {
    const auto& tiles = *overworld_->mutable_map_tiles();
    EXPECT_EQ(tiles.light_world, initial_tiles_.light_world);
    EXPECT_EQ(tiles.dark_world, initial_tiles_.dark_world);
    EXPECT_EQ(tiles.special_world, initial_tiles_.special_world);
    EXPECT_EQ((*maps_)[0].vector(), initial_pixels_);
    EXPECT_FALSE(rom_.dirty());
    EXPECT_TRUE(undo_points_.empty());
    EXPECT_EQ(finalize_count_, 0);
    EXPECT_EQ(refresh_count_, 0);
  }

  ImGuiContext* context_ = nullptr;
  ImVec2 host_size_ = ImVec2(400, 400);
  gui::Canvas canvas_;
  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
  std::unique_ptr<std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>> maps_;
  gfx::Tilemap blockset_{};
  zelda3::OverworldMapTiles initial_tiles_;
  std::vector<uint8_t> initial_pixels_;
  std::vector<int> selected_ids_;
  std::vector<std::array<int, 5>> undo_points_;
  std::vector<int> refreshed_maps_;
  int current_tile_ = 2;
  int current_map_ = 0;
  int current_world_ = 0;
  EditingMode current_mode_ = EditingMode::DRAW_TILE;
  int finalize_count_ = 0;
  int refresh_count_ = 0;
  std::unique_ptr<TilePaintingManager> manager_;
};

TEST_F(TilePaintingManagerGestureTest,
       OutsideClickAndDragLeaveActiveBrushUntouched) {
  SelectTwoTileBrush();
  Frame(Outside(), true);
  Frame(At(-60, 160), true);
  Frame(At(-60, 160));
  EXPECT_FALSE(canvas_.IsMouseHovering());
  ExpectUnchanged();
}

TEST_F(TilePaintingManagerGestureTest,
       DragStartingOutsideCannotStampAfterEnteringCanvas) {
  SelectTwoTileBrush();
  Frame(Outside(), true);
  Frame(At(128, 128), true);
  Frame(At(160, 128), true);
  Frame(At(160, 128));
  ExpectUnchanged();
}

TEST_F(TilePaintingManagerGestureTest, CanvasClickAndDragStampSelectedTiles) {
  SelectTwoTileBrush();
  Frame(At(128, 128));
  Frame(At(128, 128), true);
  ASSERT_TRUE(canvas_.IsMouseHovering());
  Frame(At(160, 128), true);
  Frame(At(160, 128));

  const auto& world = overworld_->mutable_map_tiles()->light_world;
  EXPECT_EQ(world[8][8], 1);
  EXPECT_EQ(world[9][8], 2);
  EXPECT_EQ(world[10][8], 1);
  EXPECT_EQ(world[11][8], 2);
  EXPECT_EQ(world[0][0], 1);
  EXPECT_EQ(world[1][0], 2);
  EXPECT_EQ((*maps_)[0].vector()[128 * 512 + 128], 0x11);
  EXPECT_EQ((*maps_)[0].vector()[128 * 512 + 144], 0x22);
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(undo_points_.size(), 4);
  EXPECT_EQ(finalize_count_, 1);
  EXPECT_EQ(refresh_count_, 2);
}

TEST_F(TilePaintingManagerGestureTest,
       CanvasDragPausesOutsideAndResumesOnReturn) {
  SelectTwoTileBrush();
  Frame(At(128, 128));
  Frame(At(128, 128), true);
  const auto after_click = overworld_->mutable_map_tiles()->light_world;
  const auto pixels_after_click = (*maps_)[0].vector();
  ASSERT_EQ(undo_points_.size(), 2);

  Frame(Outside(), true);
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world, after_click);
  EXPECT_EQ((*maps_)[0].vector(), pixels_after_click);
  EXPECT_EQ(undo_points_.size(), 2);
  EXPECT_EQ(finalize_count_, 0);
  EXPECT_EQ(refresh_count_, 1);

  Frame(At(160, 128), true);
  Frame(At(160, 128));
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world[10][8], 1);
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world[11][8], 2);
  EXPECT_EQ(undo_points_.size(), 4);
  EXPECT_EQ(finalize_count_, 1);
}

TEST_F(TilePaintingManagerGestureTest,
       ReleaseOutsideDoesNotAuthorizeNextOutsideDrag) {
  SelectTwoTileBrush();
  Frame(At(128, 128));
  Frame(At(128, 128), true);
  Frame(Outside());
  const auto after_click = overworld_->mutable_map_tiles()->light_world;
  const auto pixels_after_click = (*maps_)[0].vector();
  ASSERT_EQ(undo_points_.size(), 2);
  rom_.ClearDirty();

  Frame(Outside(), true);
  Frame(At(160, 128), true);
  Frame(At(160, 128));
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world, after_click);
  EXPECT_EQ((*maps_)[0].vector(), pixels_after_click);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_EQ(undo_points_.size(), 2);
  EXPECT_EQ(finalize_count_, 1);
  EXPECT_EQ(refresh_count_, 1);
}

TEST_F(TilePaintingManagerGestureTest, OutsideDragCannotPaintSingleTiles) {
  ASSERT_FALSE(canvas_.select_rect_active());
  Frame(Outside(), true);
  Frame(At(128, 128), true);
  Frame(At(160, 128), true);
  Frame(At(160, 128));
  ExpectUnchanged();
}

TEST_F(TilePaintingManagerGestureTest, CanvasSingleTileClickAndDragStillPaint) {
  Frame(At(128, 128));
  Frame(At(128, 128), true);
  Frame(At(160, 128), true);
  Frame(At(160, 128));
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world[8][8], 2);
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world[10][8], 2);
  EXPECT_EQ((*maps_)[0].vector()[128 * 512 + 128], 0x22);
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(undo_points_.size(), 2);
}

TEST_F(TilePaintingManagerGestureTest, FillOnlyRespondsToCanvasClick) {
  current_mode_ = EditingMode::FILL_TILE;
  Frame(Outside(), true);
  Frame(At(128, 128), true);
  Frame(At(128, 128));
  ExpectUnchanged();

  Frame(At(128, 128), true);
  Frame(At(128, 128));
  const auto& world = overworld_->mutable_map_tiles()->light_world;
  for (int x = 0; x < 32; ++x) {
    for (int y = 0; y < 32; ++y) {
      ASSERT_EQ(world[x][y], 2) << "Tile " << x << "," << y;
    }
  }
  EXPECT_EQ(world[32][0], 0);
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(undo_points_.size(), 1023);
  EXPECT_EQ(finalize_count_, 1);
  EXPECT_EQ(refresh_count_, 1);
}

TEST_F(TilePaintingManagerGestureTest,
       FillDistantMapRepeatsCapturedSourceInsteadOfDestination) {
  SelectFourTileBrush();
  InitializeMapBitmap(9);
  auto& world = overworld_->mutable_map_tiles()->light_world;
  // The cursor destination contains the reverse of the selected pattern.
  world[34][34] = 4;
  world[35][34] = 3;
  world[34][35] = 2;
  world[35][35] = 1;
  auto expected = world;
  for (int y = 32; y < 64; ++y) {
    for (int x = 32; x < 64; ++x) {
      expected[x][y] = (y % 2) * 2 + (x % 2) + 1;
    }
  }

  current_mode_ = EditingMode::FILL_TILE;
  SetView(1.0f, ImVec2(512, 512));
  ClickWorld(552, 552);

  EXPECT_EQ(world, expected);
  EXPECT_EQ(selected_ids_, (std::vector<int>{1, 2, 3, 4}));
  EXPECT_EQ(overworld_->mutable_map_tiles()->dark_world,
            initial_tiles_.dark_world);
  EXPECT_EQ(overworld_->mutable_map_tiles()->special_world,
            initial_tiles_.special_world);
  EXPECT_EQ(current_map_, 9);
  EXPECT_TRUE((*maps_)[9].modified());
  EXPECT_EQ((*maps_)[9].vector()[0], 0x11);
  EXPECT_EQ((*maps_)[9].vector()[16], 0x22);
  EXPECT_EQ((*maps_)[9].vector()[16 * 512], 0x33);
  EXPECT_EQ((*maps_)[9].vector()[16 * 512 + 16], 0x44);
  EXPECT_EQ((*maps_)[0].vector(), initial_pixels_);
  EXPECT_TRUE(rom_.dirty());
  ASSERT_EQ(undo_points_.size(), 1024);
  for (const auto& point : undo_points_) {
    EXPECT_EQ(point[0], 9);
    EXPECT_EQ(point[1], 0);
    EXPECT_GE(point[2], 32);
    EXPECT_LT(point[2], 64);
    EXPECT_GE(point[3], 32);
    EXPECT_LT(point[3], 64);
  }
  EXPECT_EQ(finalize_count_, 1);
  EXPECT_EQ(refreshed_maps_, (std::vector<int>{9}));
}

TEST_F(TilePaintingManagerGestureTest,
       OverlappingStampKeepsCapturedBrushForNextPlacement) {
  SelectFourTileBrush();
  ClickWorld(24, 8);
  auto& world = overworld_->mutable_map_tiles()->light_world;
  ASSERT_EQ(world[1][0], 1);
  ASSERT_EQ(world[1][1], 3);
  EXPECT_EQ(selected_ids_, (std::vector<int>{1, 2, 3, 4}));
  ExpectBrush(2, 2, {1, 2, 3, 4});

  auto expected = world;
  expected[8][8] = 1;
  expected[9][8] = 2;
  expected[8][9] = 3;
  expected[9][9] = 4;
  ClickWorld(136, 136);

  EXPECT_EQ(world, expected);
  EXPECT_EQ(selected_ids_, (std::vector<int>{1, 2, 3, 4}));
  ExpectBrush(2, 2, {1, 2, 3, 4});
  EXPECT_EQ((*maps_)[0].vector()[128 * 512 + 128], 0x11);
  EXPECT_EQ((*maps_)[0].vector()[128 * 512 + 144], 0x22);
  EXPECT_EQ((*maps_)[0].vector()[144 * 512 + 128], 0x33);
  EXPECT_EQ((*maps_)[0].vector()[144 * 512 + 144], 0x44);
  EXPECT_EQ(undo_points_.size(), 8);
  EXPECT_EQ(finalize_count_, 2);
}

TEST_F(TilePaintingManagerGestureTest,
       ReselectingSameSourceCapturesItsCurrentTiles) {
  SelectFourTileBrush();
  ClickWorld(24, 8);
  // The overlapping stamp changed the original source to [1,1;3,3]. A new
  // selection at those same coordinates must replace the old snapshot.
  Frame(AtWorld(8, 8), false, true);
  Frame(AtWorld(24, 24), false, true);
  Frame(AtWorld(24, 24));
  ASSERT_EQ(selected_ids_, (std::vector<int>{1, 1, 3, 3}));

  ClickWorld(136, 136);
  const auto& world = overworld_->mutable_map_tiles()->light_world;
  EXPECT_EQ(world[8][8], 1);
  EXPECT_EQ(world[9][8], 1);
  EXPECT_EQ(world[8][9], 3);
  EXPECT_EQ(world[9][9], 3);
}

class TilePaintingManagerZoomGestureTest
    : public TilePaintingManagerGestureTest,
      public ::testing::WithParamInterface<float> {};

TEST_P(TilePaintingManagerZoomGestureTest,
       PreviewAndStampUseSameWorldCoordinates) {
  SelectFourTileBrush(4, 1);
  SetView(GetParam(), ImVec2(128, 128));
  const ImVec2 mouse = AtWorld(250, 249);
  Frame(mouse);
  ASSERT_TRUE(canvas_.IsMouseHovering());
  // One hover frame must move the preview to the destination. Four tile
  // origins occupy x=240,256,272,288 irrespective of the viewport zoom.
  ExpectPreview(240, 240, 288, 240);
  ExpectBrush(4, 1, {1, 2, 3, 4});
  auto expected = overworld_->mutable_map_tiles()->light_world;
  expected[15][15] = 1;
  expected[16][15] = 2;
  expected[17][15] = 3;
  expected[18][15] = 4;

  Frame(mouse, true);
  Frame(mouse);

  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world, expected);
  ExpectPreview(240, 240, 288, 240);
  EXPECT_EQ((*maps_)[0].vector()[240 * 512 + 240], 0x11);
  EXPECT_EQ((*maps_)[0].vector()[240 * 512 + 256], 0x22);
  EXPECT_EQ((*maps_)[0].vector()[240 * 512 + 272], 0x33);
  EXPECT_EQ((*maps_)[0].vector()[240 * 512 + 288], 0x44);
  EXPECT_EQ(undo_points_, (std::vector<std::array<int, 5>>{{0, 0, 15, 15, 0},
                                                           {0, 0, 16, 15, 0},
                                                           {0, 0, 17, 15, 0},
                                                           {0, 0, 18, 15, 0}}));
}

INSTANTIATE_TEST_SUITE_P(ZoomLevels, TilePaintingManagerZoomGestureTest,
                         ::testing::Values(0.5f, 1.0f, 2.0f));

TEST_F(TilePaintingManagerGestureTest,
       StampAcrossFourScreensWritesEachDestinationBitmapAndUndoMap) {
  SelectFourTileBrush();
  InitializeMapBitmap(1);
  InitializeMapBitmap(8);
  InitializeMapBitmap(9);
  SetView(1.0f, ImVec2(384, 384));
  Frame(AtWorld(503, 503));
  ExpectPreview(496, 496, 512, 512);
  auto expected = overworld_->mutable_map_tiles()->light_world;
  expected[31][31] = 1;
  expected[32][31] = 2;
  expected[31][32] = 3;
  expected[32][32] = 4;

  ClickWorld(503, 503);

  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world, expected);
  EXPECT_EQ((*maps_)[0].vector()[496 * 512 + 496], 0x11);
  EXPECT_EQ((*maps_)[1].vector()[496 * 512], 0x22);
  EXPECT_EQ((*maps_)[8].vector()[496], 0x33);
  EXPECT_EQ((*maps_)[9].vector()[0], 0x44);
  // The three other screen corners must not be painted into map zero.
  EXPECT_EQ((*maps_)[0].vector()[496 * 512], 0);
  EXPECT_EQ((*maps_)[0].vector()[496], 0);
  EXPECT_EQ((*maps_)[0].vector()[0], 0);
  EXPECT_EQ(undo_points_, (std::vector<std::array<int, 5>>{{0, 0, 31, 31, 0},
                                                           {1, 0, 32, 31, 0},
                                                           {8, 0, 31, 32, 0},
                                                           {9, 0, 32, 32, 0}}));
  EXPECT_EQ(finalize_count_, 1);
  EXPECT_TRUE(rom_.dirty());
}

TEST_F(TilePaintingManagerGestureTest,
       WorldBottomRightClipsOverflowCellsWithoutMovingOrWrappingStamp) {
  SelectFourTileBrush();
  InitializeMapBitmap(63);
  SetView(1.0f, ImVec2(3968, 3968));
  Frame(AtWorld(4095, 4095));
  ExpectPreview(4080, 4080, 4096, 4096);
  auto expected = overworld_->mutable_map_tiles()->light_world;
  expected[255][255] = 1;

  ClickWorld(4095, 4095);

  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world, expected);
  EXPECT_EQ((*maps_)[63].vector()[496 * 512 + 496], 0x11);
  EXPECT_EQ((*maps_)[0].vector(), initial_pixels_);
  EXPECT_EQ(selected_ids_, (std::vector<int>{1, 2, 3, 4}));
  EXPECT_EQ(undo_points_,
            (std::vector<std::array<int, 5>>{{63, 0, 255, 255, 0}}));
  EXPECT_EQ(finalize_count_, 1);
}

TEST_F(TilePaintingManagerGestureTest,
       NegativeOriginClipsCellsAndKeepsTheirPatternOffsets) {
  SelectFourTileBrush();
  SetView(1.0f, ImVec2(-128, -128));
  Frame(AtWorld(-8, -8));
  ExpectPreview(-16, -16, 0, 0);
  auto expected = overworld_->mutable_map_tiles()->light_world;
  expected[0][0] = 4;

  ClickWorld(-8, -8);

  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world, expected);
  EXPECT_EQ((*maps_)[0].vector()[0], 0x44);
  EXPECT_EQ(selected_ids_, (std::vector<int>{1, 2, 3, 4}));
  EXPECT_EQ(undo_points_, (std::vector<std::array<int, 5>>{{0, 0, 0, 0, 1}}));
  EXPECT_EQ(finalize_count_, 1);
}

TEST_F(TilePaintingManagerGestureTest,
       PasteUsesHoveredSeamInsteadOfStaleMapAndDrawnPosition) {
  // Leave the legacy single-tile destination at (64,64), then move to a
  // different world position without another left press.
  ClickWorld(72, 72);
  ASSERT_FLOAT_EQ(canvas_.drawn_tile_position().x, 64);
  ASSERT_FLOAT_EQ(canvas_.drawn_tile_position().y, 64);
  ASSERT_EQ(overworld_->mutable_map_tiles()->light_world[4][4], 2);
  undo_points_.clear();
  finalize_count_ = 0;
  refresh_count_ = 0;
  refreshed_maps_.clear();
  rom_.ClearDirty();

  InitializeMapBitmap(1);
  InitializeMapBitmap(8);
  InitializeMapBitmap(9);
  SetView(1.0f, ImVec2(384, 384));
  current_map_ = 42;
  auto expected = overworld_->mutable_map_tiles()->light_world;
  expected[31][31] = 1;
  expected[32][31] = 2;
  expected[31][32] = 3;
  expected[32][32] = 4;

  const auto status = PasteAt(AtWorld(503, 503), TileBrush{2, 2, {1, 2, 3, 4}});

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world, expected);
  EXPECT_EQ(current_map_, 0);
  EXPECT_EQ((*maps_)[0].vector()[496 * 512 + 496], 0x11);
  EXPECT_EQ((*maps_)[1].vector()[496 * 512], 0x22);
  EXPECT_EQ((*maps_)[8].vector()[496], 0x33);
  EXPECT_EQ((*maps_)[9].vector()[0], 0x44);
  EXPECT_EQ(undo_points_, (std::vector<std::array<int, 5>>{{0, 0, 31, 31, 0},
                                                           {1, 0, 32, 31, 0},
                                                           {8, 0, 31, 32, 0},
                                                           {9, 0, 32, 32, 0}}));
  EXPECT_EQ(refreshed_maps_, (std::vector<int>{0, 1, 8, 9}));
  EXPECT_EQ(finalize_count_, 1);
  EXPECT_TRUE(rom_.dirty());
}

TEST_F(TilePaintingManagerGestureTest,
       InvalidPasteBrushesLeaveTilesHistoryAndSelectionUnchanged) {
  SelectFourTileBrush();
  const std::vector<TileBrush> invalid_brushes{
      {0, 1, {}},        {1, 0, {}},
      {-1, 1, {1}},      {2, 2, {1, 2, 3}},
      {1, 1, {1, 2}},    {1, 1, {-1}},
      {1, 1, {0x10000}}, {257, 1, std::vector<int>(257, 1)},
  };
  for (const auto& brush : invalid_brushes) {
    SCOPED_TRACE(::testing::Message()
                 << "Dimensions " << brush.width << "x" << brush.height
                 << ", entries " << brush.tile_ids.size());
    const auto status = PasteAt(AtWorld(136, 136), brush);
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
    ExpectUnchanged();
    ExpectBrush(2, 2, {1, 2, 3, 4});
  }
}

TEST_F(TilePaintingManagerGestureTest, PasteOutsideCanvasDoesNotEdit) {
  SelectFourTileBrush();
  const auto status = PasteAt(Outside(), TileBrush{2, 2, {1, 2, 3, 4}});
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  ExpectUnchanged();
  ExpectBrush(2, 2, {1, 2, 3, 4});
}

TEST_F(TilePaintingManagerGestureTest,
       SpecialWorldLastScreenClipsPaddedRowsWithoutWrapping) {
  SelectFourTileBrush();
  InitializeMapBitmap(159);
  current_world_ = 2;
  current_map_ = 128;
  SetView(1.0f, ImVec2(3968, 1920));
  Frame(AtWorld(4095, 2047));
  ExpectPreview(4080, 2032, 4096, 2048);
  auto expected = overworld_->mutable_map_tiles()->special_world;
  expected[255][127] = 1;

  ClickWorld(4095, 2047);

  EXPECT_EQ(overworld_->mutable_map_tiles()->special_world, expected);
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world,
            initial_tiles_.light_world);
  EXPECT_EQ(overworld_->mutable_map_tiles()->dark_world,
            initial_tiles_.dark_world);
  EXPECT_EQ((*maps_)[159].vector()[496 * 512 + 496], 0x11);
  EXPECT_EQ((*maps_)[0].vector(), initial_pixels_);
  EXPECT_EQ(current_map_, 159);
  ExpectBrush(2, 2, {1, 2, 3, 4});
  EXPECT_EQ(undo_points_,
            (std::vector<std::array<int, 5>>{{159, 2, 255, 127, 0}}));
  EXPECT_EQ(refreshed_maps_, (std::vector<int>{159}));
  EXPECT_EQ(finalize_count_, 1);
}

TEST_F(TilePaintingManagerGestureTest,
       ZoomedPreviewDrawCommandsCoverTheActualStampFootprint) {
  SelectFourTileBrush();
  host_size_ = ImVec2(600, 540);
  SetView(2.0f, ImVec2(0, 0));

  // ImGui only records this identity in draw commands. No graphics backend is
  // invoked, and the fake handle is removed before the bitmap can be destroyed.
  int texture_tag = 0;
  struct ScopedTextureReset {
    gfx::Bitmap& bitmap;
    ~ScopedTextureReset() { bitmap.set_texture(nullptr); }
  } reset{blockset_.atlas};
  blockset_.atlas.set_texture(&texture_tag);
  const ImTextureID preview_texture = (ImTextureID)(intptr_t)&texture_tag;
  Frame(AtWorld(168, 168));
  blockset_.atlas.set_texture(nullptr);

  const ImVec2 top_left = AtWorld(160, 160);
  const ImVec2 bottom_right = AtWorld(192, 192);
  const auto* draw_data = ImGui::GetDrawData();
  ASSERT_NE(draw_data, nullptr);
  int image_indices = 0;
  ImVec2 min_vertex(std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max());
  ImVec2 max_vertex(std::numeric_limits<float>::lowest(),
                    std::numeric_limits<float>::lowest());
  for (const auto* draw_list : draw_data->CmdLists) {
    for (const auto& command : draw_list->CmdBuffer) {
      // This fixture has no renderer to upload the font atlas. Its commands
      // cannot use ImDrawCmd::GetTexID until a backend assigns a texture ID.
      if (command.TexRef._TexData != nullptr &&
          command.TexRef._TexData->GetTexID() == ImTextureID_Invalid) {
        continue;
      }
      if (command.GetTexID() != preview_texture) {
        continue;
      }
      EXPECT_LE(command.ClipRect.x, top_left.x);
      EXPECT_LE(command.ClipRect.y, top_left.y);
      EXPECT_GE(command.ClipRect.z, bottom_right.x);
      EXPECT_GE(command.ClipRect.w, bottom_right.y);
      image_indices += command.ElemCount;
      for (unsigned int i = 0; i < command.ElemCount; ++i) {
        const auto vertex_index =
            command.VtxOffset + draw_list->IdxBuffer[command.IdxOffset + i];
        const ImVec2 position = draw_list->VtxBuffer[vertex_index].pos;
        min_vertex.x = std::min(min_vertex.x, position.x);
        min_vertex.y = std::min(min_vertex.y, position.y);
        max_vertex.x = std::max(max_vertex.x, position.x);
        max_vertex.y = std::max(max_vertex.y, position.y);
      }
    }
  }
  ASSERT_EQ(image_indices, 24);  // Four image quads, six indices each.
  EXPECT_FLOAT_EQ(min_vertex.x, top_left.x);
  EXPECT_FLOAT_EQ(min_vertex.y, top_left.y);
  EXPECT_FLOAT_EQ(max_vertex.x, bottom_right.x);
  EXPECT_FLOAT_EQ(max_vertex.y, bottom_right.y);
  ExpectPreview(160, 160, 176, 176);
  ExpectUnchanged();

  ClickWorld(168, 168);
  const auto& world = overworld_->mutable_map_tiles()->light_world;
  EXPECT_EQ(world[10][10], 1);
  EXPECT_EQ(world[11][10], 2);
  EXPECT_EQ(world[10][11], 3);
  EXPECT_EQ(world[11][11], 4);
}

TEST_F(TilePaintingManagerGestureTest,
       SingleTileHeldStrokeFinalizesOneUndoBatchOnRelease) {
  Frame(At(128, 128));
  Frame(At(128, 128), true);
  Frame(At(160, 128), true);
  Frame(At(192, 128), true);
  EXPECT_EQ(undo_points_.size(), 3);
  EXPECT_EQ(finalize_count_, 0);
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world[8][8], 2);
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world[10][8], 2);
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world[12][8], 2);

  Frame(At(192, 128));
  EXPECT_EQ(finalize_count_, 1);
  Frame(At(192, 128));
  EXPECT_EQ(finalize_count_, 1);
}

}  // namespace
}  // namespace yaze::editor
