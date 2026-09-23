#include "app/editor/dungeon/dungeon_selection_edit.h"

#include <climits>
#include <vector>

#include "gtest/gtest.h"
#include "zelda3/dungeon/dungeon_limits.h"

namespace yaze::editor {
namespace {

zelda3::RoomObject Object(int id, int x = 8, int y = 8, int layer = 0) {
  return zelda3::RoomObject(id, x, y, zelda3::CanonicalRoomObjectSize(id, 0),
                            layer);
}

zelda3::Room::Door Door(uint8_t position = 0) {
  auto door = zelda3::Room::Door::FromRomBytes(position << 4, 0);
  return door;
}

zelda3::PotItem Item(int x = 64, int y = 64, uint8_t id = 0xFE) {
  return {static_cast<uint16_t>(((y / 16) << 8) | (x / 4)), id};
}

class DungeonSelectionEditTest : public ::testing::Test {
 protected:
  void SetUp() override {
    room_.SetTileObjects({Object(0xF99), Object(0x21, 12, 12)});
    room_.GetChests() = {{0xF1, false}};
    room_.GetDoors() = {Door()};
    room_.GetSprites().emplace_back(0x08, 4, 4, 3, 1);
    room_.GetSprites().back().set_key_drop(2);
    room_.GetSprites().back().set_deleted(true);
    room_.GetPotItems() = {Item()};
    room_.ClearSaveDirtyState();
  }
  DungeonSelectionEditRequest Request(DungeonSelectionEditKind kind) {
    DungeonSelectionEditRequest request;
    request.kind = kind;
    request.objects = {0};
    request.entities = {
        {EntityType::Door, 0}, {EntityType::Sprite, 0}, {EntityType::Item, 0}};
    return request;
  }
  zelda3::Room room_{7, nullptr};
};

TEST_F(DungeonSelectionEditTest, PlansAllDomainDeleteWithoutMutatingRoom) {
  const auto before = CaptureDungeonSelectionEditState(room_);
  const auto plan = PlanDungeonSelectionEdit(
      room_, Request(DungeonSelectionEditKind::kDelete));
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->room_id, 7);
  EXPECT_EQ(plan->domains, 15);
  ASSERT_EQ(plan->after.objects.size(), 1);
  EXPECT_EQ(plan->after.objects[0].id_, 0x21);
  EXPECT_TRUE(plan->after.chests.empty());
  EXPECT_TRUE(plan->after.doors.empty());
  EXPECT_TRUE(plan->after.sprites.empty());
  EXPECT_TRUE(plan->after.items.empty());
  EXPECT_TRUE(plan->after.selected_objects.empty());
  EXPECT_TRUE(plan->after.selected_entities.empty());
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                before, CaptureDungeonSelectionEditState(room_)),
            0);
  EXPECT_FALSE(room_.chests_dirty());
  EXPECT_FALSE(room_.object_stream_dirty());
}

TEST_F(DungeonSelectionEditTest,
       PublishAndRestoreAllDomainsPreservesExactData) {
  const auto plan = PlanDungeonSelectionEdit(
      room_, Request(DungeonSelectionEditKind::kDelete));
  ASSERT_TRUE(plan.ok()) << plan.status();
  ApplyDungeonSelectionEditState(room_, plan->after, plan->domains);
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                plan->after, CaptureDungeonSelectionEditState(room_)),
            0);
  EXPECT_TRUE(room_.chests_dirty());
  EXPECT_TRUE(room_.object_stream_dirty());
  EXPECT_TRUE(room_.sprites_dirty());
  EXPECT_TRUE(room_.pot_items_dirty());
  ApplyDungeonSelectionEditState(room_, plan->before, plan->domains);
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                plan->before, CaptureDungeonSelectionEditState(room_)),
            0);
  EXPECT_EQ(room_.GetSprites()[0].key_drop(), 2);
  EXPECT_TRUE(room_.GetSprites()[0].deleted());
  EXPECT_EQ(room_.GetChests()[0].id, 0xF1);
}

TEST_F(DungeonSelectionEditTest, DuplicateRetainsRewardsAndEntityProperties) {
  const auto plan = PlanDungeonSelectionEdit(
      room_, Request(DungeonSelectionEditKind::kDuplicate));
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->domains, 15);
  ASSERT_EQ(plan->after.chests.size(), 2);
  EXPECT_EQ(plan->after.chests[1].id, 0xF1);
  EXPECT_EQ(plan->after.sprites[1], plan->before.sprites[0]);
  EXPECT_EQ(plan->after.items[1].item, 0xFE);
  EXPECT_EQ(plan->after.selected_objects, (std::vector<size_t>{2}));
  EXPECT_EQ(plan->after.selected_entities,
            (std::vector<SelectedEntity>{{EntityType::Door, 1},
                                         {EntityType::Sprite, 1},
                                         {EntityType::Item, 1}}));
}

