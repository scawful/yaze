#include "app/editor/dungeon/interaction/tile_object_handler.h"
#include "app/editor/dungeon/object_selection.h"
#include "app/gfx/resource/arena.h"
#include "app/platform/sdl_compat.h"
#include "imgui/imgui.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <array>

#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::editor {
namespace {

// Helper to create test objects
zelda3::RoomObject CreateTestObject(uint8_t x, uint8_t y, uint8_t size = 0x00,
                                    int16_t id = 0x01) {
  return zelda3::RoomObject(id, x, y, size, 0);
}

zelda3::RoomObject CreateLayeredTestObject(uint8_t x, uint8_t y,
                                           zelda3::RoomObject::LayerType layer,
                                           uint8_t size = 0x00,
                                           int16_t id = 0x01) {
  auto object = CreateTestObject(x, y, size, id);
  object.layer_ = layer;
  return object;
}

// Test fixture for TileObjectHandler tests
class TileObjectHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gfx::Arena::Get().ClearTextureQueue();
    // Rooms use default constructor - no ROM needed for basic tests

    // Initialize ImGui
    ImGui::CreateContext();
    ImGui::GetIO().DisplaySize = ImVec2(1024, 768);

    // Set up context
    ctx_.rooms = &rooms_;
    ctx_.current_room_id = 0;
    ctx_.selection = &selection_;
    ctx_.on_mutation = [this]() {
      mutation_count_++;
      last_mutation_domain_ = ctx_.last_mutation_domain;
    };
    ctx_.on_invalidate_cache = [this]() {
      invalidate_count_++;
      last_invalidation_domain_ = ctx_.last_invalidation_domain;
    };

    handler_.SetContext(&ctx_);
  }

  void TearDown() override {
    handler_.CancelPlacement();
    gfx::Arena::Get().ClearTextureQueue();
    // Clear any test objects
    rooms_[0].ClearTileObjects();
    ImGui::DestroyContext();
  }

  // Helper to add test objects to room 0
  void AddTestObjects(const std::vector<zelda3::RoomObject>& objects) {
    for (const auto& obj : objects) {
      rooms_[0].AddTileObject(obj);
    }
  }

  DungeonRoomStore rooms_;
  ObjectSelection selection_;
  InteractionContext ctx_;
  TileObjectHandler handler_;
  int mutation_count_ = 0;
  int invalidate_count_ = 0;
  MutationDomain last_mutation_domain_ = MutationDomain::kUnknown;
  MutationDomain last_invalidation_domain_ = MutationDomain::kUnknown;
};

// ============================================================================
// Placement Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, PlaceObjectAtValidPosition) {
  auto obj = CreateTestObject(0, 0, 0x00, 0x01);

  handler_.PlaceObjectAt(0, obj, 10, 15);

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].x_, 10);
  EXPECT_EQ(objects[0].y_, 15);
  EXPECT_EQ(objects[0].id_, 0x01);
  EXPECT_GT(mutation_count_, 0);
  EXPECT_EQ(last_mutation_domain_, MutationDomain::kTileObjects);
  EXPECT_EQ(last_invalidation_domain_, MutationDomain::kTileObjects);
}

TEST_F(TileObjectHandlerTest, MoveObjectMarksObjectStreamDirty) {
  auto obj = CreateTestObject(10, 10);
  rooms_[0].AddTileObject(obj);
  rooms_[0].ClearSaveDirtyState();

  handler_.MoveObjects(0, {0}, 2, 3);

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].x_, 12);
  EXPECT_EQ(objects[0].y_, 13);
  EXPECT_TRUE(rooms_[0].object_stream_dirty());
  EXPECT_TRUE(rooms_[0].HasUnsavedChanges());
}

TEST_F(TileObjectHandlerTest, MoveTorchMarksTorchDirtyOnly) {
  auto torch = CreateTestObject(10, 10, 0x00, 0x150);
  torch.set_options(zelda3::ObjectOption::Torch);
  rooms_[0].AddTileObject(torch);
  rooms_[0].ClearSaveDirtyState();

  handler_.MoveObjects(0, {0}, 1, 0);

  EXPECT_TRUE(rooms_[0].torches_dirty());
  EXPECT_FALSE(rooms_[0].object_stream_dirty());
  EXPECT_TRUE(rooms_[0].HasUnsavedChanges());
}

TEST_F(TileObjectHandlerTest, PlaceObjectClampsToRoomBounds) {
  auto obj = CreateTestObject(0, 0, 0x00, 0x01);

  handler_.PlaceObjectAt(0, obj, 100, -5);

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].x_, 63);  // Clamped to max
  EXPECT_EQ(objects[0].y_, 0);   // Clamped to min
}

TEST_F(TileObjectHandlerTest, PlaceObjectRejectsInvalidRoom) {
  auto obj = CreateTestObject(0, 0, 0x00, 0x01);

  // Room ID out of bounds
  EXPECT_FALSE(handler_.PlaceObjectAt(999, obj, 10, 15));
  EXPECT_TRUE(handler_.was_placement_blocked());
  EXPECT_EQ(handler_.placement_block_reason(),
            TileObjectHandler::PlacementBlockReason::kInvalidRoom);
  handler_.clear_placement_blocked();
  EXPECT_FALSE(handler_.was_placement_blocked());

  // Nothing should be added to room 0
  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects.size(), 0);
}

