#include "app/editor/overworld/painting/tile_painting_manager.h"
#include "app/editor/overworld/painting/tile_brush_preview.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/overworld/ui/ui_constants.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas.h"
#include "core/lyra_terrain_document.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

TEST(TileBrushPreviewTest, CrossingQuadrantKeepsTextureExtentAndPixelStride) {
  gfx::Arena::Get().ClearTextureQueue();
  const TileBrush brush{2, 2, {1, 2, 3, 4}};
  std::vector<uint8_t> atlas(128 * 16);
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 128; ++x)
      atlas[y * 128 + x] = x / 16;
  }
  gfx::Bitmap preview;
  const gfx::SnesPalette palette(std::vector<uint16_t>{0, 1, 2, 3, 4});
  const auto crossing = BuildMapBrushPreview(brush, 31, 31, 0, 0, atlas);
  SyncMapBrushPreview(crossing, palette, preview);
  const int texture_width = preview.width();
  const int texture_height = preview.height();
  // An uploaded SDL texture cannot change dimensions through UpdateTexture.
  int texture_tag = 0;
  preview.set_texture(&texture_tag);
  gfx::Arena::Get().ClearTextureQueue();
  const auto inside = BuildMapBrushPreview(brush, 30, 30, 0, 0, atlas);
  SyncMapBrushPreview(inside, palette, preview);
  EXPECT_EQ(preview.width(), texture_width);
  EXPECT_EQ(preview.height(), texture_height);
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      EXPECT_EQ(preview.vector()[y * preview.width() + x],
                brush.at(x / 16, y / 16));
    }
  }
  preview.set_texture(nullptr);
  gfx::Arena::Get().ClearTextureQueue();
}

TEST(TileBrushPreviewTest, CrossingFourMapsUsesEachDestinationGraphicsGroup) {
  const TileBrush brush{2, 2, {1, 2, 3, 4}};
  for (int my = 0; my < 2; ++my) {
    for (int mx = 0; mx < 2; ++mx) {
      const int group = 1 + mx + my * 2;
      std::vector<uint8_t> atlas(128 * 16);
      for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 128; ++x) {
          atlas[y * 128 + x] = group * 16 + x / 16;
        }
      }
      const auto piece = BuildMapBrushPreview(brush, 31, 31, mx, my, atlas);
      ASSERT_EQ(piece.width, 16);
      ASSERT_EQ(piece.height, 16);
      EXPECT_EQ(piece.tile_x, 31 + mx);
      EXPECT_EQ(piece.tile_y, 31 + my);
      const uint8_t expected = group * 16 + brush.at(mx, my);
      EXPECT_TRUE(std::all_of(piece.pixels.begin(), piece.pixels.end(),
                              [expected](uint8_t p) { return p == expected; }));
    }
  }
}

TEST(TileBrushPreviewTest, ClipsNegativeAnchorAndReadsAtlasRows) {
  const TileBrush brush{3, 2, {0, 1, 2, 3, 8, 9}};
  std::vector<uint8_t> atlas(128 * 32);
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 128; ++x) {
      atlas[y * 128 + x] = x / 16 + y / 16 * 8;
    }
  }
  const auto piece = BuildMapBrushPreview(brush, -1, -1, 0, 0, atlas);
  ASSERT_EQ(piece.width, 32);
  ASSERT_EQ(piece.height, 16);
  EXPECT_EQ(piece.tile_x, 0);
  EXPECT_EQ(piece.tile_y, 0);
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 32; ++x)
      EXPECT_EQ(piece.pixels[y * 32 + x], 8 + x / 16);
  }
  EXPECT_TRUE(BuildMapBrushPreview(brush, -1, -1, 1, 0, atlas).pixels.empty());
}