TEST_F(DungeonSelectionEditTest, CopyIsIndependentAndUsesMinimumPixelAnchor) {
  const auto request = Request(DungeonSelectionEditKind::kDuplicate);
  const auto copied =
      CopyDungeonSelection(room_, request.objects, request.entities);
  ASSERT_TRUE(copied.ok()) << copied.status();
  EXPECT_FALSE(copied->empty());
  EXPECT_EQ(copied->origin_pixel_x, 64);
  EXPECT_EQ(copied->origin_pixel_y, 32);  // North door anchor.
  room_.GetChests()[0].id = 1;
  room_.GetPotItems()[0].item = 2;
  EXPECT_EQ(copied->object_chests[0]->id, 0xF1);
  EXPECT_EQ(copied->items[0].item, 0xFE);
}

TEST_F(DungeonSelectionEditTest, PasteRebuildsChestOrderInDestinationLayers) {
  room_.SetTileObjects({Object(0xFB1, 8, 8, 2), Object(0xF99, 8, 8, 0)});
  room_.GetChests() = {{0x11, false}, {0xE7, true}};
  const auto copied = CopyDungeonSelection(room_, {0, 1}, {});
  ASSERT_TRUE(copied.ok()) << copied.status();
  zelda3::Room destination(9, nullptr);
  destination.SetTileObjects({Object(0xF99, 4, 4, 1)});
  destination.GetChests() = {{0x22, false}};
  DungeonSelectionEditRequest request;
  request.kind = DungeonSelectionEditKind::kPaste;
  request.clipboard = &*copied;
  request.delta_x_pixels = 16;
  const auto plan = PlanDungeonSelectionEdit(destination, request);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->after.chests.size(), 3);
  EXPECT_EQ(plan->after.chests[0].id, 0x11);
  EXPECT_EQ(plan->after.chests[1].id, 0x22);
  EXPECT_EQ(plan->after.chests[2].id, 0xE7);
  EXPECT_TRUE(plan->after.chests[2].size);
}

TEST_F(DungeonSelectionEditTest, MixedTranslationPreservesRelativeOffsets) {
  auto request = Request(DungeonSelectionEditKind::kMove);
  request.delta_x_pixels = 128;  // Exact next north-door slot.
  const auto plan = PlanDungeonSelectionEdit(room_, request);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->domains, 15);
  EXPECT_EQ(plan->after.objects[0].x_, 24);
  EXPECT_EQ(plan->after.doors[0].position, 1);
  EXPECT_EQ(plan->after.sprites[0].x, 12);
  EXPECT_EQ(plan->after.items[0].GetPixelX(), 192);
  EXPECT_EQ(plan->after.chests[0].id, 0xF1);
  EXPECT_EQ(plan->after.selected_entities, plan->before.selected_entities);
}

TEST_F(DungeonSelectionEditTest, RejectsDoorMoveWithoutExactAnchorAtomically) {
  const auto before = CaptureDungeonSelectionEditState(room_);
  auto request = Request(DungeonSelectionEditKind::kMove);
  request.delta_x_pixels = 16;
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                before, CaptureDungeonSelectionEditState(room_)),
            0);
}

TEST_F(DungeonSelectionEditTest, RejectsUnrepresentableMixedGridTranslation) {
  auto request = Request(DungeonSelectionEditKind::kMove);
  request.entities.erase(request.entities.begin());
  request.delta_x_pixels = 8;
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
  request.delta_x_pixels = 0;
  request.delta_y_pixels = 8;
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
}

TEST_F(DungeonSelectionEditTest, RejectsOutOfBoundsInsteadOfClampingGroup) {
  auto request = Request(DungeonSelectionEditKind::kMove);
  request.entities.erase(request.entities.begin());
  request.delta_x_pixels = -80;
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
  request.delta_x_pixels = INT_MAX;
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
}

