/**
 * @file interaction_delegation_test.cc
 * @brief Integration tests for InteractionCoordinator + TileObjectHandler delegation
 *
 * These tests verify that DungeonObjectInteraction correctly delegates
 * operations through InteractionCoordinator to TileObjectHandler.
 */

#include <gtest/gtest.h>
#include <array>
#include <vector>

#include "app/editor/dungeon/dungeon_object_interaction.h"
#include "app/editor/dungeon/interaction/interaction_coordinator.h"
#include "app/editor/dungeon/interaction/tile_object_handler.h"
#include "app/editor/dungeon/object_selection.h"
#include "app/gui/canvas/canvas.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {
namespace {

// Test fixture for interaction delegation tests
class InteractionDelegationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize room with test objects
    auto& room = rooms_[0];
    room.AddTileObject(zelda3::RoomObject{0x55, 10, 10, 0x00, 0});  // Object at (10,10)
    room.AddTileObject(zelda3::RoomObject{0x02, 20, 20, 0x02, 0});  // Object at (20,20)
    room.AddTileObject(zelda3::RoomObject{0x03, 15, 15, 0x01, 0});  // Object at (15,15)

    // Set up interaction context
    interaction_.SetCurrentRoom(&rooms_, 0);
    
    // Track callbacks
    mutation_count_ = 0;
    invalidate_count_ = 0;
  }

  zelda3::Room& CurrentRoom() { return rooms_[0]; }

  std::array<zelda3::Room, 0x128> rooms_;
  gui::Canvas canvas_{"TestCanvas", ImVec2(512, 512)};
  DungeonObjectInteraction interaction_{&canvas_};
  int mutation_count_ = 0;
  int invalidate_count_ = 0;
};

// =============================================================================
// Coordinator Access Tests
// =============================================================================

TEST_F(InteractionDelegationTest, CanAccessEntityCoordinator) {
  auto& coordinator = interaction_.entity_coordinator();
  
  // Coordinator should be properly initialized
  EXPECT_EQ(coordinator.GetCurrentMode(), InteractionCoordinator::Mode::Select);
}

TEST_F(InteractionDelegationTest, CoordinatorModeTransitionsWork) {
  auto& coordinator = interaction_.entity_coordinator();
  
  coordinator.SetMode(InteractionCoordinator::Mode::PlaceDoor);
  EXPECT_EQ(coordinator.GetCurrentMode(), InteractionCoordinator::Mode::PlaceDoor);
  
  coordinator.SetMode(InteractionCoordinator::Mode::PlaceSprite);
  EXPECT_EQ(coordinator.GetCurrentMode(), InteractionCoordinator::Mode::PlaceSprite);
  
  coordinator.CancelCurrentMode();
  EXPECT_EQ(coordinator.GetCurrentMode(), InteractionCoordinator::Mode::Select);
}

// =============================================================================
// TileObjectHandler Access Tests
// =============================================================================

TEST_F(InteractionDelegationTest, CanAccessTileHandlerViaCoordinator) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  // Handler should be accessible
  EXPECT_FALSE(tile_handler.IsPlacementActive());
}

TEST_F(InteractionDelegationTest, TileHandlerCanMoveObjects) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  // Get initial position
  auto& objects = CurrentRoom().GetTileObjects();
  int original_x = objects[0].x_;
  int original_y = objects[0].y_;
  
  // Move object
  tile_handler.MoveObjects(0, {0}, 3, 5);
  
  // Verify position changed
  EXPECT_EQ(objects[0].x_, original_x + 3);
  EXPECT_EQ(objects[0].y_, original_y + 5);
}

TEST_F(InteractionDelegationTest, TileHandlerCanResizeObjects) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  auto& objects = CurrentRoom().GetTileObjects();
  int original_size = objects[1].size_;
  
  tile_handler.ResizeObjects(0, {1}, 2);
  
  EXPECT_EQ(objects[1].size_, original_size + 2);
}