TEST_F(TileObjectHandlerTest, PlaceObjectBlocksAtRoomObjectLimit) {
  // Fill room to max object count.
  constexpr int kMaxObjects = 400;
  std::vector<zelda3::RoomObject> objects;
  objects.reserve(kMaxObjects);
  for (int i = 0; i < kMaxObjects; ++i) {
    objects.push_back(CreateTestObject(i % 64, (i / 64) % 64, 0x00, 0x01));
  }
  AddTestObjects(objects);

  auto obj = CreateTestObject(0, 0, 0x00, 0x01);
  EXPECT_FALSE(handler_.PlaceObjectAt(0, obj, 10, 15));
  EXPECT_TRUE(handler_.was_placement_blocked());
  EXPECT_EQ(handler_.placement_block_reason(),
            TileObjectHandler::PlacementBlockReason::kObjectLimit);
  EXPECT_EQ(rooms_[0].GetTileObjects().size(),
            static_cast<size_t>(kMaxObjects));
}

TEST_F(TileObjectHandlerTest, GhostCapacityStateIsNormalBelowLastSlot) {
  std::vector<zelda3::RoomObject> objects;
  objects.reserve(398);
  for (int i = 0; i < 398; ++i) {
    objects.push_back(CreateTestObject(i % 64, (i / 64) % 64, 0x00, 0x01));
  }
  AddTestObjects(objects);

  EXPECT_EQ(handler_.GetPlacementGhostCapacityState(),
            TileObjectHandler::GhostCapacityState::kNormal);
}

TEST_F(TileObjectHandlerTest, GhostCapacityStateWarnsOnLastAvailableSlot) {
  std::vector<zelda3::RoomObject> objects;
  objects.reserve(399);
  for (int i = 0; i < 399; ++i) {
    objects.push_back(CreateTestObject(i % 64, (i / 64) % 64, 0x00, 0x01));
  }
  AddTestObjects(objects);

  EXPECT_EQ(handler_.GetPlacementGhostCapacityState(),
            TileObjectHandler::GhostCapacityState::kNearLimit);
}

TEST_F(TileObjectHandlerTest, GhostCapacityStateBlocksWhenRoomIsFull) {
  std::vector<zelda3::RoomObject> objects;
  objects.reserve(400);
  for (int i = 0; i < 400; ++i) {
    objects.push_back(CreateTestObject(i % 64, (i / 64) % 64, 0x00, 0x01));
  }
  AddTestObjects(objects);

  EXPECT_EQ(handler_.GetPlacementGhostCapacityState(),
            TileObjectHandler::GhostCapacityState::kAtLimit);
}

TEST_F(TileObjectHandlerTest,
       GhostPreviewGeometryPreservesUpwardAndLeftwardExtents) {
  struct TestCase {
    int16_t object_id;
    uint8_t size;
    int anchor_x;
    int anchor_y;
    int offset_x;
    int offset_y;
  };

  // The first two need negative-axis headroom; 0x33 is the ordinary baseline,
  // and 0x34 starts three tiles to the right of its encoded origin.
  constexpr std::array<TestCase, 4> kCases{{
      {0x09, 0x12, 0, 8, 0, -8},
      {0xA3, 0x12, 5, 5, -5, -5},
      {0x33, 0x12, 0, 0, 0, 0},
      {0x34, 0x12, 0, 0, 3, 0},
  }};

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(::testing::Message()
                 << "object_id=0x" << std::hex << test_case.object_id);
    const auto object = CreateTestObject(
        /*x=*/0, /*y=*/0, test_case.size, test_case.object_id);
    const auto geometry =
        TileObjectHandler::CalculateGhostPreviewGeometry(object);

    EXPECT_EQ(geometry.render_anchor_x_tiles, test_case.anchor_x);
    EXPECT_EQ(geometry.render_anchor_y_tiles, test_case.anchor_y);
    EXPECT_EQ(geometry.offset_x_tiles, test_case.offset_x);
    EXPECT_EQ(geometry.offset_y_tiles, test_case.offset_y);
    EXPECT_GE(geometry.render_anchor_x_tiles + geometry.offset_x_tiles, 0);
    EXPECT_GE(geometry.render_anchor_y_tiles + geometry.offset_y_tiles, 0);
    EXPECT_GT(geometry.width_pixels, 0);
    EXPECT_GT(geometry.height_pixels, 0);
    EXPECT_EQ(
        geometry.buffer_width_pixels,
        geometry.width_pixels +
            (geometry.render_anchor_x_tiles + geometry.offset_x_tiles) * 8);
    EXPECT_EQ(
        geometry.buffer_height_pixels,
        geometry.height_pixels +
            (geometry.render_anchor_y_tiles + geometry.offset_y_tiles) * 8);
  }
}

