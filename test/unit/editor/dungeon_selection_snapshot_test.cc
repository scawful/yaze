#include <gtest/gtest.h>

#include <vector>

#include "app/editor/dungeon/dungeon_object_interaction.h"
#include "app/editor/dungeon/dungeon_selection_snapshot.h"
#include "app/gui/canvas/canvas.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {
namespace {

class DungeonSelectionSnapshotTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto& room = rooms_[0];
    room.AddTileObject(zelda3::RoomObject{0x01, 10, 10, 0x12, 0});
    room.AddTileObject(zelda3::RoomObject{0x02, 20, 10, 0x14, 2});
    room.AddDoor(zelda3::Room::Door{.position = 1,
                                    .type = zelda3::DoorType::NormalDoor,
                                    .direction = zelda3::DoorDirection::North,
                                    .byte1 = 0,
                                    .byte2 = 0});
    room.GetSprites().emplace_back(0x42, 5, 6, 0, 0);
    room.GetPotItems().push_back(zelda3::PotItem{0x1234, 0x09});
    interaction_.SetCurrentRoom(&rooms_, 0);
  }

  DungeonRoomStore rooms_;
  gui::Canvas canvas_{"SelectionSnapshotCanvas", ImVec2(512, 512)};
  DungeonObjectInteraction interaction_{&canvas_};
};

TEST_F(DungeonSelectionSnapshotTest, EmptySelectionReturnsNoneSummary) {
  const auto snapshot = BuildDungeonSelectionSnapshot(interaction_, &rooms_, 0);

  EXPECT_EQ(snapshot.kind, DungeonSelectionKind::None);
  EXPECT_EQ(snapshot.count, 0u);
  EXPECT_FALSE(snapshot.HasSelection());
  EXPECT_EQ(GetDungeonSelectionSummaryText(snapshot), "No selection");
}

TEST_F(DungeonSelectionSnapshotTest,
       SingleObjectSelectionReportsLayerAndPrimaryIndex) {
  interaction_.SetSelectedObjects({1});

  const auto snapshot = BuildDungeonSelectionSnapshot(interaction_, &rooms_, 0);

  ASSERT_TRUE(snapshot.primary_object_index.has_value());
  EXPECT_EQ(snapshot.kind, DungeonSelectionKind::ObjectSingle);
  EXPECT_EQ(snapshot.count, 1u);
  EXPECT_EQ(*snapshot.primary_object_index, 1u);
  EXPECT_EQ(snapshot.selection_layer, 2);
  EXPECT_TRUE(snapshot.HasObjectSelection());
  EXPECT_EQ(GetDungeonSelectionSummaryText(snapshot), "1 obj, L3");
  EXPECT_STREQ(GetDungeonSelectionKindLabel(snapshot.kind), "object");
}

TEST_F(DungeonSelectionSnapshotTest,
       MultiObjectSelectionUsesObjectPluralLabel) {
  interaction_.SetSelectedObjects({0, 1});

  const auto snapshot = BuildDungeonSelectionSnapshot(interaction_, &rooms_, 0);

  EXPECT_EQ(snapshot.kind, DungeonSelectionKind::ObjectMulti);
  EXPECT_EQ(snapshot.count, 2u);
  EXPECT_EQ(GetDungeonSelectionSummaryText(snapshot), "2 obj");
  EXPECT_STREQ(GetDungeonSelectionKindLabel(snapshot.kind), "objects");
}

TEST_F(DungeonSelectionSnapshotTest, EntitySelectionsMapToInspectorKinds) {
  interaction_.SelectEntity(EntityType::Door, 0);
  auto snapshot = BuildDungeonSelectionSnapshot(interaction_, &rooms_, 0);
  EXPECT_EQ(snapshot.kind, DungeonSelectionKind::Door);
  EXPECT_TRUE(snapshot.HasEntitySelection());
  EXPECT_EQ(GetDungeonSelectionSummaryText(snapshot), "Door");

  interaction_.SelectEntity(EntityType::Sprite, 0);
  snapshot = BuildDungeonSelectionSnapshot(interaction_, &rooms_, 0);
  EXPECT_EQ(snapshot.kind, DungeonSelectionKind::Sprite);
  EXPECT_EQ(GetDungeonSelectionSummaryText(snapshot), "Sprite");

  interaction_.SelectEntity(EntityType::Item, 0);
  snapshot = BuildDungeonSelectionSnapshot(interaction_, &rooms_, 0);
  EXPECT_EQ(snapshot.kind, DungeonSelectionKind::Item);
  EXPECT_EQ(GetDungeonSelectionSummaryText(snapshot), "Item");
}