TEST(TileBrushPreviewTest, Tile16PixelsReadOneTileFromEachAreaBlockset) {
  // Two areas' blocksets (128 px wide = 8 tiles per row) that draw Tile16 9
  // differently: the pixels must come from the blockset that is passed.
  std::vector<uint8_t> area_a(128 * 32, 0x01);
  std::vector<uint8_t> area_b(128 * 32, 0x02);
  for (int row = 0; row < 16; ++row) {
    area_b[(16 + row) * 128 + 16 + row] = 0x0F;  // Tile 9 diagonal.
  }
  const auto a = Tile16PixelsFromBlockset(area_a, 9);
  const auto b = Tile16PixelsFromBlockset(area_b, 9);
  ASSERT_EQ(a.size(), 256u);
  ASSERT_EQ(b.size(), 256u);
  EXPECT_NE(a, b);
  EXPECT_EQ(b[0], 0x0F);
  EXPECT_EQ(b[1], 0x02);
  EXPECT_EQ(b[17], 0x0F);
  EXPECT_TRUE(Tile16PixelsFromBlockset(area_a, 16).empty());  // Past the end.
  EXPECT_TRUE(Tile16PixelsFromBlockset(area_a, -1).empty());
  EXPECT_TRUE(Tile16PixelsFromBlockset(area_a, 0, 100).empty());
}

TEST(TileBrushPreviewTest, InvalidSourceAndBrushAreBounded) {
  EXPECT_TRUE(BuildMapBrushPreview({}, 0, 0, 0, 0, {}).pixels.empty());
  const TileBrush brush{1, 1, {0xFFFF}};
  std::vector<uint8_t> atlas(128 * 16, 7);
  const auto piece = BuildMapBrushPreview(brush, 0, 0, 0, 0, atlas);
  ASSERT_EQ(piece.pixels.size(), 256u);
  EXPECT_TRUE(std::all_of(piece.pixels.begin(), piece.pixels.end(),
                          [](uint8_t p) { return p == 0; }));
  EXPECT_TRUE(BuildMapBrushPreview(brush, 0, 0, 0, 0, atlas, 1).pixels.empty());
}

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
        std::make_unique<std::array<gfx::Bitmap, zelda3::kExpandedMapCount>>();
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
    callbacks.request_tile16_selection = [this](int tile_id) {
      current_tile_ = tile_id;
    };
    select_map_hook_ = [this](int map) {
      current_map_ = map;
    };
    callbacks.select_map_for_editing = [this](int map) {
      selected_maps_.push_back(map);
      if (select_map_hook_) {
        select_map_hook_(map);
      }
    };
    callbacks.is_project_tile = [this](int world, int x, int y) {
      return project_document_ && world == 2 && x >= 0 && x < 256 && y >= 128 &&
             y < 256;
    };
    callbacks.read_project_tile = [this](int, int x, int y) {
      return project_document_->tile(x, y - 128);
    };
    callbacks.paint_project_pattern = [this](const TileBrush& brush, int x,
                                             int y, int width, int height,
                                             bool finalize) -> absl::Status {
      ++project_paint_calls_;
      if (project_write_error_)
        return absl::FailedPreconditionError("Fixture project write refused");
      for (int id : brush.tile_ids) {
        if (id >= project_document_->profile().tile_count)
          return absl::OutOfRangeError("Fixture tile is outside project atlas");
      }
      if (!project_document_->stroke_active()) {
        auto status = project_document_->BeginStroke();
        if (!status.ok())
          return status;
      }
      for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
          if (x + dx < 0 || x + dx >= 256 || y + dy < 128 || y + dy >= 256)
            continue;
          auto status = project_document_->Paint(
              x + dx, y + dy - 128,
              brush.at(dx % brush.width, dy % brush.height));
          if (!status.ok())
            return status;
        }
      }
      if (finalize)
        FinishProjectPaint();
      return absl::OkStatus();
    };
    callbacks.finalize_project_paint = [this] {
      FinishProjectPaint();
    };
    callbacks.project_tile_atlas = [this](int) {
      return &blockset_.atlas;
    };
    callbacks.allocated_rows = [this](int world) {
      return world == 2 && !project_document_ ? 4 : 8;
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
    if (suspend_canvas_gestures_) {
      manager_->SuspendCanvasGestures();
    } else {
      manager_->CheckForOverworldEdits();
    }
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

  void EnableProjectTerrain() {
    core::LyraTerrainProfile profile;
    profile.tile_count = 5;
    profile.tile16_definition_fingerprint = "fixture:tiles";
    profile.graphics_fingerprint = "fixture:gfx";
    profile.palette_fingerprint = "fixture:palette";
    profile.animation_fingerprint = "fixture:animation";
    auto created = core::LyraTerrainDocument::Create(profile);
    ASSERT_TRUE(created.ok());
    project_document_ =
        std::make_unique<core::LyraTerrainDocument>(std::move(*created));
    current_world_ = 2;
    current_map_ = 0xA0;
    SetView(1.0f, ImVec2(0, 2048));
  }

  void FinishProjectPaint() {
    if (!project_document_ || !project_document_->stroke_active())
      return;
    auto stroke = project_document_->EndStroke();
    ASSERT_TRUE(stroke.ok());
    if (!stroke->empty())
      project_strokes_.push_back(std::move(*stroke));
  }

  void SelectProjectSeamBrush(float scale) {
    ASSERT_NE(project_document_, nullptr);
    ASSERT_TRUE(project_document_->BeginStroke().ok());
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 2; ++x) {
        ASSERT_TRUE(
            project_document_->Paint(31 + x, 31 + y, y * 2 + x + 1).ok());
      }
    }
    ASSERT_TRUE(project_document_->EndStroke().ok());
    project_document_->MarkSaved();
    SetView(scale, ImVec2(448, 2496));
    const ImVec2 start = AtWorld(31 * 16 + 8, 159 * 16 + 8);
    const ImVec2 end = AtWorld(32 * 16 + 8, 160 * 16 + 8);
    Frame(start);
    Frame(start, false, true);
    Frame(end, false, true);
    Frame(end);
    ASSERT_TRUE(canvas_.select_rect_active());
    ExpectBrush(2, 2, {1, 2, 3, 4});
    EXPECT_EQ(selected_ids_, (std::vector<int>{1, 2, 3, 4}));
    EXPECT_FALSE(project_document_->dirty());
    EXPECT_TRUE(project_strokes_.empty());
    ExpectUnchanged();
  }

  ImGuiContext* context_ = nullptr;
  ImVec2 host_size_ = ImVec2(400, 400);
  gui::Canvas canvas_;
  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
  std::unique_ptr<std::array<gfx::Bitmap, zelda3::kExpandedMapCount>> maps_;
  gfx::Tilemap blockset_{};
  zelda3::OverworldMapTiles initial_tiles_;
  std::vector<uint8_t> initial_pixels_;
  std::vector<int> selected_ids_;
  std::vector<std::array<int, 5>> undo_points_;
  std::vector<int> refreshed_maps_;
  std::vector<int> selected_maps_;
  std::function<void(int)> select_map_hook_;
  int current_tile_ = 2;
  int current_map_ = 0;
  int current_world_ = 0;
  EditingMode current_mode_ = EditingMode::DRAW_TILE;
  int finalize_count_ = 0;
  int refresh_count_ = 0;
  bool suspend_canvas_gestures_ = false;
  std::unique_ptr<core::LyraTerrainDocument> project_document_;
  std::vector<core::LyraTerrainStroke> project_strokes_;
  int project_paint_calls_ = 0;
  bool project_write_error_ = false;
  std::unique_ptr<TilePaintingManager> manager_;
};

