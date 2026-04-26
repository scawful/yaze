#include "app/editor/dungeon/interaction/interaction_coordinator.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <memory>

#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "app/editor/dungeon/object_selection.h"
#include "app/gui/canvas/canvas.h"
#include "zelda3/dungeon/door_types.h"
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
    ctx_.on_mutation = [this]() {
      mutation_count_++;
    };
    ctx_.on_invalidate_cache = [this]() {
      invalidation_count_++;
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
  int mutation_count_ = 0;
  int invalidation_count_ = 0;
};

size_t CountSelectedType(const std::vector<SelectedEntity>& entities,
                         EntityType type) {
  return static_cast<size_t>(std::count_if(
      entities.begin(), entities.end(),
      [type](SelectedEntity entity) { return entity.type == type; }));
}

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

TEST_F(InteractionCoordinatorTest, AltClickClearsSelectionOverEntity) {
  rooms_[0].GetSprites().push_back(
      zelda3::Sprite(/*id=*/0x12, /*x=*/5, /*y=*/5, 0, 0));
  coordinator_.SelectEntity(EntityType::Sprite, 0);
  ASSERT_TRUE(coordinator_.HasEntitySelection());

  ImGui::GetIO().KeyAlt = true;

  ASSERT_TRUE(coordinator_.HandleClick(80, 80));
  EXPECT_FALSE(coordinator_.HasEntitySelection());
  EXPECT_FALSE(selection_.HasSelection());

  ImGui::GetIO().KeyAlt = false;
}

TEST_F(InteractionCoordinatorTest, CtrlAltClickCyclesOverlappingEntities) {
  rooms_[0].GetSprites().push_back(
      zelda3::Sprite(/*id=*/0x12, /*x=*/5, /*y=*/5, 0, 0));
  zelda3::PotItem item;
  item.position = static_cast<uint16_t>((5 << 8) | 20);
  item.item = 0x04;
  rooms_[0].GetPotItems().push_back(item);

  ImGui::GetIO().KeyAlt = true;
  ImGui::GetIO().KeyCtrl = true;

  ASSERT_TRUE(coordinator_.HandleClick(80, 80));
  EXPECT_EQ(coordinator_.GetSelectedEntity().type, EntityType::Sprite);
  EXPECT_EQ(coordinator_.GetSelectedEntity().index, 0u);
  EXPECT_FALSE(selection_.HasSelection());

  ASSERT_TRUE(coordinator_.HandleClick(80, 80));
  EXPECT_EQ(coordinator_.GetSelectedEntity().type, EntityType::Item);
  EXPECT_EQ(coordinator_.GetSelectedEntity().index, 0u);

  ImGui::GetIO().KeyAlt = false;
  ImGui::GetIO().KeyCtrl = false;
}

TEST_F(InteractionCoordinatorTest, NudgeSelectedSpriteMovesAndInvalidates) {
  rooms_[0].GetSprites().push_back(
      zelda3::Sprite(/*id=*/0x12, /*x=*/5, /*y=*/5, 0, 0));

  coordinator_.SelectEntity(EntityType::Sprite, 0);
  const int initial_mutations = mutation_count_;
  const int initial_invalidations = invalidation_count_;

  ASSERT_TRUE(coordinator_.NudgeSelected(1, -1));
  const auto& sprite = rooms_[0].GetSprites()[0];
  EXPECT_EQ(sprite.x(), 6);
  EXPECT_EQ(sprite.y(), 4);
  EXPECT_GT(mutation_count_, initial_mutations);
  EXPECT_GT(invalidation_count_, initial_invalidations);
  EXPECT_TRUE(rooms_[0].sprites_dirty());
}

TEST_F(InteractionCoordinatorTest, NudgeSelectedItemUsesEncodableGrid) {
  zelda3::PotItem item;
  item.position = static_cast<uint16_t>((5 << 8) | 20);
  item.item = 0x04;
  rooms_[0].GetPotItems().push_back(item);

  coordinator_.SelectEntity(EntityType::Item, 0);

  ASSERT_TRUE(coordinator_.NudgeSelected(1, 1));
  const auto& moved = rooms_[0].GetPotItems()[0];
  EXPECT_EQ(moved.GetPixelX(), 88);
  EXPECT_EQ(moved.GetPixelY(), 96);
  EXPECT_TRUE(rooms_[0].pot_items_dirty());
}

