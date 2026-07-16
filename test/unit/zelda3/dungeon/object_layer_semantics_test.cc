#include "gtest/gtest.h"

#include <vector>

#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {

TEST(ObjectLayerSemanticsTest, CeilingRoutineIsSingleLayer) {
  RoomObject obj(/*id=*/0x00, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);

  const auto sem = GetObjectLayerSemantics(obj);
  EXPECT_FALSE(sem.draws_to_both_bgs);
  EXPECT_EQ(sem.effective_bg_layer, EffectiveBgLayer::kBg1);
}

TEST(ObjectLayerSemanticsTest, RoutineMetadataCanForceBothBgForType2Objects) {
  // Type 2 corners (0x108-0x10F) use RoomDraw_4x4Corner_BothBG in usdasm.
  // Our RoomObject doesn't set all_bgs_ for these IDs, so this must come from
  // DrawRoutineRegistry metadata.
  RoomObject obj(/*id=*/0x108, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);

  EXPECT_FALSE(obj.all_bgs_);
  const auto sem = GetObjectLayerSemantics(obj);
  EXPECT_TRUE(sem.draws_to_both_bgs);
  EXPECT_EQ(sem.effective_bg_layer, EffectiveBgLayer::kBothBg1Bg2);
}

TEST(ObjectLayerSemanticsTest, AllBgsOverrideForcesBothBg) {
  RoomObject obj(/*id=*/0x0C, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/1);

  EXPECT_TRUE(obj.all_bgs_);
  const auto sem = GetObjectLayerSemantics(obj);
  EXPECT_TRUE(sem.draws_to_both_bgs);
  EXPECT_EQ(sem.effective_bg_layer, EffectiveBgLayer::kBothBg1Bg2);
}

TEST(ObjectLayerSemanticsTest, NonBothBgUsesStoredLayer) {
  RoomObject obj(/*id=*/0x21, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/1);

  const auto sem = GetObjectLayerSemantics(obj);
  EXPECT_FALSE(sem.draws_to_both_bgs);
  EXPECT_EQ(sem.effective_bg_layer, EffectiveBgLayer::kBg2);
}

TEST(DungeonObjectLayerGuardrailsTest,
     SingleAndBatchStreamChangesIncludeBothBgObjects) {
  Rom rom;
  Room room(/*room_id=*/0, &rom);

  ASSERT_TRUE(room.AddObject(RoomObject(0x03, 0, 0, 0, 0)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x21, 0, 0, 0, 0)).ok());

  DungeonObjectEditor editor(&rom);
  editor.SetExternalRoom(&room);

  std::vector<size_t> indices = {0, 1};
  ASSERT_TRUE(editor.BatchChangeObjectLayer(indices, /*new_layer=*/1).ok());

  // BothBG controls buffer fan-out, not the object's ROM stream or draw order.
  EXPECT_EQ(room.GetTileObject(0).layer_, RoomObject::LayerType::BG2);
  EXPECT_EQ(room.GetTileObject(1).layer_, RoomObject::LayerType::BG2);

  ASSERT_TRUE(editor.ChangeObjectLayer(0, /*new_layer=*/0).ok());
  EXPECT_EQ(room.GetTileObject(0).layer_, RoomObject::LayerType::BG1);
  ASSERT_TRUE(editor.ChangeObjectLayer(0, /*new_layer=*/1).ok());
  ASSERT_EQ(room.GetTileObjectCount(), 2);
  EXPECT_EQ(room.GetTileObject(0).id_, 0x21);
  EXPECT_EQ(room.GetTileObject(1).id_, 0x03);
  EXPECT_EQ(room.GetTileObject(1).layer_, RoomObject::LayerType::BG2);

  const std::vector<uint8_t> expected = {
      0xFF, 0xFF,             // Empty primary stream.
      0x00, 0x00, 0x21,       // Ordinary object in BG2 overlay stream.
      0x00, 0x00, 0x03,       // BothBG object appended to that stream.
      0xFF, 0xFF,             // End BG2 overlay stream.
      0xF0, 0xFF, 0xFF, 0xFF  // Empty BG1 overlay and door list.
  };
  EXPECT_EQ(room.EncodeObjects(), expected);
}