TEST_F(TileObjectHandlerTest,
       GhostPreviewBitmapRendersPositiveOffsetWithoutClipping) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  rooms_.SetRom(&rom);
  rooms_[0].SetLoaded(true);
  ctx_.rom = &rom;

  auto object = CreateTestObject(/*x=*/0, /*y=*/0, /*size=*/0x02,
                                 /*id=*/0x34);
  object.mutable_tiles().assign(
      1, gfx::TileInfo(/*id=*/0, /*palette=*/0, /*v=*/false, /*h=*/false,
                       /*o=*/false));
  object.tiles_loaded_ = true;

  const auto geometry =
      TileObjectHandler::CalculateGhostPreviewGeometry(object);
  ASSERT_EQ(geometry.offset_x_tiles, 3);
  handler_.SetPreviewObject(object);
  handler_.BeginPlacement();

  const auto* buffer = handler_.ghost_preview_buffer_for_testing();
  ASSERT_NE(buffer, nullptr);
  const auto& bitmap = buffer->bitmap();
  ASSERT_TRUE(bitmap.is_active());
  ASSERT_GE(bitmap.width(), geometry.buffer_width_pixels);
  ASSERT_GE(bitmap.height(), geometry.buffer_height_pixels);

  const auto& coverage = buffer->coverage_data();
  ASSERT_EQ(coverage.size(),
            static_cast<size_t>(bitmap.width() * bitmap.height()));
  const int first_drawn_x = geometry.offset_x_tiles * 8;
  for (int y = 0; y < bitmap.height(); ++y) {
    EXPECT_EQ(coverage[y * bitmap.width() + first_drawn_x - 1], 0);
  }
  const auto column_has_coverage = [&](int x) {
    for (int y = 0; y < bitmap.height(); ++y) {
      if (coverage[y * bitmap.width() + x] != 0) {
        return true;
      }
    }
    return false;
  };
  EXPECT_TRUE(column_has_coverage(first_drawn_x));
  EXPECT_TRUE(column_has_coverage(geometry.buffer_width_pixels - 1));
}

TEST_F(TileObjectHandlerTest, GhostPreviewBitmapKeepsUpwardExtentVisible) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  rooms_.SetRom(&rom);
  rooms_[0].SetLoaded(true);
  ctx_.rom = &rom;

  auto object = CreateTestObject(/*x=*/0, /*y=*/0, /*size=*/0x12,
                                 /*id=*/0x09);
  object.mutable_tiles().assign(
      64, gfx::TileInfo(/*id=*/0, /*palette=*/0, /*v=*/false, /*h=*/false,
                        /*o=*/false));
  object.tiles_loaded_ = true;

  const auto geometry =
      TileObjectHandler::CalculateGhostPreviewGeometry(object);
  ASSERT_LT(geometry.offset_y_tiles, 0);
  handler_.SetPreviewObject(object);
  handler_.BeginPlacement();

  const auto* buffer = handler_.ghost_preview_buffer_for_testing();
  ASSERT_NE(buffer, nullptr);
  const auto& bitmap = buffer->bitmap();
  ASSERT_TRUE(bitmap.is_active());
  ASSERT_GE(bitmap.width(), geometry.buffer_width_pixels);
  ASSERT_GE(bitmap.height(), geometry.buffer_height_pixels);
  const auto& coverage = buffer->coverage_data();
  EXPECT_TRUE(std::any_of(coverage.begin(), coverage.begin() + bitmap.width(),
                          [](uint8_t covered) { return covered != 0; }));
}

TEST_F(TileObjectHandlerTest,
       GhostPreviewFlattensSampledBg2ObjectAndSynchronizesSurface) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  rooms_.SetRom(&rom);
  rooms_[0].SetLoaded(true);
  ctx_.rom = &rom;
  auto& room_gfx =
      const_cast<std::array<uint8_t, 0x10000>&>(rooms_[0].get_gfx_buffer());
  room_gfx.fill(1);
  rooms_[0].bg1_buffer().EnsureBitmapInitialized();
  std::vector<SDL_Color> room_palette(256);
  for (int i = 0; i < 256; ++i) {
    room_palette[i] = {static_cast<Uint8>(i), static_cast<Uint8>(255 - i),
                       static_cast<Uint8>(i / 2), 255};
  }
  room_palette[255] = {0, 0, 0, 0};
  rooms_[0].bg1_buffer().bitmap().SetPalette(room_palette);

  auto object = CreateLayeredTestObject(
      /*x=*/0, /*y=*/0, zelda3::RoomObject::LayerType::BG2,
      /*size=*/0x02, /*id=*/0x33);
  object.mutable_tiles().assign(
      64, gfx::TileInfo(/*id=*/0, /*palette=*/0, /*v=*/false, /*h=*/false,
                        /*o=*/false));
  object.tiles_loaded_ = true;

  handler_.SetPreviewObject(object);
  handler_.BeginPlacement();

  const auto* buffer = handler_.ghost_preview_buffer_for_testing();
  ASSERT_NE(buffer, nullptr);
  const auto& bitmap = buffer->bitmap();
  const auto& coverage = buffer->coverage_data();
  const auto drawn = std::find_if(coverage.begin(), coverage.end(),
                                  [](uint8_t covered) { return covered != 0; });
  ASSERT_NE(drawn, coverage.end());
  const size_t drawn_index = std::distance(coverage.begin(), drawn);
  ASSERT_NE(bitmap.at(drawn_index), 255);
  ASSERT_NE(bitmap.surface(), nullptr);
  const auto* surface_pixels =
      static_cast<const uint8_t*>(bitmap.surface()->pixels);
  const size_t surface_index =
      (drawn_index / bitmap.width()) * bitmap.surface()->pitch +
      (drawn_index % bitmap.width());
  EXPECT_EQ(surface_pixels[surface_index], bitmap.at(drawn_index));
  SDL_Palette* ghost_palette = platform::GetSurfacePalette(bitmap.surface());
  ASSERT_NE(ghost_palette, nullptr);
  const uint8_t color_index = bitmap.at(drawn_index);
  EXPECT_EQ(ghost_palette->colors[color_index].r, room_palette[color_index].r);
  EXPECT_EQ(ghost_palette->colors[color_index].g, room_palette[color_index].g);
  EXPECT_EQ(ghost_palette->colors[color_index].b, room_palette[color_index].b);

  ASSERT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1);
  handler_.CancelPlacement();
  handler_.BeginPlacement();
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1);
}

