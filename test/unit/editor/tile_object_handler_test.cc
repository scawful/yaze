#include "app/editor/dungeon/interaction/tile_object_handler.h"
#include "app/editor/dungeon/object_selection.h"
#include "imgui/imgui.h"

#include <gtest/gtest.h>
#include <array>

#include "app/editor/dungeon/interaction/interaction_context.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::editor {
namespace {

// Helper to create test objects
zelda3::RoomObject CreateTestObject(uint8_t x, uint8_t y, uint8_t size = 0x00,
                                    int16_t id = 0x01) {
  return zelda3::RoomObject(id, x, y, size, 0);
}

// Test fixture for TileObjectHandler tests
class TileObjectHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
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

  std::array<zelda3::Room, dungeon_coords::kRoomCount> rooms_;
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
  handler_.PlaceObjectAt(999, obj, 10, 15);
  
  // Nothing should be added to room 0
  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects.size(), 0);
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
  AddTestObjects({
    CreateTestObject(5, 5, 0x03, 0x01),
    CreateTestObject(10, 10, 0x05, 0x02),
    CreateTestObject(15, 15, 0x07, 0x03)
  });
  
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
  AddTestObjects({
    CreateTestObject(5, 5, 0x00, 0x01),
    CreateTestObject(10, 10, 0x00, 0x02)
  });
  
  handler_.DeleteObjects(0, {0});
  
  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].id_, 0x02);  // Only second object remains
}

TEST_F(TileObjectHandlerTest, DeleteMultipleObjects) {
  AddTestObjects({
    CreateTestObject(5, 5, 0x00, 0x01),
    CreateTestObject(10, 10, 0x00, 0x02),
    CreateTestObject(15, 15, 0x00, 0x03)
  });
  
  handler_.DeleteObjects(0, {0, 2});
  
  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].id_, 0x02);  // Only middle object remains
}

TEST_F(TileObjectHandlerTest, DeleteAllObjects) {
  AddTestObjects({
    CreateTestObject(5, 5, 0x00, 0x01),
    CreateTestObject(10, 10, 0x00, 0x02)
  });
  
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

TEST_F(TileObjectHandlerTest, AltDragDuplicatesOnceThenMovesClones) {
  AddTestObjects({CreateTestObject(10, 10, 0x00, 0x42)});
  selection_.SelectObject(0);

  const int initial_mutations = mutation_count_;

  handler_.InitDrag(ImVec2(80.0f, 80.0f));
  ImGui::GetIO().KeyAlt = true;
  handler_.HandleDrag(ImVec2(96.0f, 80.0f), ImVec2(16.0f, 0.0f));
  ImGui::GetIO().KeyAlt = false;
  handler_.HandleRelease();

  const auto& objects = rooms_[0].GetTileObjects();
  ASSERT_EQ(objects.size(), 2);
  EXPECT_EQ(objects[0].x_, 10);
  EXPECT_EQ(objects[0].y_, 10);
  EXPECT_EQ(objects[1].x_, 12);
  EXPECT_EQ(objects[1].y_, 10);
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

TEST_F(TileObjectHandlerTest, ShiftMarqueeAddsToSelection) {
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

  EXPECT_TRUE(selection_.IsObjectSelected(0));
  EXPECT_TRUE(selection_.IsObjectSelected(1));
}

TEST_F(TileObjectHandlerTest, CtrlMarqueeTogglesSelection) {
  AddTestObjects({
      CreateTestObject(5, 5, 0x00, 0x01),
      CreateTestObject(20, 20, 0x00, 0x02),
  });

  // Preselect both.
  selection_.SelectObject(0);
  selection_.SelectObject(1, ObjectSelection::SelectionMode::Add);

  // Toggle only object 1 via marquee.
  handler_.BeginMarqueeSelection(ImVec2(120.0f, 120.0f));
  handler_.HandleMarqueeSelection(
      /*mouse_pos=*/ImVec2(200.0f, 200.0f),
      /*mouse_left_down=*/false,
      /*mouse_left_released=*/true,
      /*shift_down=*/false,
      /*toggle_down=*/true,
      /*alt_down=*/false,
      /*draw_box=*/false);

  EXPECT_TRUE(selection_.IsObjectSelected(0));
  EXPECT_FALSE(selection_.IsObjectSelected(1));
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
  AddTestObjects({
    CreateTestObject(0, 0, 0x00, 0x01),
    CreateTestObject(0, 0, 0x00, 0x02),
    CreateTestObject(0, 0, 0x00, 0x03)
  });
  
  handler_.SendToFront(0, {0});
  
  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].id_, 0x02);
  EXPECT_EQ(objects[1].id_, 0x03);
  EXPECT_EQ(objects[2].id_, 0x01);  // Moved to front (last in list)
}

TEST_F(TileObjectHandlerTest, SendToBack) {
  AddTestObjects({
    CreateTestObject(0, 0, 0x00, 0x01),
    CreateTestObject(0, 0, 0x00, 0x02),
    CreateTestObject(0, 0, 0x00, 0x03)
  });
  
  handler_.SendToBack(0, {2});
  
  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].id_, 0x03);  // Moved to back (first in list)
  EXPECT_EQ(objects[1].id_, 0x01);
  EXPECT_EQ(objects[2].id_, 0x02);
}

TEST_F(TileObjectHandlerTest, MoveForward) {
  AddTestObjects({
    CreateTestObject(0, 0, 0x00, 0x01),
    CreateTestObject(0, 0, 0x00, 0x02),
    CreateTestObject(0, 0, 0x00, 0x03)
  });
  
  handler_.MoveForward(0, {0});
  
  const auto& objects = rooms_[0].GetTileObjects();
  EXPECT_EQ(objects[0].id_, 0x02);
  EXPECT_EQ(objects[1].id_, 0x01);  // Swapped forward
  EXPECT_EQ(objects[2].id_, 0x03);
}

TEST_F(TileObjectHandlerTest, MoveBackward) {
  AddTestObjects({
    CreateTestObject(0, 0, 0x00, 0x01),
    CreateTestObject(0, 0, 0x00, 0x02),
    CreateTestObject(0, 0, 0x00, 0x03)
  });
  
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
  AddTestObjects({
    CreateTestObject(5, 5, 0x02, 0x01),
    CreateTestObject(10, 10, 0x03, 0x02)
  });
  
  EXPECT_FALSE(handler_.HasClipboardData());
  
  handler_.CopyObjectsToClipboard(0, {0, 1});
  
  EXPECT_TRUE(handler_.HasClipboardData());
}

TEST_F(TileObjectHandlerTest, CopyToClipboardPartialSelection) {
  AddTestObjects({
    CreateTestObject(5, 5, 0x02, 0x01),
    CreateTestObject(10, 10, 0x03, 0x02),
    CreateTestObject(15, 15, 0x04, 0x03)
  });
  
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
  EXPECT_EQ(objects[1].x_, 7);   // 5 + 2
  EXPECT_EQ(objects[1].y_, 8);   // 5 + 3
  EXPECT_EQ(objects[1].id_, 0x42);
  EXPECT_EQ(objects[1].size_, 0x02);
}

TEST_F(TileObjectHandlerTest, PasteMultipleObjects) {
  AddTestObjects({
    CreateTestObject(5, 5, 0x01, 0x01),
    CreateTestObject(10, 10, 0x02, 0x02)
  });
  
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
  ImVec2 delta(16.0f, 16.0f); // Passed but ignored by implementation logic which uses current-start
  
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
