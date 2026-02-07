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

}  // namespace test
}  // namespace yaze
