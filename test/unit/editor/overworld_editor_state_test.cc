#include "app/editor/overworld/overworld_editor.h"

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

TEST(OverworldEditorStateTest,
     NormalizeMapSelectionLeavesValidSelectionUntouched) {
  int current_world = 1;
  int current_map = 0x40;

  EXPECT_FALSE(
      OverworldEditor::NormalizeMapSelection(current_world, current_map));
  EXPECT_EQ(current_world, 1);
  EXPECT_EQ(current_map, 0x40);
}

TEST(OverworldEditorStateTest,
     NormalizeMapSelectionClampsNegativeMapToWorldStart) {
  int current_world = 1;
  int current_map = -7;

  EXPECT_TRUE(
      OverworldEditor::NormalizeMapSelection(current_world, current_map));
  EXPECT_EQ(current_world, 1);
  EXPECT_EQ(current_map, 0x40);
}

TEST(OverworldEditorStateTest,
     NormalizeMapSelectionClampsOversizedMapIntoValidWorld) {
  int current_world = 4;
  int current_map = 0x103;

  EXPECT_TRUE(
      OverworldEditor::NormalizeMapSelection(current_world, current_map));
  EXPECT_EQ(current_world, 2);
  EXPECT_EQ(current_map, 0x80);
}

TEST(OverworldEditorStateTest, NormalizeMapSelectionRealignsWorldToValidMap) {
  int current_world = 0;
  int current_map = 0x81;

  EXPECT_TRUE(
      OverworldEditor::NormalizeMapSelection(current_world, current_map));
  EXPECT_EQ(current_world, 2);
  EXPECT_EQ(current_map, 0x81);
}

}  // namespace
}  // namespace yaze::editor