TEST_F(DungeonSelectionEditTest, RejectsObjectEncodingAliasBeforePublish) {
  auto request = Request(DungeonSelectionEditKind::kMove);
  request.entities.clear();
  request.delta_x_pixels = (63 - 8) * 8;
  const auto plan = PlanDungeonSelectionEdit(room_, request);
  ASSERT_FALSE(plan.ok());
  EXPECT_NE(plan.status().message().find("discriminator"), std::string::npos);
}

TEST_F(DungeonSelectionEditTest, RejectsSpriteTerminatorEncoding) {
  room_.GetSprites() = {zelda3::Sprite(0x08, 4, 30, 24, 1)};
  auto request = Request(DungeonSelectionEditKind::kMove);
  request.objects.clear();
  request.entities = {{EntityType::Sprite, 0}};
  request.delta_y_pixels = 16;
  const auto plan = PlanDungeonSelectionEdit(room_, request);
  ASSERT_FALSE(plan.ok());
  EXPECT_NE(plan.status().message().find("end of the sprite list"),
            std::string::npos);
}

TEST_F(DungeonSelectionEditTest, RejectsSpriteHiddenKeyMarkerEncoding) {
  room_.GetSprites() = {zelda3::Sprite(0xE4, 1, 29, 24, 1)};
  auto request = Request(DungeonSelectionEditKind::kMove);
  request.objects.clear();
  request.entities = {{EntityType::Sprite, 0}};
  request.delta_x_pixels = -16;
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
}

TEST_F(DungeonSelectionEditTest,
       ZeroMoveRetainsUnknownValuesAndProducesNoHistoryData) {
  room_.GetDoors()[0].type = static_cast<zelda3::DoorType>(0xF1);
  const auto plan =
      PlanDungeonSelectionEdit(room_, Request(DungeonSelectionEditKind::kMove));
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_FALSE(plan->changed());
  EXPECT_EQ(plan->after.doors[0].type, room_.GetDoors()[0].type);
}

TEST_F(DungeonSelectionEditTest, UnchangedDoorCopyRetainsOriginalRawBytes) {
  room_.GetDoors()[0] = Door(3);
  room_.GetDoors()[0].byte1 |= 0x0C;
  auto request = Request(DungeonSelectionEditKind::kDuplicate);
  request.objects.clear();
  request.entities = {{EntityType::Door, 0}};
  const auto plan = PlanDungeonSelectionEdit(room_, request);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->after.doors[1].position, 3);
  EXPECT_EQ(plan->after.doors[1].byte1, 0x3C);
}

TEST_F(DungeonSelectionEditTest,
       DeduplicatesSelectionsAcrossObjectRepresentations) {
  auto request = Request(DungeonSelectionEditKind::kDuplicate);
  request.objects = {0, 0};
  request.entities.push_back({EntityType::Object, 0});
  request.entities.push_back({EntityType::Sprite, 0});
  const auto plan = PlanDungeonSelectionEdit(room_, request);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->after.objects.size(), 3);
  EXPECT_EQ(plan->after.sprites.size(), 2);
  EXPECT_EQ(plan->before.selected_objects.size(), 1);
  EXPECT_EQ(plan->before.selected_entities.size(), 3);
}

TEST_F(DungeonSelectionEditTest, RejectsStaleSelectionsBeforeDeletingAnything) {
  auto request = Request(DungeonSelectionEditKind::kDelete);
  request.entities.push_back({EntityType::Item, 100});
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
  request.entities.clear();
  request.objects.push_back(100);
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
  EXPECT_EQ(room_.GetTileObjects().size(), 2);
}

TEST_F(DungeonSelectionEditTest,
       RejectsMissingOrMismatchedChestClipboardMetadata) {
  auto copied = CopyDungeonSelection(room_, {0}, {});
  ASSERT_TRUE(copied.ok());
  DungeonSelectionEditRequest request;
  request.kind = DungeonSelectionEditKind::kPaste;
  request.clipboard = &*copied;
  copied->object_chests[0].reset();
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
  copied->object_chests[0] = chest_data{0xF1, true};
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
  copied->object_chests.clear();
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
}

TEST_F(DungeonSelectionEditTest, RefusesCopyWithBrokenChestMapping) {
  room_.GetChests().clear();
  EXPECT_FALSE(
      CopyDungeonSelection(room_, {0}, {{EntityType::Sprite, 0}}).ok());
  EXPECT_TRUE(CopyDungeonSelection(room_, {1}, {{EntityType::Sprite, 0}}).ok());
}