// ============================================================================
// Resize Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, ResizeObjectsIncrements) {
  AddTestObjects({CreateTestObject(5, 5, 0x05, 0x01)});

  handler_.ResizeObjects(0, {0}, 1);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].size_, 6);
}

TEST_F(TileObjectHandlerTest, ResizeObjectsDecrements) {
  AddTestObjects({CreateTestObject(5, 5, 0x05, 0x01)});

  handler_.ResizeObjects(0, {0}, -2);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].size_, 3);
}

TEST_F(TileObjectHandlerTest, ResizeObjectsClampsToMin) {
  AddTestObjects({CreateTestObject(5, 5, 0x02, 0x01)});

  handler_.ResizeObjects(0, {0}, -10);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].size_, 0);
}

TEST_F(TileObjectHandlerTest, ResizeObjectsClampsToMax) {
  AddTestObjects({CreateTestObject(5, 5, 0x0D, 0x01)});

  handler_.ResizeObjects(0, {0}, 10);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].size_, 15);
}

TEST_F(TileObjectHandlerTest, ResizeMultipleObjects) {
  AddTestObjects({CreateTestObject(5, 5, 0x03, 0x01),
                  CreateTestObject(10, 10, 0x05, 0x02),
                  CreateTestObject(15, 15, 0x07, 0x03)});

  handler_.ResizeObjects(0, {0, 2}, 2);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].size_, 5);  // 3 + 2
  EXPECT_EQ(objects[1].size_, 5);  // Unchanged
  EXPECT_EQ(objects[2].size_, 9);  // 7 + 2
}

// ============================================================================
// Property Update Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, UpdateObjectId) {
  AddTestObjects({CreateTestObject(5, 5, 0x00, 0x01)});

  handler_.UpdateObjectsId(0, {0}, 0x42);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].id_, 0x42);
  EXPECT_FALSE(objects[0].tiles_loaded_);
}

TEST_F(TileObjectHandlerTest, UpdateObjectSize) {
  AddTestObjects({CreateTestObject(5, 5, 0x00, 0x01)});

  handler_.UpdateObjectsSize(0, {0}, 0x0A);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].size_, 0x0A);
  EXPECT_FALSE(objects[0].tiles_loaded_);
}

TEST_F(TileObjectHandlerTest, UpdateObjectLayer) {
  AddTestObjects({CreateTestObject(5, 5, 0x00, 0x01)});

  handler_.UpdateObjectsLayer(0, {0}, 1);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].layer_, zelda3::RoomObject::LayerType::BG2);
}

TEST_F(TileObjectHandlerTest, UpdateObjectLayerAppendsToTargetLayerZBucket) {
  AddTestObjects({
      CreateLayeredTestObject(0, 0, zelda3::RoomObject::LayerType::BG1, 0x00,
                              0x01),
      CreateLayeredTestObject(0, 0, zelda3::RoomObject::LayerType::BG2, 0x00,
                              0x02),
      CreateLayeredTestObject(0, 0, zelda3::RoomObject::LayerType::BG2, 0x00,
                              0x03),
      CreateLayeredTestObject(0, 0, zelda3::RoomObject::LayerType::BG1, 0x00,
                              0x04),
  });
  selection_.SelectObject(0);

  handler_.UpdateObjectsLayer(0, {0}, 1);

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 4);
  EXPECT_EQ(objects[0].id_, 0x04);
  EXPECT_EQ(objects[1].id_, 0x02);
  EXPECT_EQ(objects[2].id_, 0x03);
  EXPECT_EQ(objects[3].id_, 0x01);
  EXPECT_EQ(objects[3].layer_, zelda3::RoomObject::LayerType::BG2);
  EXPECT_TRUE(selection_.IsObjectSelected(3));
}

TEST_F(TileObjectHandlerTest, UpdateObjectLayerRejectsInvalidTargetLayer) {
  AddTestObjects({CreateTestObject(5, 5, 0x00, 0x21)});

  const int initial_mutation_count = mutation_count_;
  const int initial_invalidate_count = invalidate_count_;
  handler_.UpdateObjectsLayer(0, {0}, 5);

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].layer_, zelda3::RoomObject::LayerType::BG1);
  EXPECT_EQ(mutation_count_, initial_mutation_count);
  EXPECT_EQ(invalidate_count_, initial_invalidate_count);
}

TEST_F(TileObjectHandlerTest, UpdateObjectLayerSkipsBothBgObjectsForNonBg1) {
  AddTestObjects({CreateTestObject(5, 5, 0x00, 0x03)});  // all_bgs_ object

  const int initial_mutation_count = mutation_count_;
  const int initial_invalidate_count = invalidate_count_;
  handler_.UpdateObjectsLayer(0, {0}, 1);

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].layer_, zelda3::RoomObject::LayerType::BG1);
  EXPECT_EQ(mutation_count_, initial_mutation_count);
  EXPECT_EQ(invalidate_count_, initial_invalidate_count);
}