TEST_F(InteractionCoordinatorTest, NudgeSelectedDoorMovesAlongDoorAxisOnly) {
  zelda3::Room::Door door;
  door.position = 1;
  door.type = zelda3::DoorType::NormalDoor;
  door.direction = zelda3::DoorDirection::North;
  auto [byte1, byte2] = door.EncodeBytes();
  door.byte1 = byte1;
  door.byte2 = byte2;
  rooms_[0].GetDoors().push_back(door);

  coordinator_.SelectEntity(EntityType::Door, 0);

  EXPECT_FALSE(coordinator_.NudgeSelected(0, 1));
  ASSERT_TRUE(coordinator_.NudgeSelected(1, 0));
  const auto& moved = rooms_[0].GetDoors()[0];
  EXPECT_EQ(moved.position, 2);
  const auto [expected_b1, expected_b2] = moved.EncodeBytes();
  EXPECT_EQ(moved.byte1, expected_b1);
  EXPECT_EQ(moved.byte2, expected_b2);
}

TEST_F(InteractionCoordinatorTest, SelectEntitiesInRectCapturesMixedEntities) {
  zelda3::Room::Door door;
  door.position = 1;
  door.type = zelda3::DoorType::NormalDoor;
  door.direction = zelda3::DoorDirection::North;
  auto [byte1, byte2] = door.EncodeBytes();
  door.byte1 = byte1;
  door.byte2 = byte2;
  rooms_[0].GetDoors().push_back(door);
  rooms_[0].GetSprites().push_back(
      zelda3::Sprite(/*id=*/0x12, /*x=*/5, /*y=*/5, 0, 0));
  zelda3::PotItem item;
  item.position = static_cast<uint16_t>((5 << 8) | 20);
  item.item = 0x04;
  rooms_[0].GetPotItems().push_back(item);

  coordinator_.SelectEntitiesInRect({0, 0, 512, 512}, /*additive=*/false,
                                    /*toggle=*/false);

  const auto& selected = coordinator_.GetSelectedEntities();
  EXPECT_EQ(selected.size(), 3u);
  EXPECT_EQ(CountSelectedType(selected, EntityType::Door), 1u);
  EXPECT_EQ(CountSelectedType(selected, EntityType::Sprite), 1u);
  EXPECT_EQ(CountSelectedType(selected, EntityType::Item), 1u);
  EXPECT_TRUE(coordinator_.HasEntitySelection());
}

TEST_F(InteractionCoordinatorTest, NudgeSelectedMovesMixedEntitySelection) {
  zelda3::Room::Door door;
  door.position = 1;
  door.type = zelda3::DoorType::NormalDoor;
  door.direction = zelda3::DoorDirection::North;
  auto [byte1, byte2] = door.EncodeBytes();
  door.byte1 = byte1;
  door.byte2 = byte2;
  rooms_[0].GetDoors().push_back(door);
  rooms_[0].GetSprites().push_back(
      zelda3::Sprite(/*id=*/0x12, /*x=*/5, /*y=*/5, 0, 0));
  zelda3::PotItem item;
  item.position = static_cast<uint16_t>((5 << 8) | 20);
  item.item = 0x04;
  rooms_[0].GetPotItems().push_back(item);
  coordinator_.SelectEntitiesInRect({0, 0, 512, 512}, /*additive=*/false,
                                    /*toggle=*/false);

  ASSERT_TRUE(coordinator_.NudgeSelected(1, 1));

  EXPECT_EQ(rooms_[0].GetDoors()[0].position, 2);
  EXPECT_EQ(rooms_[0].GetSprites()[0].x(), 6);
  EXPECT_EQ(rooms_[0].GetSprites()[0].y(), 6);
  EXPECT_EQ(rooms_[0].GetPotItems()[0].GetPixelX(), 88);
  EXPECT_EQ(rooms_[0].GetPotItems()[0].GetPixelY(), 96);
  EXPECT_GT(invalidation_count_, 0);
  EXPECT_TRUE(rooms_[0].sprites_dirty());
  EXPECT_TRUE(rooms_[0].pot_items_dirty());
}

