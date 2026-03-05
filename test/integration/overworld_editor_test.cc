#include "integration/overworld_editor_test.h"

namespace yaze {
namespace test {

TEST_F(OverworldEditorTest, LoadAndSave) {
  // Verify initial state
  EXPECT_TRUE(overworld_editor_->IsRomLoaded());

  // Perform Save
  auto status = overworld_editor_->Save();
  EXPECT_TRUE(status.ok()) << "Save failed: " << status.message();
}

TEST_F(OverworldEditorTest, SwitchMaps) {
  // Test switching maps
  overworld_editor_->set_current_map(0);
  overworld_editor_->Update();  // Trigger sync
  EXPECT_EQ(overworld_editor_->overworld().current_map_id(), 0);

  overworld_editor_->set_current_map(1);
  overworld_editor_->Update();  // Trigger sync
  EXPECT_EQ(overworld_editor_->overworld().current_map_id(), 1);
}

TEST_F(OverworldEditorTest, CopyFailsWithoutSelection) {
  // Copy with no tile selected (tile16 = -1) should fail gracefully
  overworld_editor_->set_current_tile16(-1);
  auto status = overworld_editor_->Copy();
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(status));
}

TEST_F(OverworldEditorTest, PasteFailsWithEmptyClipboard) {
  // Paste with empty clipboard should fail gracefully
  ASSERT_TRUE(shared_clipboard_ != nullptr);
  EXPECT_FALSE(shared_clipboard_->has_overworld_tile16);
  auto status = overworld_editor_->Paste();
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(status));
}

TEST_F(OverworldEditorTest, ClipboardRoundTripSingleTile) {
  // Set a current tile to copy
  overworld_editor_->set_current_tile16(42);

  // Copy should succeed with a selected tile
  auto copy_status = overworld_editor_->Copy();
  ASSERT_TRUE(copy_status.ok()) << "Copy failed: " << copy_status.message();

  // Verify clipboard state
  EXPECT_TRUE(shared_clipboard_->has_overworld_tile16);
  ASSERT_EQ(shared_clipboard_->overworld_tile16_ids.size(), 1);
  EXPECT_EQ(shared_clipboard_->overworld_tile16_ids[0], 42);
  EXPECT_EQ(shared_clipboard_->overworld_width, 1);
  EXPECT_EQ(shared_clipboard_->overworld_height, 1);
}

TEST_F(OverworldEditorTest,
       ItemSelectionWorkflowDuplicateNudgeDeleteMaintainsContinuity) {
  auto* items = overworld_editor_->overworld().mutable_all_items();
  ASSERT_NE(items, nullptr);

  items->clear();
  items->emplace_back(/*id=*/0x40, /*room_map_id=*/0x00, /*x=*/96, /*y=*/112,
                      /*bg2=*/false);
  items->emplace_back(/*id=*/0x41, /*room_map_id=*/0x00, /*x=*/400, /*y=*/400,
                      /*bg2=*/false);

  const zelda3::OverworldItem source_identity = items->at(0);
  ASSERT_TRUE(overworld_editor_->SelectItemByIdentity(source_identity));
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);

  ASSERT_TRUE(overworld_editor_->DuplicateSelectedItem(/*offset_x=*/16,
                                                       /*offset_y=*/0));
  ASSERT_EQ(items->size(), 3u);

  auto* selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 112);
  EXPECT_EQ(selected->y_, 112);

  ASSERT_TRUE(overworld_editor_->NudgeSelectedItem(/*delta_x=*/1,
                                                   /*delta_y=*/-1));
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 113);
  EXPECT_EQ(selected->y_, 111);
  EXPECT_EQ(selected->game_x_, 7);
  EXPECT_EQ(selected->game_y_, 6);

  ASSERT_TRUE(overworld_editor_->DeleteSelectedItem());
  ASSERT_EQ(items->size(), 2u);
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->id_, 0x40);
  EXPECT_EQ(selected->x_, 96);
  EXPECT_EQ(selected->y_, 112);
}