TEST_F(TilePaintingManagerGestureTest,
       DragonShipQuadrantsPickCaptureAndPasteAtZoomAndPan) {
  for (float scale : {0.5f, 1.0f, 2.0f}) {
    SCOPED_TRACE(scale);
    canvas_.ClearSelection();
    auto& world = overworld_->mutable_map_tiles()->light_world;
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 2; ++x)
        world[31 + x][223 + y] = 1 + x + y * 2;
    }
    SetView(scale, ImVec2(464, 3536));
    current_map_ = 0x30;
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 2; ++x) {
        Frame(AtWorld(504 + x * 16, 3576 + y * 16), false, false,
              [&] { EXPECT_TRUE(manager_->PickTile16FromHoveredCanvas()); });
        EXPECT_EQ(current_tile_, 1 + x + y * 2);
      }
    }
    Frame(AtWorld(504, 3576));
    Frame(AtWorld(504, 3576), false, true);
    Frame(AtWorld(520, 3592), false, true);
    Frame(AtWorld(520, 3592));
    ASSERT_NE(manager_->selection_brush(), nullptr);
    const TileBrush captured = *manager_->selection_brush();
    EXPECT_EQ(captured.tile_ids, (std::vector<int>{1, 2, 3, 4}));
    Frame(AtWorld(536, 3608));
    ExpectPreview(528, 3600, 544, 3616);
    EXPECT_EQ(manager_->selection_brush()->tile_ids, captured.tile_ids);
    for (int y = 225; y <= 226; ++y) {
      for (int x = 33; x <= 34; ++x)
        world[x][y] = 0;
    }
    Frame(AtWorld(536, 3608), false, false,
          [&] { EXPECT_TRUE(manager_->PasteBrush(captured).ok()); });
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 2; ++x)
        EXPECT_EQ(world[33 + x][225 + y], captured.at(x, y));
    }
    EXPECT_EQ(current_map_, 0x39);
  }
}