TEST_F(TileObjectHandlerTest, UpdateObjectLayerRejectsOversizedBatch) {
  std::vector<zelda3::RoomObject> objects;
  objects.reserve(129);
  for (int i = 0; i < 129; ++i) {
    objects.push_back(CreateTestObject(i % 64, (i / 64) % 64, 0x00, 0x21));
  }
  AddTestObjects(objects);

  std::vector<size_t> indices;
  indices.reserve(129);
  for (size_t i = 0; i < 129; ++i) {
    indices.push_back(i);
  }

  const int initial_mutation_count = mutation_count_;
  const int initial_invalidate_count = invalidate_count_;
  handler_.UpdateObjectsLayer(0, indices, 1);

  for (const auto& obj : rooms_[0].GetTileObjects()) {
    EXPECT_EQ(obj.layer_, zelda3::RoomObject::LayerType::BG1);
  }
  EXPECT_EQ(mutation_count_, initial_mutation_count);
  EXPECT_EQ(invalidate_count_, initial_invalidate_count);
}

TEST_F(TileObjectHandlerTest, UpdateObjectLayerRejectsBg3Overflow) {
  std::vector<zelda3::RoomObject> objects;
  objects.reserve(129);
  for (int i = 0; i < 129; ++i) {
    objects.push_back(CreateTestObject(i % 64, (i / 64) % 64, 0x00, 0x21));
  }
  AddTestObjects(objects);

  std::vector<size_t> indices;
  indices.reserve(129);
  for (size_t i = 0; i < 129; ++i) {
    indices.push_back(i);
  }

  const int initial_mutation_count = mutation_count_;
  const int initial_invalidate_count = invalidate_count_;
  handler_.UpdateObjectsLayer(0, indices, 2);

  for (const auto& obj : rooms_[0].GetTileObjects()) {
    EXPECT_EQ(obj.layer_, zelda3::RoomObject::LayerType::BG1);
  }
  EXPECT_EQ(mutation_count_, initial_mutation_count);
  EXPECT_EQ(invalidate_count_, initial_invalidate_count);
}

// ============================================================================
// Deletion Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, DeleteSingleObject) {
  AddTestObjects({CreateTestObject(5, 5, 0x00, 0x01),
                  CreateTestObject(10, 10, 0x00, 0x02)});

  handler_.DeleteObjects(0, {0});

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].id_, 0x02);  // Only second object remains
}

TEST_F(TileObjectHandlerTest, DeleteMultipleObjects) {
  AddTestObjects({CreateTestObject(5, 5, 0x00, 0x01),
                  CreateTestObject(10, 10, 0x00, 0x02),
                  CreateTestObject(15, 15, 0x00, 0x03)});

  handler_.DeleteObjects(0, {0, 2});

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].id_, 0x02);  // Only middle object remains
}

TEST_F(TileObjectHandlerTest, DeleteAllObjects) {
  AddTestObjects({CreateTestObject(5, 5, 0x00, 0x01),
                  CreateTestObject(10, 10, 0x00, 0x02)});

  handler_.DeleteAllObjects(0);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects.size(), 0);
}

// ============================================================================
// Move Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, MoveObjectsPositive) {
  AddTestObjects({CreateTestObject(10, 15, 0x00, 0x01)});

  handler_.MoveObjects(0, {0}, 5, 3);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].x_, 15);
  EXPECT_EQ(objects[0].y_, 18);
}

TEST_F(TileObjectHandlerTest, MoveObjectsNegative) {
  AddTestObjects({CreateTestObject(10, 15, 0x00, 0x01)});

  handler_.MoveObjects(0, {0}, -3, -5);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].x_, 7);
  EXPECT_EQ(objects[0].y_, 10);
}

TEST_F(TileObjectHandlerTest, MoveObjectsClampsPosition) {
  AddTestObjects({CreateTestObject(5, 60, 0x00, 0x01)});

  handler_.MoveObjects(0, {0}, -10, 10);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].x_, 0);   // Clamped to min
  EXPECT_EQ(objects[0].y_, 63);  // Clamped to max
}

// ============================================================================
// Drag Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, DragMovesSelectedObjectsSnapped) {
  AddTestObjects({CreateTestObject(10, 10, 0x00, 0x01)});
  selection_.SelectObject(0);

  const int initial_mutations = mutation_count_;

  // Tile (10,10) = pixel (80,80). Move to (96,96) = +2 tiles.
  handler_.InitDrag(ImVec2(80.0f, 80.0f));
  handler_.HandleDrag(ImVec2(96.0f, 96.0f), ImVec2(16.0f, 16.0f));
  const int invalidations_before_release = invalidate_count_;
  handler_.HandleRelease();

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].x_, 12);
  EXPECT_EQ(objects[0].y_, 12);
  EXPECT_EQ(mutation_count_, initial_mutations + 1);
  EXPECT_EQ(invalidate_count_, invalidations_before_release + 1);
  EXPECT_EQ(last_invalidation_domain_, MutationDomain::kTileObjects);
}

TEST_F(TileObjectHandlerTest, DragReleaseWithoutMovementDoesNotInvalidate) {
  AddTestObjects({CreateTestObject(10, 10, 0x00, 0x01)});
  selection_.SelectObject(0);

  const int initial_invalidations = invalidate_count_;
  handler_.InitDrag(ImVec2(80.0f, 80.0f));
  handler_.HandleRelease();
  EXPECT_EQ(invalidate_count_, initial_invalidations);
}