// =============================================================================
// Selection Through Delegation Tests
// =============================================================================

TEST_F(InteractionDelegationTest, SelectionWorksWithDelegation) {
  // Set up selection
  interaction_.SetSelectedObjects({0, 2});
  
  // Verify selection state
  EXPECT_TRUE(interaction_.IsObjectSelected(0));
  EXPECT_FALSE(interaction_.IsObjectSelected(1));
  EXPECT_TRUE(interaction_.IsObjectSelected(2));
}

TEST_F(InteractionDelegationTest, ClearingSelectionWorksWithDelegation) {
  interaction_.SetSelectedObjects({0, 1});
  EXPECT_GT(interaction_.GetSelectedObjectIndices().size(), 0);
  
  interaction_.ClearSelection();
  EXPECT_EQ(interaction_.GetSelectedObjectIndices().size(), 0);
}

// =============================================================================
// Object Manipulation Integration Tests
// =============================================================================

TEST_F(InteractionDelegationTest, DeleteObjectsUpdatesSelection) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  // Select objects 0 and 2
  interaction_.SetSelectedObjects({0, 2});
  
  // Delete object at index 1
  tile_handler.DeleteObjects(0, {1});
  
  auto& objects = CurrentRoom().GetTileObjects();
  EXPECT_EQ(objects.size(), 2);  // Was 3, now 2
  
  // After deletion, original object 2 is now at index 1
  EXPECT_EQ(objects[0].id_, 0x55);  // Object 0 unchanged
  EXPECT_EQ(objects[1].id_, 0x03);  // Original object 2 now at index 1
}

TEST_F(InteractionDelegationTest, DuplicateObjectsReturnsNewIndices) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  size_t original_count = CurrentRoom().GetTileObjects().size();
  
  auto new_indices = tile_handler.DuplicateObjects(0, {0}, 2, 2);
  
  auto& objects = CurrentRoom().GetTileObjects();
  EXPECT_EQ(objects.size(), original_count + 1);  // One duplicate added
  ASSERT_EQ(new_indices.size(), 1);
  
  // New object should be at the returned index
  size_t new_idx = new_indices[0];
  EXPECT_EQ(objects[new_idx].id_, 0x55);  // Same ID as source
  EXPECT_EQ(objects[new_idx].x_, 12);      // Original was at 10, offset by 2
  EXPECT_EQ(objects[new_idx].y_, 12);      // Original was at 10, offset by 2
}

// =============================================================================
// Z-Order Integration Tests
// =============================================================================

TEST_F(InteractionDelegationTest, SendToFrontChangesObjectOrder) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  auto& objects = CurrentRoom().GetTileObjects();
  int16_t first_id = objects[0].id_;
  
  tile_handler.SendToFront(0, {0});
  
  // First object should now be at the end
  EXPECT_EQ(objects.back().id_, first_id);
}

TEST_F(InteractionDelegationTest, SendToBackChangesObjectOrder) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  auto& objects = CurrentRoom().GetTileObjects();
  int16_t last_id = objects[2].id_;
  
  tile_handler.SendToBack(0, {2});
  
  // Last object should now be at the beginning
  EXPECT_EQ(objects[0].id_, last_id);
}

// =============================================================================
// Property Update Integration Tests
// =============================================================================

TEST_F(InteractionDelegationTest, UpdateObjectIdInvalidatesCache) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  tile_handler.UpdateObjectsId(0, {0}, 0x42);
  
  auto& objects = CurrentRoom().GetTileObjects();
  EXPECT_EQ(objects[0].id_, 0x42);
  EXPECT_FALSE(objects[0].tiles_loaded_);  // Cache should be invalidated
  EXPECT_FALSE(objects[0].all_bgs_) << "Derived flags should refresh on ID change";
}

