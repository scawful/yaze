#include "app/editor/overworld/painting/tile_painting_manager.h"

#include <memory>
#include <vector>

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
    tiles.light_world.assign(64, std::vector<uint16_t>(64, 0));
    tiles.dark_world = tiles.light_world;
    tiles.special_world = tiles.light_world;
    tiles.light_world[0][0] = 1;
    tiles.light_world[1][0] = 2;
    initial_tiles_ = tiles;

    blockset_.tile_size = {16, 16};
    blockset_.map_size = {3, 1};
    std::vector<uint8_t> atlas_pixels(48 * 16);
    for (int y = 0; y < 16; ++y) {
      for (int x = 0; x < 48; ++x) {
        atlas_pixels[y * 48 + x] = (x / 16) * 0x11;
      }
    }
    blockset_.atlas.Create(48, 16, 8, atlas_pixels);
    ASSERT_TRUE(blockset_.atlas.is_active());
    maps_ =
        std::make_unique<std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>>();
    initial_pixels_.assign(512 * 512, 0);
    (*maps_)[0].Create(512, 512, 8, initial_pixels_);
    ASSERT_TRUE((*maps_)[0].is_active());
    (*maps_)[0].set_modified(false);

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
    callbacks.refresh_overworld_map = [this] {
      ++refresh_count_;
    };
    callbacks.refresh_overworld_map_on_demand = [this](int) {
      ++refresh_count_;
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

  void Frame(ImVec2 mouse, bool left_down = false, bool right_down = false) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(mouse.x, mouse.y);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, left_down);
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, right_down);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(80, 40), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_Always);
    ImGui::Begin("PaintGestureHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    canvas_.DrawBackground(ImVec2(256, 256));
    manager_->CheckForOverworldEdits();
    ImGui::End();
    ImGui::Render();
  }

  ImVec2 At(float x, float y) const {
    return ImVec2(canvas_.zero_point().x + x, canvas_.zero_point().y + y);
  }

  ImVec2 Outside() const { return At(-60, 112); }

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
  gui::Canvas canvas_;
  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
  std::unique_ptr<std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>> maps_;
  gfx::Tilemap blockset_{};
  zelda3::OverworldMapTiles initial_tiles_;
  std::vector<uint8_t> initial_pixels_;
  std::vector<int> selected_ids_;
  std::vector<std::array<int, 5>> undo_points_;
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
  EXPECT_EQ(finalize_count_, 2);
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
  EXPECT_EQ(finalize_count_, 1);
  EXPECT_EQ(refresh_count_, 1);

  Frame(At(160, 128), true);
  Frame(At(160, 128));
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world[10][8], 1);
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world[11][8], 2);
  EXPECT_EQ(undo_points_.size(), 4);
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

}  // namespace
}  // namespace yaze::editor