TEST_F(OverworldEditorTest, ItemMultiDeleteUndoRedoRestoresListAndSelection) {
  auto* items = overworld_editor_->overworld().mutable_all_items();
  ASSERT_NE(items, nullptr);

  items->clear();
  items->emplace_back(/*id=*/0x40, /*room_map_id=*/0x00, /*x=*/96, /*y=*/112,
                      /*bg2=*/false);
  items->emplace_back(/*id=*/0x41, /*room_map_id=*/0x00, /*x=*/120, /*y=*/112,
                      /*bg2=*/false);
  items->emplace_back(/*id=*/0x42, /*room_map_id=*/0x00, /*x=*/200, /*y=*/112,
                      /*bg2=*/false);

  ASSERT_TRUE(overworld_editor_->SelectItemByIdentity(items->at(1)));
  ASSERT_TRUE(overworld_editor_->DeleteSelectedItem());
  ASSERT_EQ(items->size(), 2u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x40);

  ASSERT_TRUE(overworld_editor_->DeleteSelectedItem());
  ASSERT_EQ(items->size(), 1u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x42);

  ASSERT_TRUE(overworld_editor_->Undo().ok());
  ASSERT_EQ(items->size(), 2u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x40);

  ASSERT_TRUE(overworld_editor_->Undo().ok());
  ASSERT_EQ(items->size(), 3u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x41);

  ASSERT_TRUE(overworld_editor_->Redo().ok());
  ASSERT_EQ(items->size(), 2u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x40);

  ASSERT_TRUE(overworld_editor_->Redo().ok());
  ASSERT_EQ(items->size(), 1u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x42);
}

TEST_F(OverworldEditorTest, ItemNudgeSequenceUndoRedoRestoresCoordinates) {
  auto* items = overworld_editor_->overworld().mutable_all_items();
  ASSERT_NE(items, nullptr);

  items->clear();
  items->emplace_back(/*id=*/0x55, /*room_map_id=*/0x00, /*x=*/160, /*y=*/176,
                      /*bg2=*/false);

  ASSERT_TRUE(overworld_editor_->SelectItemByIdentity(items->at(0)));
  ASSERT_TRUE(overworld_editor_->NudgeSelectedItem(/*delta_x=*/16,
                                                   /*delta_y=*/0));
  ASSERT_TRUE(overworld_editor_->NudgeSelectedItem(/*delta_x=*/0,
                                                   /*delta_y=*/-16));

  auto* selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 176);
  EXPECT_EQ(selected->y_, 160);
  EXPECT_EQ(selected->game_x_, 11);
  EXPECT_EQ(selected->game_y_, 10);

  ASSERT_TRUE(overworld_editor_->Undo().ok());
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 176);
  EXPECT_EQ(selected->y_, 176);
  EXPECT_EQ(selected->game_x_, 11);
  EXPECT_EQ(selected->game_y_, 11);

  ASSERT_TRUE(overworld_editor_->Undo().ok());
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 160);
  EXPECT_EQ(selected->y_, 176);
  EXPECT_EQ(selected->game_x_, 10);
  EXPECT_EQ(selected->game_y_, 11);

  ASSERT_TRUE(overworld_editor_->Redo().ok());
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 176);
  EXPECT_EQ(selected->y_, 176);

  ASSERT_TRUE(overworld_editor_->Redo().ok());
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 176);
  EXPECT_EQ(selected->y_, 160);
}

TEST_F(OverworldEditorTest, StaleItemSelectionClearsWhenBackingItemIsRemoved) {
  auto* items = overworld_editor_->overworld().mutable_all_items();
  ASSERT_NE(items, nullptr);

  items->clear();
  items->emplace_back(/*id=*/0x51, /*room_map_id=*/0x00, /*x=*/160, /*y=*/176,
                      /*bg2=*/false);

  const zelda3::OverworldItem selected_identity = items->at(0);
  ASSERT_TRUE(overworld_editor_->SelectItemByIdentity(selected_identity));
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);

  items->clear();
  EXPECT_EQ(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_FALSE(overworld_editor_->selected_item_identity().has_value());
  EXPECT_FALSE(overworld_editor_->DeleteSelectedItem());
}

}  // namespace test
}  // namespace yaze