TEST_F(TilePaintingManagerGestureTest,
       ProjectRectangleCapturesAndStampsAcrossNeighboringMaps) {
  EnableProjectTerrain();
  SelectProjectSeamBrush(1.0f);
  SetView(1.0f, ImVec2(448, 3008));
  ClickWorld(504, 3064);
  ExpectBrush(2, 2, {1, 2, 3, 4});
  ExpectPreview(496, 3056, 512, 3072);
  EXPECT_EQ(*project_document_->tile(31, 63), 1);
  EXPECT_EQ(*project_document_->tile(32, 63), 2);
  EXPECT_EQ(*project_document_->tile(31, 64), 3);
  EXPECT_EQ(*project_document_->tile(32, 64), 4);
  ASSERT_EQ(project_strokes_.size(), 1u);
  EXPECT_EQ(project_strokes_[0].changes.size(), 4u);
  EXPECT_TRUE(project_document_->dirty());
  EXPECT_TRUE(project_document_->ApplyStroke(project_strokes_[0], true).ok());
  EXPECT_FALSE(project_document_->dirty());
  EXPECT_TRUE(project_document_->ApplyStroke(project_strokes_[0], false).ok());
  EXPECT_EQ(*project_document_->tile(32, 64), 4);
  ExpectUnchanged();
}

TEST_F(TilePaintingManagerGestureTest,
       ProjectCaptureAndPasteRetainWorldCoordinatesAtEveryZoom) {
  for (float scale : {0.5f, 1.0f, 2.0f}) {
    SCOPED_TRACE(scale);
    canvas_.ClearSelection();
    project_strokes_.clear();
    EnableProjectTerrain();
    SelectProjectSeamBrush(scale);
    const TileBrush captured = *manager_->selection_brush();
    SetView(scale, ImVec2(448, 3008));
    ASSERT_TRUE(PasteAt(AtWorld(504, 3064), captured).ok());
    EXPECT_EQ(*project_document_->tile(31, 63), 1);
    EXPECT_EQ(*project_document_->tile(32, 64), 4);
    ASSERT_EQ(project_strokes_.size(), 1u);
    EXPECT_EQ(project_strokes_[0].changes.size(), 4u);
    ExpectBrush(2, 2, {1, 2, 3, 4});
    ExpectUnchanged();
  }
}

TEST_F(TilePaintingManagerGestureTest,
       ProjectFillRepeatsCapturedPatternWithinOneScreen) {
  EnableProjectTerrain();
  SelectProjectSeamBrush(1.0f);
  SetView(1.0f, ImVec2(0, 3072));
  current_mode_ = EditingMode::FILL_TILE;
  ClickWorld(8, 3080);
  for (int y = 64; y < 96; ++y) {
    for (int x = 0; x < 32; ++x) {
      EXPECT_EQ(*project_document_->tile(x, y), (y % 2) * 2 + x % 2 + 1);
    }
  }
  EXPECT_EQ(*project_document_->tile(32, 64), 0);
  EXPECT_EQ(*project_document_->tile(0, 63), 0);
  EXPECT_EQ(*project_document_->tile(0, 96), 0);
  ASSERT_EQ(project_strokes_.size(), 1u);
  EXPECT_EQ(project_strokes_[0].changes.size(), 1024u);
  EXPECT_TRUE(project_document_->ApplyStroke(project_strokes_[0], true).ok());
  EXPECT_FALSE(project_document_->dirty());
  ExpectUnchanged();
}