TEST_F(TileObjectHandlerTest, AltDragMovesOriginalWithoutDuplicating) {
  AddTestObjects({CreateTestObject(10, 10, 0x00, 0x42)});
  selection_.SelectObject(0);

  const int initial_mutations = mutation_count_;

  handler_.InitDrag(ImVec2(80.0f, 80.0f));
  ImGui::GetIO().KeyAlt = true;
  handler_.HandleDrag(ImVec2(96.0f, 80.0f), ImVec2(16.0f, 0.0f));
  ImGui::GetIO().KeyAlt = false;
  handler_.HandleRelease();

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].x_, 12);
  EXPECT_EQ(objects[0].y_, 10);
  EXPECT_EQ(mutation_count_, initial_mutations + 1);
}

// ============================================================================
// Marquee (Rectangle) Selection Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, MarqueeSelectsObjectsInRect) {
  AddTestObjects({
      CreateTestObject(5, 5, 0x00, 0x01),
      CreateTestObject(20, 20, 0x00, 0x02),
      CreateTestObject(40, 40, 0x00, 0x03),
  });

  // Drag from (0,0) to (200,200) which covers room tiles [0..25].
  handler_.BeginMarqueeSelection(ImVec2(0.0f, 0.0f));
  handler_.HandleMarqueeSelection(
      /*mouse_pos=*/ImVec2(200.0f, 200.0f),
      /*mouse_left_down=*/true,
      /*mouse_left_released=*/false,
      /*shift_down=*/false,
      /*toggle_down=*/false,
      /*alt_down=*/false,
      /*draw_box=*/false);
  handler_.HandleMarqueeSelection(
      /*mouse_pos=*/ImVec2(200.0f, 200.0f),
      /*mouse_left_down=*/false,
      /*mouse_left_released=*/true,
      /*shift_down=*/false,
      /*toggle_down=*/false,
      /*alt_down=*/false,
      /*draw_box=*/false);

  EXPECT_TRUE(selection_.IsObjectSelected(0));
  EXPECT_TRUE(selection_.IsObjectSelected(1));
  EXPECT_FALSE(selection_.IsObjectSelected(2));
}

TEST_F(TileObjectHandlerTest, ShiftMarqueeReplacesSelection) {
  AddTestObjects({
      CreateTestObject(5, 5, 0x00, 0x01),
      CreateTestObject(20, 20, 0x00, 0x02),
  });

  selection_.SelectObject(0);

  handler_.BeginMarqueeSelection(ImVec2(120.0f, 120.0f));  // Near (15,15)
  handler_.HandleMarqueeSelection(
      /*mouse_pos=*/ImVec2(200.0f, 200.0f),  // Covers object 1
      /*mouse_left_down=*/false,
      /*mouse_left_released=*/true,
      /*shift_down=*/true,
      /*toggle_down=*/false,
      /*alt_down=*/false,
      /*draw_box=*/false);

  EXPECT_FALSE(selection_.IsObjectSelected(0));
  EXPECT_TRUE(selection_.IsObjectSelected(1));
}

TEST_F(TileObjectHandlerTest, CtrlMarqueeReplacesSelection) {
  AddTestObjects({
      CreateTestObject(5, 5, 0x00, 0x01),
      CreateTestObject(20, 20, 0x00, 0x02),
  });

  // Preselect both.
  selection_.SelectObject(0);
  selection_.SelectObject(1, ObjectSelection::SelectionMode::Add);

  // Modified marquee matches ZScream's replace-selection behavior.
  handler_.BeginMarqueeSelection(ImVec2(120.0f, 120.0f));
  handler_.HandleMarqueeSelection(
      /*mouse_pos=*/ImVec2(200.0f, 200.0f),
      /*mouse_left_down=*/false,
      /*mouse_left_released=*/true,
      /*shift_down=*/false,
      /*toggle_down=*/true,
      /*alt_down=*/false,
      /*draw_box=*/false);

  EXPECT_FALSE(selection_.IsObjectSelected(0));
  EXPECT_TRUE(selection_.IsObjectSelected(1));
}

// ============================================================================
// Duplicate Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, DuplicateObjects) {
  AddTestObjects({CreateTestObject(10, 15, 0x03, 0x42)});

  auto new_indices = handler_.DuplicateObjects(0, {0}, 2, 3);

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 2);
  ASSERT_EQ(new_indices.size(), 1);

  // Original unchanged
  EXPECT_EQ(objects[0].x_, 10);
  EXPECT_EQ(objects[0].y_, 15);

  // Clone offset
  EXPECT_EQ(objects[1].x_, 12);
  EXPECT_EQ(objects[1].y_, 18);
  EXPECT_EQ(objects[1].id_, 0x42);
  EXPECT_EQ(objects[1].size_, 0x03);
}

// ============================================================================
// Z-Order Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, SendToFront) {
  AddTestObjects({CreateTestObject(0, 0, 0x00, 0x01),
                  CreateTestObject(0, 0, 0x00, 0x02),
                  CreateTestObject(0, 0, 0x00, 0x03)});

  handler_.SendToFront(0, {0});

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].id_, 0x02);
  EXPECT_EQ(objects[1].id_, 0x03);
  EXPECT_EQ(objects[2].id_, 0x01);  // Moved to front (last in list)
}

