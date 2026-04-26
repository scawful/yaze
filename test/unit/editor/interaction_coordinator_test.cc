#include "app/editor/dungeon/interaction/interaction_coordinator.h"

#include <gtest/gtest.h>
#include <array>
#include <memory>

#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "app/editor/dungeon/object_selection.h"
#include "app/gui/canvas/canvas.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {
namespace {

// Test fixture for InteractionCoordinator tests
class InteractionCoordinatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ImGui::CreateContext();
    ImGui::GetIO().DisplaySize = ImVec2(1024, 768);
    canvas_ = std::make_unique<gui::Canvas>("TestCanvas", ImVec2(512, 512),
                                            gui::CanvasGridSize::k16x16);

    // Rooms use default constructor - no ROM needed for basic tests

    // Set up context
    ctx_.canvas = canvas_.get();
    ctx_.rooms = &rooms_;
    ctx_.current_room_id = 0;
    ctx_.selection = &selection_;
    ctx_.on_entity_changed = [this]() {
      entity_changed_count_++;
    };
    ctx_.on_selection_changed = [this]() {
      selection_changed_count_++;
    };

    coordinator_.SetContext(&ctx_);
  }

  void TearDown() override {
    canvas_.reset();
    ImGui::DestroyContext();
  }

  std::unique_ptr<gui::Canvas> canvas_;
  DungeonRoomStore rooms_;
  ObjectSelection selection_;
  InteractionContext ctx_;
  InteractionCoordinator coordinator_;
  int entity_changed_count_ = 0;
  int selection_changed_count_ = 0;
};

// ============================================================================
// Mode Transition Tests
// ============================================================================

TEST_F(InteractionCoordinatorTest, InitializesInSelectMode) {
  EXPECT_EQ(coordinator_.GetCurrentMode(),
            InteractionCoordinator::Mode::Select);
}

TEST_F(InteractionCoordinatorTest, SetModeToDoor) {
  coordinator_.SetMode(InteractionCoordinator::Mode::PlaceDoor);
  EXPECT_EQ(coordinator_.GetCurrentMode(),
            InteractionCoordinator::Mode::PlaceDoor);
}

TEST_F(InteractionCoordinatorTest, SetModeToSprite) {
  coordinator_.SetMode(InteractionCoordinator::Mode::PlaceSprite);
  EXPECT_EQ(coordinator_.GetCurrentMode(),
            InteractionCoordinator::Mode::PlaceSprite);
}

TEST_F(InteractionCoordinatorTest, SetModeToItem) {
  coordinator_.SetMode(InteractionCoordinator::Mode::PlaceItem);
  EXPECT_EQ(coordinator_.GetCurrentMode(),
            InteractionCoordinator::Mode::PlaceItem);
}

TEST_F(InteractionCoordinatorTest, CancelCurrentModeReturnsToSelect) {
  coordinator_.SetMode(InteractionCoordinator::Mode::PlaceDoor);
  EXPECT_NE(coordinator_.GetCurrentMode(),
            InteractionCoordinator::Mode::Select);

  coordinator_.CancelCurrentMode();
  EXPECT_EQ(coordinator_.GetCurrentMode(),
            InteractionCoordinator::Mode::Select);
}

// ============================================================================
// Entity Selection Tests
// ============================================================================

// Note: HasEntitySelection checks handler selections (door/sprite/item handlers),
// not the coordinator's selected_entity_ member. SelectEntity sets selected_entity_
// and notifies callbacks, but doesn't change handler state.

TEST_F(InteractionCoordinatorTest, InitiallyHasNoEntitySelection) {
  EXPECT_FALSE(coordinator_.HasEntitySelection());
}

TEST_F(InteractionCoordinatorTest, SelectEntityNotifiesCallback) {
  int initial_count = entity_changed_count_;

  coordinator_.SelectEntity(EntityType::Sprite, 3);

  EXPECT_GT(entity_changed_count_, initial_count);
}

TEST_F(InteractionCoordinatorTest, ClearEntitySelectionNotifiesCallback) {
  coordinator_.SelectEntity(EntityType::Sprite, 3);
  EXPECT_GT(entity_changed_count_, 0);
  int initial_count = entity_changed_count_;

  coordinator_.ClearEntitySelection();

  EXPECT_GT(entity_changed_count_, initial_count);
}

// ============================================================================
// Handler Access Tests
// ============================================================================