TEST(DungeonObjectLayerGuardrailsTest,
     SingleStreamChangeAppendsAfterExistingTargetAndRemapsSelection) {
  Rom rom;
  Room room(/*room_id=*/0, &rom);
  ASSERT_TRUE(room.AddObject(RoomObject(0x03, 0, 0, 0, 0)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x21, 0, 0, 0, 0)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x22, 0, 0, 0, 1)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x23, 0, 0, 0, 1)).ok());

  DungeonObjectEditor editor(&rom);
  editor.SetExternalRoom(&room);
  ASSERT_TRUE(editor.AddToSelection(0).ok());

  size_t callback_index = 0;
  editor.SetObjectChangedCallback(
      [&](size_t index, const RoomObject&) { callback_index = index; });
  ASSERT_TRUE(editor.ChangeObjectLayer(0, /*new_layer=*/1).ok());

  ASSERT_EQ(room.GetTileObjectCount(), 4);
  EXPECT_EQ(room.GetTileObject(0).id_, 0x21);
  EXPECT_EQ(room.GetTileObject(1).id_, 0x22);
  EXPECT_EQ(room.GetTileObject(2).id_, 0x23);
  EXPECT_EQ(room.GetTileObject(3).id_, 0x03);
  ASSERT_EQ(editor.GetSelection().selected_objects.size(), 1);
  EXPECT_EQ(editor.GetSelection().selected_objects[0], 3);
  EXPECT_EQ(callback_index, 3);

  const std::vector<uint8_t> expected = {
      0x00, 0x00, 0x21, 0xFF, 0xFF, 0x00, 0x00, 0x22, 0x00, 0x00,
      0x23, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF,
  };
  EXPECT_EQ(room.EncodeObjects(), expected);
}

TEST(DungeonObjectLayerGuardrailsTest,
     SpecialOnlyLayerChangePreservesRoomStreamOrder) {
  Rom rom;
  Room room(/*room_id=*/0, &rom);
  ASSERT_TRUE(room.AddObject(RoomObject(0x22, 0, 0, 0, 1)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x21, 0, 0, 0, 0)).ok());
  RoomObject torch(0x150, 1, 1, 0, 0);
  torch.set_options(ObjectOption::Torch);
  ASSERT_TRUE(room.AddObject(torch).ok());

  DungeonObjectEditor editor(&rom);
  editor.SetExternalRoom(&room);
  ASSERT_TRUE(editor.ChangeObjectLayer(2, /*new_layer=*/1).ok());

  ASSERT_EQ(room.GetTileObjectCount(), 3);
  EXPECT_EQ(room.GetTileObject(0).id_, 0x22);
  EXPECT_EQ(room.GetTileObject(1).id_, 0x21);
  EXPECT_EQ(room.GetTileObject(2).id_, 0x150);
  EXPECT_EQ(room.GetTileObject(2).GetLayerValue(), 1);
}

TEST(DungeonObjectLayerGuardrailsTest,
     MixedBatchRejectsUnsupportedSpecialTargetAtomically) {
  Rom rom;
  Room room(/*room_id=*/0, &rom);
  RoomObject torch(0x150, 1, 1, 0, 0);
  torch.set_options(ObjectOption::Torch);
  ASSERT_TRUE(room.AddObject(torch).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x21, 0, 0, 0, 0)).ok());
  room.ClearSaveDirtyState();

  DungeonObjectEditor editor(&rom);
  editor.SetExternalRoom(&room);
  const auto status = editor.BatchChangeObjectLayer({0, 1}, /*new_layer=*/2);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(room.GetTileObject(0).GetLayerValue(), 0);
  EXPECT_EQ(room.GetTileObject(1).GetLayerValue(), 0);
  EXPECT_FALSE(room.torches_dirty());
  EXPECT_FALSE(room.object_stream_dirty());
}

}  // namespace zelda3
}  // namespace yaze