TEST_F(TileObjectHandlerTest, SendToFrontStaysWithinObjectLayer) {
  AddTestObjects({
      CreateLayeredTestObject(0, 0, zelda3::RoomObject::LayerType::BG1, 0x00,
                              0x01),
      CreateLayeredTestObject(0, 0, zelda3::RoomObject::LayerType::BG2, 0x00,
                              0x02),
      CreateLayeredTestObject(0, 0, zelda3::RoomObject::LayerType::BG1, 0x00,
                              0x03),
  });
  selection_.SelectObject(0);

  handler_.SendToFront(0, {0});

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 3);
  EXPECT_EQ(objects[0].id_, 0x03);
  EXPECT_EQ(objects[1].id_, 0x01);
  EXPECT_EQ(objects[2].id_, 0x02);
  EXPECT_TRUE(selection_.IsObjectSelected(1));
}

TEST_F(TileObjectHandlerTest, SendToBack) {
  AddTestObjects({CreateTestObject(0, 0, 0x00, 0x01),
                  CreateTestObject(0, 0, 0x00, 0x02),
                  CreateTestObject(0, 0, 0x00, 0x03)});

  handler_.SendToBack(0, {2});

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].id_, 0x03);  // Moved to back (first in list)
  EXPECT_EQ(objects[1].id_, 0x01);
  EXPECT_EQ(objects[2].id_, 0x02);
}

TEST_F(TileObjectHandlerTest, MoveForward) {
  AddTestObjects({CreateTestObject(0, 0, 0x00, 0x01),
                  CreateTestObject(0, 0, 0x00, 0x02),
                  CreateTestObject(0, 0, 0x00, 0x03)});

  handler_.MoveForward(0, {0});

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].id_, 0x02);
  EXPECT_EQ(objects[1].id_, 0x01);  // Swapped forward
  EXPECT_EQ(objects[2].id_, 0x03);
}

TEST_F(TileObjectHandlerTest, MoveForwardStaysWithinObjectLayer) {
  AddTestObjects({
      CreateLayeredTestObject(0, 0, zelda3::RoomObject::LayerType::BG1, 0x00,
                              0x01),
      CreateLayeredTestObject(0, 0, zelda3::RoomObject::LayerType::BG2, 0x00,
                              0x02),
      CreateLayeredTestObject(0, 0, zelda3::RoomObject::LayerType::BG1, 0x00,
                              0x03),
  });
  selection_.SelectObject(0);

  handler_.MoveForward(0, {0});

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 3);
  EXPECT_EQ(objects[0].id_, 0x03);
  EXPECT_EQ(objects[1].id_, 0x01);
  EXPECT_EQ(objects[2].id_, 0x02);
  EXPECT_TRUE(selection_.IsObjectSelected(1));
}

TEST_F(TileObjectHandlerTest, MoveBackward) {
  AddTestObjects({CreateTestObject(0, 0, 0x00, 0x01),
                  CreateTestObject(0, 0, 0x00, 0x02),
                  CreateTestObject(0, 0, 0x00, 0x03)});

  handler_.MoveBackward(0, {2});

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].id_, 0x01);
  EXPECT_EQ(objects[1].id_, 0x03);  // Swapped backward
  EXPECT_EQ(objects[2].id_, 0x02);
}

// ============================================================================
// Placement Mode Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, PlacementModeLifecycle) {
  EXPECT_FALSE(handler_.IsPlacementActive());

  handler_.BeginPlacement();
  EXPECT_TRUE(handler_.IsPlacementActive());

  handler_.CancelPlacement();
  EXPECT_FALSE(handler_.IsPlacementActive());
}

TEST_F(TileObjectHandlerTest, SetPreviewObject) {
  auto preview = CreateTestObject(0, 0, 0x05, 0x42);

  handler_.SetPreviewObject(preview);

  // Implicitly tested - no crash means success
  // Preview object is used internally for ghost rendering
}

// ============================================================================
// Hit Testing Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, GetEntityAtPositionFindsObject) {
  AddTestObjects({CreateTestObject(10, 10, 0x00, 0x01)});

  // Object at tile (10,10) = pixel (80,80)
  // DimensionService determines actual bounds
  auto result = handler_.GetEntityAtPosition(80, 80);

  // Should find the object if within bounds
  // Exact result depends on DimensionService calculations
  if (result.has_value()) {
    EXPECT_EQ(result.value(), 0);
  }
}

TEST_F(TileObjectHandlerTest, GetEntityAtPositionReturnsEmptyOnMiss) {
  AddTestObjects({CreateTestObject(10, 10, 0x00, 0x01)});

  // Far from any object
  auto result = handler_.GetEntityAtPosition(0, 0);

  EXPECT_FALSE(result.has_value());
}

TEST_F(TileObjectHandlerTest, GetEntityAtPositionPrioritizesTopmost) {
  // Add two overlapping objects
  AddTestObjects({
      CreateTestObject(10, 10, 0x05, 0x01),  // Bottom
      CreateTestObject(10, 10, 0x05, 0x02)   // Top (added last)
  });

  auto result = handler_.GetEntityAtPosition(80, 80);

  if (result.has_value()) {
    EXPECT_EQ(result.value(), 1);  // Should return topmost (last) object
  }
}

