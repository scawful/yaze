#include <gtest/gtest.h>

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

}  // namespace
}  // namespace yaze::editor