TEST_F(DungeonSelectionEditTest, UnrelatedEditsPreserveBrokenChestMapping) {
  room_.GetChests().clear();
  room_.GetTileObjects()[0].layer_ =
      static_cast<zelda3::RoomObject::LayerType>(3);
  auto request = Request(DungeonSelectionEditKind::kMove);
  request.objects.clear();
  request.entities = {{EntityType::Item, 0}};
  request.delta_x_pixels = 4;
  const auto plan = PlanDungeonSelectionEdit(room_, request);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->domains, kSelectionItems);
  EXPECT_TRUE(plan->after.chests.empty());
}

TEST_F(DungeonSelectionEditTest, EntireDuplicateRejectedAtSpriteCapacity) {
  room_.GetSprites().assign(zelda3::kMaxTotalSprites,
                            zelda3::Sprite(0x08, 4, 4, 0, 0));
  const auto before = CaptureDungeonSelectionEditState(room_);
  const auto plan = PlanDungeonSelectionEdit(
      room_, Request(DungeonSelectionEditKind::kDuplicate));
  ASSERT_FALSE(plan.ok());
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                before, CaptureDungeonSelectionEditState(room_)),
            0);
}

TEST_F(DungeonSelectionEditTest, EntireDuplicateRejectedAtDoorCapacity) {
  room_.GetDoors().assign(zelda3::kMaxDoors, Door());
  const auto plan = PlanDungeonSelectionEdit(
      room_, Request(DungeonSelectionEditKind::kDuplicate));
  ASSERT_FALSE(plan.ok());
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
}

TEST_F(DungeonSelectionEditTest, EntireDuplicateRejectedAtObjectCapacity) {
  room_.GetTileObjects().resize(zelda3::kMaxTileObjects, Object(0x21));
  const auto plan = PlanDungeonSelectionEdit(
      room_, Request(DungeonSelectionEditKind::kDuplicate));
  ASSERT_FALSE(plan.ok());
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
}

TEST_F(DungeonSelectionEditTest, DeleteCanRepairAnOverLimitLegacyRoom) {
  room_.GetSprites().assign(zelda3::kMaxTotalSprites + 2,
                            zelda3::Sprite(0x08, 4, 4, 0, 0));
  const auto plan = PlanDungeonSelectionEdit(
      room_, Request(DungeonSelectionEditKind::kDelete));
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->after.sprites.size(), zelda3::kMaxTotalSprites + 1);
}

TEST_F(DungeonSelectionEditTest, ChestEventCapacityRejectsWholeDuplicate) {
  room_.SetTileObjects(std::vector<zelda3::RoomObject>(6, Object(0xF99)));
  room_.GetChests().assign(6, {0x34, false});
  const auto plan = PlanDungeonSelectionEdit(
      room_, Request(DungeonSelectionEditKind::kDuplicate));
  ASSERT_FALSE(plan.ok());
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
}

TEST_F(DungeonSelectionEditTest, ChestAfterLockRejectsWholeDuplicate) {
  room_.GetTileObjects().push_back(Object(0xF98));
  const auto plan = PlanDungeonSelectionEdit(
      room_, Request(DungeonSelectionEditKind::kDuplicate));
  ASSERT_FALSE(plan.ok());
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(DungeonSelectionEditTest,
       DuplicatePreservesSpecialMetadataButReleasesBlockSlot) {
  auto block = Object(0x00);
  block.set_options(zelda3::ObjectOption::Block);
  block.set_block_load_order(12);
  block.set_block_behavior_layer(1);
  auto torch = Object(0x00);
  torch.set_options(zelda3::ObjectOption::Torch);
  torch.set_torch_reserved_bit(1);
  room_.SetTileObjects({block, torch});
  room_.GetChests().clear();
  auto request = Request(DungeonSelectionEditKind::kDuplicate);
  request.objects = {0, 1};
  request.entities.clear();
  const auto plan = PlanDungeonSelectionEdit(room_, request);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->after.objects[2].block_load_order(),
            zelda3::RoomObject::kBlockLoadOrderNew);
  EXPECT_EQ(plan->after.objects[2].block_behavior_layer(), 1);
  EXPECT_EQ(plan->after.objects[3].torch_reserved_bit(), 1);
  EXPECT_EQ(plan->before.objects[0].block_load_order(), 12);
}

TEST_F(DungeonSelectionEditTest, TorchMoveRespectsSerializerBounds) {
  auto torch = Object(0x00, 62, 62);
  torch.set_options(zelda3::ObjectOption::Torch);
  room_.SetTileObjects({torch});
  room_.GetChests().clear();
  auto request = Request(DungeonSelectionEditKind::kMove);
  request.entities.clear();
  request.delta_y_pixels = 8;
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
}

