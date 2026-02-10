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

TEST(DungeonObjectLayerGuardrailsTest, BatchLayerChangeSkipsBothBgObjects) {
  Rom rom;
  Room room(/*room_id=*/0, &rom);

  ASSERT_TRUE(room.AddObject(RoomObject(0x03, 0, 0, 0, 0)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x21, 0, 0, 0, 0)).ok());

  DungeonObjectEditor editor(&rom);
  editor.SetExternalRoom(&room);

  std::vector<size_t> indices = {0, 1};
  ASSERT_TRUE(editor.BatchChangeObjectLayer(indices, /*new_layer=*/1).ok());

  // Object 0x03 is marked AllBGs (draws to both BGs). It should be skipped.
  EXPECT_EQ(room.GetTileObject(0).layer_, RoomObject::LayerType::BG1);
  // Normal objects should still be updated.
  EXPECT_EQ(room.GetTileObject(1).layer_, RoomObject::LayerType::BG2);
}

}  // namespace zelda3
}  // namespace yaze