TEST_F(TilePaintingManagerGestureTest,
       LegacyCapturedBrushPastesIntoProjectAndClipsAtBF) {
  SelectFourTileBrush();
  const TileBrush captured = *manager_->selection_brush();
  EnableProjectTerrain();
  SetView(1.0f, ImVec2(3968, 3968));
  ASSERT_TRUE(PasteAt(AtWorld(4095, 4095), captured).ok());
  EXPECT_EQ(*project_document_->tile(255, 127), 1);
  EXPECT_EQ(*project_document_->tile(0, 0), 0);
  ASSERT_EQ(project_strokes_.size(), 1u);
  EXPECT_EQ(project_strokes_[0].changes.size(), 1u);
  ExpectBrush(2, 2, {1, 2, 3, 4});
  ExpectUnchanged();
}

TEST_F(TilePaintingManagerGestureTest,
       ProjectPastePropagatesRefusalWithoutFallingBackToRom) {
  EnableProjectTerrain();
  project_write_error_ = true;
  auto status = PasteAt(AtWorld(8, 2056), TileBrush{2, 1, {1, 2}});
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  project_write_error_ = false;
  status = PasteAt(AtWorld(8, 2056), TileBrush{2, 1, {1, 5}});
  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange);
  EXPECT_EQ(project_paint_calls_, 2);
  EXPECT_EQ(*project_document_->tile(0, 0), 0);
  EXPECT_FALSE(project_document_->dirty());
  EXPECT_TRUE(project_strokes_.empty());
  ExpectUnchanged();
}

TEST_F(TilePaintingManagerGestureTest,
       ProjectAndLegacyGesturesCannotTransferAcrossStorageBoundary) {
  EnableProjectTerrain();
  current_map_ = 0x98;
  InitializeMapBitmap(current_map_);
  SetView(1.0f, ImVec2(0, 1920));
  const ImVec2 legacy = AtWorld(8, 2040);
  const ImVec2 project = AtWorld(8, 2056);
  Frame(legacy);
  Frame(legacy, true);
  ASSERT_EQ(undo_points_.size(), 1u);
  Frame(project, true);
  EXPECT_EQ(finalize_count_, 1);
  EXPECT_EQ(project_paint_calls_, 0);
  Frame(AtWorld(24, 2040), true);
  Frame(AtWorld(24, 2040));
  EXPECT_EQ(undo_points_.size(), 1u);

  const auto legacy_after = overworld_->mutable_map_tiles()->special_world;
  rom_.ClearDirty();
  Frame(project);
  Frame(project, true);
  Frame(AtWorld(24, 2040), true);
  ASSERT_EQ(project_strokes_.size(), 1u);
  Frame(AtWorld(24, 2056), true);
  Frame(AtWorld(24, 2056));
  EXPECT_EQ(project_paint_calls_, 1);
  EXPECT_EQ(*project_document_->tile(0, 0), 2);
  EXPECT_EQ(*project_document_->tile(1, 0), 0);
  EXPECT_EQ(overworld_->mutable_map_tiles()->special_world, legacy_after);
  EXPECT_EQ(undo_points_.size(), 1u);
  EXPECT_FALSE(rom_.dirty());

  // Only a fresh press may acquire the project destination again.
  ClickWorld(24, 2056);
  EXPECT_EQ(*project_document_->tile(1, 0), 2);
  EXPECT_EQ(project_strokes_.size(), 2u);
}

TEST_F(TilePaintingManagerGestureTest,
       LegacyPatternClipsProjectCellsEvenWhenBrushOverlapsBoundary) {
  EnableProjectTerrain();
  current_map_ = 0x98;
  InitializeMapBitmap(current_map_);
  SetView(1.0f, ImVec2(0, 1920));
  ASSERT_TRUE(PasteAt(AtWorld(8, 2040), TileBrush{1, 2, {1, 2}}).ok());
  EXPECT_EQ(overworld_->mutable_map_tiles()->special_world[0][127], 1);
  EXPECT_EQ(*project_document_->tile(0, 0), 0);
  EXPECT_EQ(project_paint_calls_, 0);
  EXPECT_TRUE(project_strokes_.empty());
  EXPECT_EQ(undo_points_.size(), 1u);
  EXPECT_EQ(finalize_count_, 1);
}