TEST_F(DungeonSelectionEditTest, ObjectMoveDoesNotDirtyUnchangedChestContents) {
  auto request = Request(DungeonSelectionEditKind::kMove);
  request.entities.clear();
  request.delta_x_pixels = 8;
  const auto plan = PlanDungeonSelectionEdit(room_, request);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ApplyDungeonSelectionEditState(room_, plan->after, plan->domains);
  EXPECT_TRUE(room_.object_stream_dirty());
  EXPECT_FALSE(room_.chests_dirty());
  EXPECT_FALSE(room_.sprites_dirty());
  EXPECT_FALSE(room_.pot_items_dirty());
}

TEST_F(DungeonSelectionEditTest,
       MaskedRestoreDoesNotOverwriteOtherDomainChanges) {
  const auto before = CaptureDungeonSelectionEditState(room_);
  room_.GetPotItems()[0].item = 0x11;
  room_.GetSprites()[0].set_key_drop(1);
  room_.GetDoors().clear();
  ApplyDungeonSelectionEditState(room_, before, kSelectionDoors);
  EXPECT_EQ(room_.GetDoors().size(), 1);
  EXPECT_EQ(room_.GetPotItems()[0].item, 0x11);
  EXPECT_EQ(room_.GetSprites()[0].key_drop(), 1);
  EXPECT_FALSE(room_.pot_items_dirty());
  EXPECT_FALSE(room_.sprites_dirty());
}

TEST_F(DungeonSelectionEditTest, ReapplyingIdenticalStateDoesNotDirtyAnything) {
  const auto state = CaptureDungeonSelectionEditState(room_);
  ApplyDungeonSelectionEditState(room_, state, 15);
  EXPECT_FALSE(room_.object_stream_dirty());
  EXPECT_FALSE(room_.chests_dirty());
  EXPECT_FALSE(room_.sprites_dirty());
  EXPECT_FALSE(room_.pot_items_dirty());
}

TEST_F(DungeonSelectionEditTest, RewardOnlyRestoreDoesNotDirtyObjectStream) {
  auto state = CaptureDungeonSelectionEditState(room_);
  state.chests[0].id = 0x22;
  ApplyDungeonSelectionEditState(room_, state, kSelectionObjects);
  EXPECT_EQ(room_.GetChests()[0].id, 0x22);
  EXPECT_TRUE(room_.chests_dirty());
  EXPECT_FALSE(room_.object_stream_dirty());
}

TEST_F(DungeonSelectionEditTest, EmptySelectionAndClipboardAreNoOps) {
  DungeonSelectionEditRequest request;
  auto plan = PlanDungeonSelectionEdit(room_, request);
  ASSERT_TRUE(plan.ok());
  EXPECT_FALSE(plan->changed());
  const auto clipboard = CopyDungeonSelection(room_, {}, {});
  ASSERT_TRUE(clipboard.ok());
  EXPECT_TRUE(clipboard->empty());
  EXPECT_EQ(clipboard->origin_pixel_x, 0);
  request.kind = DungeonSelectionEditKind::kPaste;
  request.clipboard = &*clipboard;
  plan = PlanDungeonSelectionEdit(room_, request);
  ASSERT_TRUE(plan.ok());
  EXPECT_FALSE(plan->changed());
}

TEST_F(DungeonSelectionEditTest,
       SelectionAndPreviewChangesAreNotAuthoredDifferences) {
  const auto before = CaptureDungeonSelectionEditState(room_);
  auto after = before;
  after.selected_objects = {0};
  after.selected_entities = {{EntityType::Sprite, 0}};
  after.objects[0].tiles_loaded_ = !after.objects[0].tiles_loaded_;
  EXPECT_EQ(ChangedDungeonSelectionDomains(before, after), 0);
}

TEST_F(DungeonSelectionEditTest, RejectsOverflowingClipboardSpritePosition) {
  DungeonSelectionClipboard clipboard;
  clipboard.sprites.push_back({8, INT_MAX, 4, 0, 0, 0, false});
  DungeonSelectionEditRequest request;
  request.kind = DungeonSelectionEditKind::kPaste;
  request.clipboard = &clipboard;
  EXPECT_FALSE(PlanDungeonSelectionEdit(room_, request).ok());
}

}  // namespace
}  // namespace yaze::editor