TEST_F(DungeonSelectionSnapshotTest, EntityMultiSelectionReportsCounts) {
  interaction_.entity_coordinator().SelectEntitiesInRect(
      {0, 0, 512, 512}, /*additive=*/false, /*toggle=*/false);

  const auto snapshot = BuildDungeonSelectionSnapshot(interaction_, &rooms_, 0);

  EXPECT_EQ(snapshot.kind, DungeonSelectionKind::EntityMulti);
  EXPECT_EQ(snapshot.count, 3u);
  EXPECT_EQ(snapshot.object_count, 0u);
  EXPECT_EQ(snapshot.door_count, 1u);
  EXPECT_EQ(snapshot.sprite_count, 1u);
  EXPECT_EQ(snapshot.item_count, 1u);
  EXPECT_TRUE(snapshot.HasEntitySelection());
  EXPECT_FALSE(snapshot.HasObjectSelection());
  EXPECT_EQ(GetDungeonSelectionSummaryText(snapshot),
            "3 selected: 1 door, 1 sprite, 1 item");
}

TEST_F(DungeonSelectionSnapshotTest, MixedSelectionReportsObjectsAndEntities) {
  interaction_.SetSelectedObjects({0});
  interaction_.entity_coordinator().SelectEntitiesInRect(
      {0, 0, 512, 512}, /*additive=*/true, /*toggle=*/false);

  const auto snapshot = BuildDungeonSelectionSnapshot(interaction_, &rooms_, 0);

  EXPECT_EQ(snapshot.kind, DungeonSelectionKind::Mixed);
  EXPECT_EQ(snapshot.count, 4u);
  EXPECT_EQ(snapshot.object_count, 1u);
  EXPECT_EQ(snapshot.door_count, 1u);
  EXPECT_EQ(snapshot.sprite_count, 1u);
  EXPECT_EQ(snapshot.item_count, 1u);
  EXPECT_TRUE(snapshot.HasEntitySelection());
  EXPECT_FALSE(snapshot.HasObjectSelection());
  EXPECT_EQ(GetDungeonSelectionSummaryText(snapshot),
            "4 selected: 1 obj, 1 door, 1 sprite, 1 item");
}

TEST_F(DungeonSelectionSnapshotTest,
       SelectingObjectsClearsAnyExistingEntitySelection) {
  interaction_.SelectEntity(EntityType::Sprite, 0);
  ASSERT_TRUE(interaction_.HasEntitySelection());

  interaction_.SetSelectedObjects({0});

  EXPECT_FALSE(interaction_.HasEntitySelection());
  EXPECT_EQ(interaction_.GetSelectedEntity().type, EntityType::None);

  const auto snapshot = BuildDungeonSelectionSnapshot(interaction_, &rooms_, 0);
  EXPECT_EQ(snapshot.kind, DungeonSelectionKind::ObjectSingle);
  EXPECT_EQ(snapshot.count, 1u);
}

// Selections are indices, so a selection kept across a room change names an
// unrelated object in the new room.
TEST_F(DungeonSelectionSnapshotTest, RoomChangeClearsObjectAndEntitySelection) {
  rooms_[1].AddTileObject(zelda3::RoomObject{0x03, 4, 4, 0x00, 0});
  rooms_[1].AddTileObject(zelda3::RoomObject{0x04, 8, 8, 0x00, 0});
  interaction_.SetSelectedObjects({1});

  // The canvas binds the same room every frame; that keeps the selection.
  interaction_.SetCurrentRoom(&rooms_, 0);
  EXPECT_EQ(interaction_.GetSelectedObjectIndices(), std::vector<size_t>{1});

  interaction_.SetCurrentRoom(&rooms_, 1);
  EXPECT_TRUE(interaction_.GetSelectedObjectIndices().empty());

  interaction_.SetCurrentRoom(&rooms_, 0);
  interaction_.SelectEntity(EntityType::Sprite, 0);
  ASSERT_TRUE(interaction_.HasEntitySelection());
  interaction_.SetCurrentRoom(&rooms_, 1);
  EXPECT_FALSE(interaction_.HasEntitySelection());
}

TEST_F(DungeonSelectionSnapshotTest, RoomChangeKeepsObjectPlacementActive) {
  interaction_.SetPreviewObject(zelda3::RoomObject{0x01, 0, 0, 0x12, 0},
                                /*loaded=*/true);
  ASSERT_NE(interaction_.GetPlacementPreview(), nullptr);

  interaction_.SetCurrentRoom(&rooms_, 1);

  EXPECT_NE(interaction_.GetPlacementPreview(), nullptr);
}