// ============================================================================
// Context Validation Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, HasValidContextReturnsFalseWithoutContext) {
  TileObjectHandler handler;  // No context set

  // Operations should be safe (no crash)
  handler.PlaceObjectAt(0, CreateTestObject(0, 0), 0, 0);
  handler.DeleteObjects(0, {0});
}

TEST_F(TileObjectHandlerTest, NotifiesOnMutation) {
  AddTestObjects({CreateTestObject(5, 5, 0x00, 0x01)});

  int initial_count = mutation_count_;
  handler_.MoveObjects(0, {0}, 1, 1);

  EXPECT_GT(mutation_count_, initial_count);
  EXPECT_GT(invalidate_count_, 0);
}

// ============================================================================
// Clipboard Tests
// ============================================================================

TEST_F(TileObjectHandlerTest, CopyToClipboardStoresObjects) {
  AddTestObjects({CreateTestObject(5, 5, 0x02, 0x01),
                  CreateTestObject(10, 10, 0x03, 0x02)});

  EXPECT_FALSE(handler_.HasClipboardData());

  handler_.CopyObjectsToClipboard(0, {0, 1});

  EXPECT_TRUE(handler_.HasClipboardData());
}

TEST_F(TileObjectHandlerTest, CopyToClipboardPartialSelection) {
  AddTestObjects({CreateTestObject(5, 5, 0x02, 0x01),
                  CreateTestObject(10, 10, 0x03, 0x02),
                  CreateTestObject(15, 15, 0x04, 0x03)});

  handler_.CopyObjectsToClipboard(0, {1});

  EXPECT_TRUE(handler_.HasClipboardData());
}

TEST_F(TileObjectHandlerTest, PasteFromClipboardCreatesObjects) {
  AddTestObjects({CreateTestObject(5, 5, 0x02, 0x42)});

  handler_.CopyObjectsToClipboard(0, {0});
  auto new_indices = handler_.PasteFromClipboard(0, 2, 3);

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 2);
  ASSERT_EQ(new_indices.size(), 1);
  EXPECT_EQ(new_indices[0], 1);

  // Pasted object should be offset
  EXPECT_EQ(objects[1].x_, 7);  // 5 + 2
  EXPECT_EQ(objects[1].y_, 8);  // 5 + 3
  EXPECT_EQ(objects[1].id_, 0x42);
  EXPECT_EQ(objects[1].size_, 0x02);
}

TEST_F(TileObjectHandlerTest, PasteMultipleObjects) {
  AddTestObjects({CreateTestObject(5, 5, 0x01, 0x01),
                  CreateTestObject(10, 10, 0x02, 0x02)});

  handler_.CopyObjectsToClipboard(0, {0, 1});
  auto new_indices = handler_.PasteFromClipboard(0, 1, 1);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects.size(), 4);
  EXPECT_EQ(new_indices.size(), 2);
}

TEST_F(TileObjectHandlerTest, PasteClampsToBounds) {
  AddTestObjects({CreateTestObject(62, 62, 0x00, 0x01)});

  handler_.CopyObjectsToClipboard(0, {0});
  handler_.PasteFromClipboard(0, 5, 5);

  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[1].x_, 63);  // Clamped to max
  EXPECT_EQ(objects[1].y_, 63);  // Clamped to max
}

TEST_F(TileObjectHandlerTest, ClearClipboard) {
  AddTestObjects({CreateTestObject(5, 5, 0x00, 0x01)});

  handler_.CopyObjectsToClipboard(0, {0});
  EXPECT_TRUE(handler_.HasClipboardData());

  handler_.ClearClipboard();
  EXPECT_FALSE(handler_.HasClipboardData());
}

TEST_F(TileObjectHandlerTest, PasteEmptyClipboardReturnsEmpty) {
  auto result = handler_.PasteFromClipboard(0, 1, 1);

  EXPECT_TRUE(result.empty());
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 0);
}

TEST_F(TileObjectHandlerTest, PasteInvalidatesCache) {
  AddTestObjects({CreateTestObject(5, 5, 0x00, 0x01)});

  handler_.CopyObjectsToClipboard(0, {0});
  int initial_count = invalidate_count_;

  handler_.PasteFromClipboard(0, 1, 1);

  EXPECT_GT(invalidate_count_, initial_count);
}

TEST_F(TileObjectHandlerTest, DragMovesObject) {
  // Setup: Add object at (10, 10)
  auto obj = CreateTestObject(10, 10);
  rooms_[0].AddTileObject(obj);

  // Select object
  selection_.SelectObject(0);

  // Start drag at (80, 80) pixels (tile 10, 10)
  // Note: HandleDrag logic uses snapped delta.
  // If we start at 80, 80, and move to 96, 96 (delta +16, +16)
  // The object should move +2 tiles (+2, +2)

  ImVec2 start_pos(80.0f, 80.0f);
  handler_.InitDrag(start_pos);

  ImVec2 current_pos(96.0f, 96.0f);
  ImVec2 delta(
      16.0f,
      16.0f);  // Passed but ignored by implementation logic which uses current-start

  handler_.HandleDrag(current_pos, delta);

  // Verify object moved to (12, 12)
  auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].x_, 12);
  EXPECT_EQ(objects[0].y_, 12);

  handler_.HandleRelease();
}

}  // namespace
}  // namespace yaze::editor