TEST_F(InteractionCoordinatorTest, CanAccessDoorHandler) {
  auto& handler = coordinator_.door_handler();
  // Should not crash, handler exists
  handler.CancelPlacement();
}

TEST_F(InteractionCoordinatorTest, CanAccessSpriteHandler) {
  auto& handler = coordinator_.sprite_handler();
  handler.CancelPlacement();
}

TEST_F(InteractionCoordinatorTest, CanAccessItemHandler) {
  auto& handler = coordinator_.item_handler();
  handler.CancelPlacement();
}

TEST_F(InteractionCoordinatorTest, CanAccessTileHandler) {
  auto& handler = coordinator_.tile_handler();
  handler.CancelPlacement();
}

// ============================================================================
// Cancel Placement Tests
// ============================================================================

TEST_F(InteractionCoordinatorTest, CancelPlacementResetsAllHandlers) {
  // Activate placement on multiple handlers
  coordinator_.door_handler().BeginPlacement();
  coordinator_.tile_handler().BeginPlacement();

  coordinator_.CancelPlacement();

  EXPECT_FALSE(coordinator_.door_handler().IsPlacementActive());
  EXPECT_FALSE(coordinator_.tile_handler().IsPlacementActive());
}

// ============================================================================
// Context Propagation Tests
// ============================================================================

TEST_F(InteractionCoordinatorTest, SetContextPropagatestoHandlers) {
  InteractionCoordinator fresh_coordinator;
  InteractionContext new_ctx;
  new_ctx.rooms = &rooms_;
  new_ctx.current_room_id = 42;

  fresh_coordinator.SetContext(&new_ctx);

  // Handlers should receive the context
  // Implicitly tested - operations should work without crash
  fresh_coordinator.tile_handler().PlaceObjectAt(
      42, zelda3::RoomObject(0x01, 0, 0, 0, 0), 5, 5);
}

// ============================================================================
// Ghost Preview Tests
// ============================================================================

TEST_F(InteractionCoordinatorTest,
       DrawGhostPreviewsDoesNotCrashWithNoActiveHandler) {
  // No placement active, should be safe
  coordinator_.DrawGhostPreviews();
}

TEST_F(InteractionCoordinatorTest, DrawSelectionHighlightsDoesNotCrash) {
  coordinator_.DrawSelectionHighlights();
}

// ============================================================================
// Entity At Position Tests
// ============================================================================

TEST_F(InteractionCoordinatorTest, GetEntityAtPositionReturnsNulloptOnEmpty) {
  auto result = coordinator_.GetEntityAtPosition(100, 100);

  // No entities in empty room
  EXPECT_FALSE(result.has_value());
}

TEST_F(InteractionCoordinatorTest, AltClickCyclesOverlappingEntities) {
  rooms_[0].GetSprites().push_back(
      zelda3::Sprite(/*id=*/0x12, /*x=*/5, /*y=*/5, 0, 0));
  zelda3::PotItem item;
  item.position = static_cast<uint16_t>((5 << 8) | 20);
  item.item = 0x04;
  rooms_[0].GetPotItems().push_back(item);

  ImGui::GetIO().KeyAlt = true;

  ASSERT_TRUE(coordinator_.HandleClick(80, 80));
  EXPECT_EQ(coordinator_.GetSelectedEntity().type, EntityType::Sprite);
  EXPECT_EQ(coordinator_.GetSelectedEntity().index, 0u);
  EXPECT_FALSE(selection_.HasSelection());

  ASSERT_TRUE(coordinator_.HandleClick(80, 80));
  EXPECT_EQ(coordinator_.GetSelectedEntity().type, EntityType::Item);
  EXPECT_EQ(coordinator_.GetSelectedEntity().index, 0u);

  ImGui::GetIO().KeyAlt = false;
}

// ============================================================================
// Mode Changing Clears Old Placement
// ============================================================================

TEST_F(InteractionCoordinatorTest, ChangingModeCancelsPreviousPlacement) {
  coordinator_.SetMode(InteractionCoordinator::Mode::PlaceDoor);
  coordinator_.door_handler().BeginPlacement();
  EXPECT_TRUE(coordinator_.door_handler().IsPlacementActive());

  coordinator_.SetMode(InteractionCoordinator::Mode::PlaceSprite);

  // Previous placement should be cancelled
  EXPECT_FALSE(coordinator_.door_handler().IsPlacementActive());
}

}  // namespace
}  // namespace yaze::editor