TEST_F(TilePaintingManagerGestureTest,
       ProjectDragDepartureCannotBridgeOnHeldReentry) {
  for (bool outside_document : {false, true}) {
    SCOPED_TRACE(outside_document);
    project_strokes_.clear();
    project_paint_calls_ = 0;
    EnableProjectTerrain();
    SetView(1.0f, ImVec2(3968, 2048));
    const ImVec2 start = AtWorld(4088, 2056);
    const ImVec2 reentry = AtWorld(4088, 2088);
    Frame(start);
    Frame(start, true);
    ASSERT_EQ(project_paint_calls_, 1);
    ASSERT_TRUE(project_document_->stroke_active());

    // Both an occluded/outside canvas and an out-of-document tile inside the
    // canvas must end interpolation before a distant held reentry.
    Frame(outside_document ? AtWorld(4104, 2056) : Outside(), true);
    EXPECT_EQ(canvas_.IsMouseHovering(), outside_document);
    ASSERT_EQ(project_strokes_.size(), 1u);
    EXPECT_FALSE(project_document_->stroke_active());
    Frame(reentry, true);
    Frame(reentry);
    EXPECT_EQ(project_paint_calls_, 1);
    EXPECT_EQ(*project_document_->tile(255, 0), 2);
    EXPECT_EQ(*project_document_->tile(255, 1), 0);
    EXPECT_EQ(*project_document_->tile(255, 2), 0);

    Frame(reentry, true);
    Frame(reentry);
    EXPECT_EQ(project_strokes_.size(), 2u);
    EXPECT_EQ(*project_document_->tile(255, 2), 2);
    ExpectUnchanged();
  }
}

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

