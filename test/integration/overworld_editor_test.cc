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
  overworld_editor_->Update(); // Trigger sync
  EXPECT_EQ(overworld_editor_->overworld().current_map_id(), 0);

  overworld_editor_->set_current_map(1);
  overworld_editor_->Update(); // Trigger sync
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

}  // namespace test
}  // namespace yaze