// Cross-room navigation binds the room before selecting, so the next frame's
// bind of the same room keeps the selection.
TEST_F(DungeonSelectionSnapshotTest, SelectionMadeAfterBindingSurvivesRedraw) {
  rooms_[1].AddTileObject(zelda3::RoomObject{0x03, 4, 4, 0x00, 0});
  interaction_.SetCurrentRoom(&rooms_, 1);
  interaction_.SetSelectedObjects({0});

  interaction_.SetCurrentRoom(&rooms_, 1);

  EXPECT_EQ(interaction_.GetSelectedObjectIndices(), std::vector<size_t>{0});
}

TEST_F(DungeonSelectionSnapshotTest, SameRoomRedrawPreservesMarquee) {
  auto& handler = interaction_.entity_coordinator().tile_handler();
  handler.BeginMarqueeSelection(ImVec2(0, 0));

  interaction_.SetCurrentRoom(&rooms_, 0);

  EXPECT_TRUE(interaction_.IsObjectSelectActive());
  EXPECT_TRUE(handler.context()->selection->IsRectangleSelectionActive());
}

TEST_F(DungeonSelectionSnapshotTest, RoomChangeCancelsMarqueeBeforeRelease) {
  rooms_[1].AddTileObject(zelda3::RoomObject{0x03, 4, 4, 0x00, 0});
  auto& handler = interaction_.entity_coordinator().tile_handler();
  handler.BeginMarqueeSelection(ImVec2(0, 0));

  interaction_.SetCurrentRoom(&rooms_, 1);

  EXPECT_FALSE(interaction_.IsObjectSelectActive());
  handler.HandleMarqueeSelection(ImVec2(64, 64), false, true, false, false,
                                 false, false);
  EXPECT_TRUE(interaction_.GetSelectedObjectIndices().empty());
}

TEST_F(DungeonSelectionSnapshotTest,
       RoomStoreChangeClearsSelectionsAndCancelsMarquee) {
  DungeonRoomStore replacement;
  replacement[0].AddTileObject(zelda3::RoomObject{0x03, 4, 4, 0x00, 0});
  interaction_.SetSelectedObjects({0});
  interaction_.entity_coordinator().SelectEntity(EntityType::Sprite, 0);
  ASSERT_TRUE(interaction_.IsObjectSelected(0));
  ASSERT_TRUE(interaction_.HasEntitySelection());
  auto& handler = interaction_.entity_coordinator().tile_handler();
  handler.BeginMarqueeSelection(ImVec2(0, 0));

  interaction_.SetCurrentRoom(&replacement, 0);

  EXPECT_FALSE(interaction_.IsObjectSelectActive());
  EXPECT_FALSE(interaction_.HasEntitySelection());
  handler.HandleMarqueeSelection(ImVec2(64, 64), false, true, false, false,
                                 false, false);
  EXPECT_TRUE(interaction_.GetSelectedObjectIndices().empty());
}

TEST_F(DungeonSelectionSnapshotTest,
       RoomChangeFinishesTileDragAgainstOriginalContext) {
  rooms_[1].AddTileObject(zelda3::RoomObject{0x03, 4, 4, 0x00, 0});
  auto& handler = interaction_.entity_coordinator().tile_handler();
  auto* ctx = handler.context();
  std::vector<int> completed_rooms;
  interaction_.SetCacheInvalidationCallback([&]() {
    if (interaction_.mode_manager().GetMode() !=
        InteractionMode::DraggingObjects) {
      EXPECT_EQ(ctx->rooms, &rooms_);
      EXPECT_EQ(ctx->last_invalidation_domain, MutationDomain::kTileObjects);
      completed_rooms.push_back(ctx->current_room_id);
    }
  });
  interaction_.SetSelectedObjects({0});
  interaction_.mode_manager().SetMode(InteractionMode::DraggingObjects);
  handler.InitDrag(ImVec2(80, 80));
  handler.HandleDrag(ImVec2(88, 80), ImVec2(8, 0));
  ASSERT_EQ(rooms_[0].GetTileObjects()[0].x(), 11);

  interaction_.SetCurrentRoom(&rooms_, 0);
  EXPECT_TRUE(completed_rooms.empty());
  EXPECT_EQ(interaction_.mode_manager().GetMode(),
            InteractionMode::DraggingObjects);

  interaction_.SetCurrentRoom(&rooms_, 1);

  EXPECT_EQ(completed_rooms, std::vector<int>{0});
  EXPECT_EQ(interaction_.mode_manager().GetMode(), InteractionMode::Select);
  handler.HandleRelease();
  EXPECT_EQ(completed_rooms, std::vector<int>{0});
  EXPECT_EQ(rooms_[1].GetTileObjects()[0].x(), 4);
}