TEST_F(TilePaintingManagerGestureTest,
       SuspendedLegacyStrokeCannotAuthorizeProjectOriginDrag) {
  SelectTwoTileBrush();
  current_world_ = 2;
  current_map_ = 0x98;
  InitializeMapBitmap(current_map_);
  // Show the last ROM-backed row and the first project-terrain row in the
  // same canvas. Only the production input dispatcher changes at Y=2048.
  SetView(1.0f, ImVec2(0, 1920));
  const ImVec2 legacy_start = AtWorld(128, 2000);
  const ImVec2 project_cell = AtWorld(128, 2080);
  const ImVec2 legacy_return = AtWorld(160, 2000);
  Frame(legacy_start);
  Frame(legacy_start, true);
  ASSERT_EQ(undo_points_.size(), 2u);
  ASSERT_EQ(finalize_count_, 0);
  const auto after_legacy_press =
      overworld_->mutable_map_tiles()->special_world;
  const auto pixels_after_legacy_press = (*maps_)[0x98].vector();

  // The legacy handler does not see the release or next press: the lower
  // Special World rows dispatch those frames to the project terrain owner.
  suspend_canvas_gestures_ = true;
  Frame(project_cell, true);
  EXPECT_EQ(finalize_count_, 1);
  Frame(project_cell);
  Frame(project_cell, true);
  ExpectBrush(2, 1, {1, 2});
  rom_.ClearDirty();

  suspend_canvas_gestures_ = false;
  Frame(legacy_return, true);
  Frame(AtWorld(192, 2000), true);
  Frame(AtWorld(192, 2000));
  EXPECT_EQ(overworld_->mutable_map_tiles()->special_world, after_legacy_press);
  EXPECT_EQ((*maps_)[0x98].vector(), pixels_after_legacy_press);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_EQ(undo_points_.size(), 2u);
  EXPECT_EQ(finalize_count_, 1);
  EXPECT_EQ(refresh_count_, 1);
  ExpectBrush(2, 1, {1, 2});

  // A new press actually owned by the legacy canvas still stamps the saved
  // brush, proving suspension did not disable editing or erase the pattern.
  Frame(legacy_return);
  Frame(legacy_return, true);
  Frame(legacy_return);
  EXPECT_EQ(overworld_->mutable_map_tiles()->special_world[10][125], 1);
  EXPECT_EQ(overworld_->mutable_map_tiles()->special_world[11][125], 2);
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(undo_points_.size(), 4u);
  EXPECT_EQ(finalize_count_, 2);
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
  // Paint a single tile at (64,64), then move to a different world position
  // without another left press. (Single-tile painting no longer goes through
  // Canvas::DrawTilemapPainter, so there is no drawn position to go stale.)
  ClickWorld(72, 72);
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
       PasteDelegatesMapSelectionWithoutWritingCurrentMap) {
  InitializeMapBitmap(0x31);
  SetView(1.0f, ImVec2(512, 3072));
  current_map_ = 0x30;
  selected_maps_.clear();
  select_map_hook_ = [](int) {
  };

  const auto status = PasteAt(AtWorld(520, 3080), TileBrush{1, 1, {7}});

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(selected_maps_, (std::vector<int>{0x31}));
  EXPECT_EQ(current_map_, 0x30);
  EXPECT_EQ(overworld_->mutable_map_tiles()->light_world[32][192], 7);
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

TEST_F(TilePaintingManagerGestureTest,
       GuardedStencilAppliesAllCellsAsOneUndoStep) {
  // Map 1 starts at world cell (32,0); the unchanged cell is a guard only.
  auto& world = overworld_->mutable_map_tiles()->light_world;
  world[32][0] = 1;
  world[33][0] = 2;
  initial_tiles_ = *overworld_->mutable_map_tiles();

  const auto changed = manager_->ApplyGuardedCells(
      1, {{0, 0, 1, 3}, {1, 0, 2, 2}, {31, 31, 0, 4}});
  ASSERT_TRUE(changed.ok()) << changed.status();
  EXPECT_EQ(*changed, 2);
  EXPECT_EQ(world[32][0], 3);
  EXPECT_EQ(world[33][0], 2);
  EXPECT_EQ(world[63][31], 4);
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(undo_points_.size(), 2);
  EXPECT_EQ(undo_points_[0], (std::array<int, 5>{1, 0, 32, 0, 1}));
  EXPECT_EQ(finalize_count_, 1);
  EXPECT_EQ(refreshed_maps_, (std::vector<int>{1}));
}

TEST_F(TilePaintingManagerGestureTest,
       GuardedStencilRefusesEveryCellWhenOneGuardDiffers) {
  const auto changed =
      manager_->ApplyGuardedCells(0, {{0, 0, 1, 3}, {1, 0, 4, 3}});
  ASSERT_FALSE(changed.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(changed.status()));
  EXPECT_NE(changed.status().message().find(
                "1 cell(s); first (1,0) expected 0x004, found 0x002"),
            std::string_view::npos)
      << changed.status();
  ExpectUnchanged();
}

TEST_F(TilePaintingManagerGestureTest, GuardedStencilRejectsInvalidCells) {
  EXPECT_TRUE(absl::IsInvalidArgument(
      manager_->ApplyGuardedCells(0, {{0, 0, 1, 3}, {0, 0, 1, 3}}).status()));
  EXPECT_TRUE(absl::IsInvalidArgument(
      manager_->ApplyGuardedCells(0, {{32, 0, 0, 3}}).status()));
  EXPECT_TRUE(absl::IsInvalidArgument(
      manager_->ApplyGuardedCells(0, {{0, 0, 1, 0x1000}}).status()));
  EXPECT_TRUE(
      absl::IsInvalidArgument(manager_->ApplyGuardedCells(0, {}).status()));
  EXPECT_TRUE(absl::IsFailedPrecondition(
      manager_->ApplyGuardedCells(0x40, {{0, 0, 0, 3}}).status()));
  EXPECT_TRUE(absl::IsOutOfRange(
      manager_->ApplyGuardedCells(zelda3::kNumOverworldMaps, {{0, 0, 0, 3}})
          .status()));
  ExpectUnchanged();
}

}  // namespace
}  // namespace yaze::editor