TEST_F(InteractionDelegationTest, UpdateObjectSizeInvalidatesCache) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  tile_handler.UpdateObjectsSize(0, {1}, 0x0F);
  
  auto& objects = CurrentRoom().GetTileObjects();
  EXPECT_EQ(objects[1].size_, 0x0F);
  EXPECT_FALSE(objects[1].tiles_loaded_);  // Cache should be invalidated
}

TEST_F(InteractionDelegationTest, UpdateObjectLayerChangesLayer) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  tile_handler.UpdateObjectsLayer(0, {0}, 1);
  
  auto& objects = CurrentRoom().GetTileObjects();
  EXPECT_EQ(objects[0].layer_, zelda3::RoomObject::LayerType::BG2);
}

TEST_F(InteractionDelegationTest, UpdateObjectLayerSkipsBothBgObjects) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();

  auto& objects = CurrentRoom().GetTileObjects();
  ASSERT_GE(objects.size(), 3u);

  // Fixture index 2 is id=0x03, which is treated as a structural BothBG object.
  const auto original_layer = objects[2].layer_;
  ASSERT_EQ(original_layer, zelda3::RoomObject::LayerType::BG1);
  ASSERT_TRUE(objects[2].all_bgs_);

  tile_handler.UpdateObjectsLayer(0, {2}, 1);
  EXPECT_EQ(objects[2].layer_, original_layer);
}

// =============================================================================
// Placement Flow Integration Tests
// =============================================================================

TEST_F(InteractionDelegationTest, PlacementModeActivatesCorrectly) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  EXPECT_FALSE(tile_handler.IsPlacementActive());
  
  tile_handler.BeginPlacement();
  EXPECT_TRUE(tile_handler.IsPlacementActive());
  
  tile_handler.CancelPlacement();
  EXPECT_FALSE(tile_handler.IsPlacementActive());
}

TEST_F(InteractionDelegationTest, PlaceObjectAtAddsToRoom) {
  auto& tile_handler = interaction_.entity_coordinator().tile_handler();
  
  size_t original_count = CurrentRoom().GetTileObjects().size();
  
  zelda3::RoomObject new_obj(0x55, 0, 0, 0x03, 0);
  tile_handler.PlaceObjectAt(0, new_obj, 30, 40);
  
  auto& objects = CurrentRoom().GetTileObjects();
  EXPECT_EQ(objects.size(), original_count + 1);
  
  // Find the new object
  bool found = false;
  for (const auto& obj : objects) {
    if (obj.x_ == 30 && obj.y_ == 40 && obj.id_ == 0x55) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Placed object not found at expected position";
}

// =============================================================================
// Multi-Handler Coordination Tests
// =============================================================================

TEST_F(InteractionDelegationTest, AllHandlersAccessible) {
  auto& coordinator = interaction_.entity_coordinator();
  
  // All handlers should be accessible without crash
  auto& door_handler = coordinator.door_handler();
  auto& sprite_handler = coordinator.sprite_handler();
  auto& item_handler = coordinator.item_handler();
  auto& tile_handler = coordinator.tile_handler();
  
  // Basic operations should work
  door_handler.CancelPlacement();
  sprite_handler.CancelPlacement();
  item_handler.CancelPlacement();
  tile_handler.CancelPlacement();
}

TEST_F(InteractionDelegationTest, CancelPlacementClearsAllHandlers) {
  auto& coordinator = interaction_.entity_coordinator();
  
  // Activate multiple handlers
  coordinator.door_handler().BeginPlacement();
  coordinator.tile_handler().BeginPlacement();
  
  EXPECT_TRUE(coordinator.door_handler().IsPlacementActive());
  EXPECT_TRUE(coordinator.tile_handler().IsPlacementActive());
  
  // Cancel all
  coordinator.CancelPlacement();
  
  EXPECT_FALSE(coordinator.door_handler().IsPlacementActive());
  EXPECT_FALSE(coordinator.tile_handler().IsPlacementActive());
}

}  // namespace
}  // namespace editor
}  // namespace yaze
