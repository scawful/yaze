#include "gtest/gtest.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/room.h"

namespace yaze::zelda3 {
namespace {

class DungeonObjectEditorDirtyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x100000, 0)).ok());
    room_ = std::make_unique<Room>(0, &rom_);
    ASSERT_TRUE(room_->AddObject(RoomObject(0x21, 1, 1, 0, 0)).ok());
    ASSERT_TRUE(room_->AddObject(RoomObject(0x22, 5, 5, 0, 0)).ok());
    room_->ClearSaveDirtyState();

    editor_ = std::make_unique<DungeonObjectEditor>(&rom_);
    editor_->SetExternalRoom(room_.get());
    auto config = editor_->GetConfig();
    config.snap_to_grid = false;
    config.validate_objects = false;
    editor_->SetConfig(config);
  }

  Rom rom_;
  std::unique_ptr<Room> room_;
  std::unique_ptr<DungeonObjectEditor> editor_;
};

TEST_F(DungeonObjectEditorDirtyTest, DirectMutatorsMarkObjectStreamDirty) {
  ASSERT_TRUE(editor_->MoveObject(0, 2, 3).ok());
  EXPECT_TRUE(room_->object_stream_dirty());

  room_->ClearSaveDirtyState();
  ASSERT_TRUE(editor_->ResizeObject(0, 4).ok());
  EXPECT_TRUE(room_->object_stream_dirty());

  room_->ClearSaveDirtyState();
  ASSERT_TRUE(editor_->ChangeObjectType(0, 0x23).ok());
  EXPECT_TRUE(room_->object_stream_dirty());

  room_->ClearSaveDirtyState();
  ASSERT_TRUE(editor_->ChangeObjectLayer(0, 1).ok());
  EXPECT_TRUE(room_->object_stream_dirty());
}

TEST_F(DungeonObjectEditorDirtyTest, BatchMutatorsMarkObjectStreamDirty) {
  const std::vector<size_t> indices = {0, 1};

  ASSERT_TRUE(editor_->BatchMoveObjects(indices, 1, 2).ok());
  EXPECT_TRUE(room_->object_stream_dirty());

  room_->ClearSaveDirtyState();
  ASSERT_TRUE(editor_->BatchResizeObjects(indices, 5).ok());
  EXPECT_TRUE(room_->object_stream_dirty());

  room_->ClearSaveDirtyState();
  ASSERT_TRUE(editor_->BatchChangeObjectLayer(indices, 2).ok());
  EXPECT_TRUE(room_->object_stream_dirty());
}

TEST_F(DungeonObjectEditorDirtyTest, AlignAndDragMarkObjectStreamDirty) {
  ASSERT_TRUE(editor_->AddToSelection(0).ok());
  ASSERT_TRUE(editor_->AddToSelection(1).ok());
  ASSERT_TRUE(
      editor_->AlignSelectedObjects(DungeonObjectEditor::Alignment::Left).ok());
  EXPECT_TRUE(room_->object_stream_dirty());

  room_->ClearSaveDirtyState();
  ASSERT_TRUE(editor_->ClearSelection().ok());
  ASSERT_TRUE(editor_->AddToSelection(0).ok());
  ASSERT_TRUE(editor_
                  ->HandleMouseDrag(/*start_x=*/0, /*start_y=*/0,
                                    /*current_x=*/16, /*current_y=*/0)
                  .ok());
  EXPECT_TRUE(room_->object_stream_dirty());
}

TEST_F(DungeonObjectEditorDirtyTest, NoOpMutatorsDoNotMarkDirty) {
  const auto& object = room_->GetTileObject(0);
  ASSERT_TRUE(editor_->MoveObject(0, object.x(), object.y()).ok());
  ASSERT_TRUE(editor_->ResizeObject(0, object.size()).ok());
  ASSERT_TRUE(editor_->ChangeObjectType(0, object.id_).ok());
  ASSERT_TRUE(editor_->ChangeObjectLayer(0, object.GetLayerValue()).ok());
  ASSERT_TRUE(editor_->BatchMoveObjects({0}, 0, 0).ok());
  ASSERT_TRUE(editor_->BatchResizeObjects({0}, object.size()).ok());
  ASSERT_TRUE(
      editor_->BatchChangeObjectLayer({0}, object.GetLayerValue()).ok());
  EXPECT_FALSE(room_->object_stream_dirty());

  ASSERT_TRUE(editor_->BatchMoveObjects({99}, 1, 1).ok());
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_F(DungeonObjectEditorDirtyTest,
       SpecialTableObjectMutationMarksItsOwnSaveDomain) {
  Room special_room(1, &rom_);
  RoomObject torch(0x150, 1, 1, 0, 0);
  torch.set_options(ObjectOption::Torch);
  ASSERT_TRUE(special_room.AddObject(torch).ok());
  special_room.ClearSaveDirtyState();
  editor_->SetExternalRoom(&special_room);

  ASSERT_TRUE(editor_->MoveObject(0, 2, 2).ok());

  EXPECT_TRUE(special_room.torches_dirty());
  EXPECT_FALSE(special_room.object_stream_dirty());
}

TEST_F(DungeonObjectEditorDirtyTest,
       DuplicateAndPasteResetLoadedBlockSlotWhileMovePreservesIt) {
  Room special_room(1, &rom_);
  RoomObject block(0x0E00, 10, 10, 0, 1);
  block.set_options(ObjectOption::Block);
  block.set_block_behavior_layer(1);
  block.set_block_load_order(7);
  ASSERT_TRUE(special_room.AddObject(block).ok());
  special_room.ClearSaveDirtyState();
  editor_->SetExternalRoom(&special_room);

  ASSERT_TRUE(editor_->MoveObject(0, 11, 10).ok());
  EXPECT_EQ(special_room.GetTileObjects()[0].block_load_order(), 7);

  const auto duplicate_index = editor_->DuplicateObject(0, 1, 0);
  ASSERT_TRUE(duplicate_index.has_value());
  EXPECT_EQ(special_room.GetTileObjects()[*duplicate_index].block_load_order(),
            RoomObject::kBlockLoadOrderNew);
  EXPECT_EQ(
      special_room.GetTileObjects()[*duplicate_index].block_behavior_layer(),
      1);

  editor_->CopySelectedObjects({0});
  const auto pasted_indices = editor_->PasteObjects();
  ASSERT_EQ(pasted_indices.size(), 1u);
  EXPECT_EQ(special_room.GetTileObjects()[pasted_indices[0]].block_load_order(),
            RoomObject::kBlockLoadOrderNew);
  EXPECT_EQ(
      special_room.GetTileObjects()[pasted_indices[0]].block_behavior_layer(),
      1);
}

}  // namespace
}  // namespace yaze::zelda3