TEST_F(DungeonSelectionSnapshotTest,
       RoomStoreChangeFinishesTileDragAgainstOriginalStore) {
  DungeonRoomStore replacement;
  replacement[0].AddTileObject(zelda3::RoomObject{0x03, 4, 4, 0x00, 0});
  auto& handler = interaction_.entity_coordinator().tile_handler();
  auto* ctx = handler.context();
  int completions = 0;
  interaction_.SetCacheInvalidationCallback([&]() {
    if (interaction_.mode_manager().GetMode() == InteractionMode::Select) {
      EXPECT_EQ(ctx->rooms, &rooms_);
      EXPECT_EQ(ctx->current_room_id, 0);
      ++completions;
    }
  });
  interaction_.SetSelectedObjects({0});
  interaction_.mode_manager().SetMode(InteractionMode::DraggingObjects);
  handler.InitDrag(ImVec2(80, 80));
  handler.HandleDrag(ImVec2(88, 80), ImVec2(8, 0));

  interaction_.SetCurrentRoom(&replacement, 0);

  EXPECT_EQ(completions, 1);
  handler.HandleRelease();
  EXPECT_EQ(completions, 1);
  EXPECT_EQ(replacement[0].GetTileObjects()[0].x(), 4);
}

TEST_F(DungeonSelectionSnapshotTest,
       RoomChangeFinishesPaintStrokeButKeepsPaintTool) {
  const auto* ctx = interaction_.entity_coordinator().tile_handler().context();
  for (auto mode :
       {InteractionMode::PaintCollision, InteractionMode::PaintWaterFill}) {
    interaction_.SetCurrentRoom(&rooms_, 0);
    interaction_.mode_manager().SetMode(mode);
    auto& state = interaction_.mode_manager().GetModeState();
    state.is_painting = true;
    state.paint_mutation_started = true;
    state.paint_last_tile_x = 20;
    state.paint_last_tile_y = 30;
    int completions = 0;
    interaction_.SetCacheInvalidationCallback([&]() {
      EXPECT_EQ(ctx->rooms, &rooms_);
      EXPECT_EQ(ctx->current_room_id, 0);
      EXPECT_EQ(ctx->last_invalidation_domain,
                mode == InteractionMode::PaintCollision
                    ? MutationDomain::kCustomCollision
                    : MutationDomain::kWaterFill);
      EXPECT_FALSE(state.is_painting);
      EXPECT_FALSE(state.paint_mutation_started);
      ++completions;
    });

    interaction_.SetCurrentRoom(&rooms_, 0);
    EXPECT_TRUE(state.is_painting);
    EXPECT_EQ(completions, 0);
    interaction_.SetCurrentRoom(&rooms_, 1);

    EXPECT_EQ(completions, 1);
    EXPECT_EQ(interaction_.mode_manager().GetMode(), mode);
    EXPECT_FALSE(state.is_painting);
    EXPECT_FALSE(state.paint_mutation_started);
    EXPECT_EQ(state.paint_last_tile_x, -1);
    EXPECT_EQ(state.paint_last_tile_y, -1);
    interaction_.SetCacheInvalidationCallback({});
  }
}

TEST_F(DungeonSelectionSnapshotTest,
       RoomChangeCancelsSingleEntityDragWithoutCommittingPreview) {
  rooms_[1].GetSprites().emplace_back(0x09, 12, 14, 0, 0);
  auto& handler = interaction_.entity_coordinator().sprite_handler();
  ASSERT_TRUE(handler.HandleClick(80, 96));
  handler.HandleDrag(ImVec2(112, 128), ImVec2(32, 32));

  interaction_.SetCurrentRoom(&rooms_, 1);
  interaction_.entity_coordinator().HandleRelease();

  EXPECT_FALSE(interaction_.HasEntitySelection());
  EXPECT_EQ(rooms_[0].GetSprites()[0].x(), 5);
  EXPECT_EQ(rooms_[0].GetSprites()[0].y(), 6);
  EXPECT_EQ(rooms_[1].GetSprites()[0].x(), 12);
  EXPECT_EQ(rooms_[1].GetSprites()[0].y(), 14);
}

}  // namespace
}  // namespace yaze::editor