TEST_F(InteractionCoordinatorTest, DeleteSelectedEntityDeletesMixedSelection) {
  zelda3::Room::Door door;
  door.position = 1;
  door.type = zelda3::DoorType::NormalDoor;
  door.direction = zelda3::DoorDirection::North;
  rooms_[0].GetDoors().push_back(door);
  rooms_[0].GetSprites().push_back(
      zelda3::Sprite(/*id=*/0x12, /*x=*/5, /*y=*/5, 0, 0));
  zelda3::PotItem item;
  item.position = static_cast<uint16_t>((5 << 8) | 20);
  item.item = 0x04;
  rooms_[0].GetPotItems().push_back(item);
  coordinator_.SelectEntitiesInRect({0, 0, 512, 512}, /*additive=*/false,
                                    /*toggle=*/false);

  coordinator_.DeleteSelectedEntity();

  EXPECT_TRUE(rooms_[0].GetDoors().empty());
  EXPECT_TRUE(rooms_[0].GetSprites().empty());
  EXPECT_TRUE(rooms_[0].GetPotItems().empty());
  EXPECT_FALSE(coordinator_.HasEntitySelection());
}

TEST_F(InteractionCoordinatorTest,
       PlainClickOnSelectedObjectPreservesMixedSelection) {
  rooms_[0].AddTileObject(zelda3::RoomObject{0x01, 20, 20, 0x12, 0});
  rooms_[0].GetSprites().push_back(
      zelda3::Sprite(/*id=*/0x12, /*x=*/5, /*y=*/5, 0, 0));
  selection_.SelectObject(0, ObjectSelection::SelectionMode::Add);
  coordinator_.SetSelectedEntities({SelectedEntity{EntityType::Sprite, 0}});

  ImGui::GetIO().KeyShift = false;
  ImGui::GetIO().KeyCtrl = false;
  ImGui::GetIO().KeySuper = false;
  ImGui::GetIO().KeyAlt = false;

  ASSERT_TRUE(coordinator_.HandleClick(20 * 8, 20 * 8));

  EXPECT_TRUE(selection_.IsObjectSelected(0));
  ASSERT_EQ(coordinator_.GetSelectedEntities().size(), 1u);
  EXPECT_EQ(coordinator_.GetSelectedEntities()[0].type, EntityType::Sprite);
  EXPECT_EQ(coordinator_.GetSelectedEntities()[0].index, 0u);
}

TEST_F(InteractionCoordinatorTest,
       PlainClickOnSelectedEntityPreservesMixedSelection) {
  rooms_[0].AddTileObject(zelda3::RoomObject{0x01, 20, 20, 0x12, 0});
  rooms_[0].GetSprites().push_back(
      zelda3::Sprite(/*id=*/0x12, /*x=*/5, /*y=*/5, 0, 0));
  selection_.SelectObject(0, ObjectSelection::SelectionMode::Add);
  coordinator_.SetSelectedEntities({SelectedEntity{EntityType::Sprite, 0}});

  ImGui::GetIO().KeyShift = false;
  ImGui::GetIO().KeyCtrl = false;
  ImGui::GetIO().KeySuper = false;
  ImGui::GetIO().KeyAlt = false;

  ASSERT_TRUE(coordinator_.HandleClick(5 * 16, 5 * 16));

  EXPECT_TRUE(selection_.IsObjectSelected(0));
  ASSERT_EQ(coordinator_.GetSelectedEntities().size(), 1u);
  EXPECT_EQ(coordinator_.GetSelectedEntities()[0].type, EntityType::Sprite);
  EXPECT_EQ(coordinator_.GetSelectedEntities()[0].index, 0u);
}

TEST_F(InteractionCoordinatorTest, GroupDragMovesMixedEntitySelection) {
  rooms_[0].GetSprites().push_back(
      zelda3::Sprite(/*id=*/0x12, /*x=*/5, /*y=*/5, 0, 0));
  zelda3::PotItem item;
  item.position = static_cast<uint16_t>((5 << 8) | 20);
  item.item = 0x04;
  rooms_[0].GetPotItems().push_back(item);
  coordinator_.SelectEntitiesInRect({0, 0, 512, 512}, /*additive=*/false,
                                    /*toggle=*/false);
  ASSERT_EQ(coordinator_.GetSelectedEntities().size(), 2u);

  coordinator_.BeginSelectionDrag(ImVec2(80.0f, 80.0f));
  coordinator_.HandleDrag(ImVec2(96.0f, 96.0f), ImVec2(16.0f, 16.0f));
  coordinator_.HandleRelease();

  EXPECT_EQ(rooms_[0].GetSprites()[0].x(), 6);
  EXPECT_EQ(rooms_[0].GetSprites()[0].y(), 6);
  EXPECT_EQ(rooms_[0].GetPotItems()[0].GetPixelX(), 88);
  EXPECT_EQ(rooms_[0].GetPotItems()[0].GetPixelY(), 96);
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
