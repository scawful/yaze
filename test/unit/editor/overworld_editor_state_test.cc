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

TEST(OverworldEditorStateTest,
     NormalizeMapSelectionClampsSpecialTailToLoadedSpecialMap) {
  int current_world = 2;
  int current_map = 0xBF;

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

TEST(OverworldEditorStateTest,
     NormalizeMapSelectionKeepsProjectTerrainInSpecialWorld) {
  for (int map_id : {0xA0, 0xBF}) {
    int current_world = 0;
    int current_map = map_id;
    EXPECT_TRUE(OverworldEditor::NormalizeMapSelection(current_world,
                                                       current_map, 192));
    EXPECT_EQ(current_world, 2);
    EXPECT_EQ(current_map, map_id);
    EXPECT_FALSE(OverworldEditor::NormalizeMapSelection(current_world,
                                                        current_map, 192));
  }
}

TEST(OverworldEditorStateTest, ProjectPathAloneDoesNotExposeTerrainMaps) {
  project::YazeProject project;
  project.lyra_terrain_file = "missing.lyra-terrain.json";
  EditorDependencies deps;
  deps.project = &project;
  OverworldEditor editor(nullptr, deps);
  EXPECT_EQ(editor.DisplayedMapCount(), 160);
  EXPECT_FALSE(editor.IsProjectTerrainMap(0xA0));
  EXPECT_FALSE(editor.IsProjectTerrainMap(0xBF));
}

TEST(OverworldEditorStateTest, NullRomSaveReturnsFailedPrecondition) {
  OverworldEditor editor(nullptr);

  const auto status = editor.Save();

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
}

TEST(OverworldEditorStateTest, UnloadedContextSnapshotIsStillInformative) {
  OverworldEditor editor(nullptr);
  const EditorContextSnapshot snapshot = editor.BuildContextSnapshot();
  EXPECT_EQ(snapshot.category, "Overworld");
  EXPECT_EQ(snapshot.title, "Overworld");
  EXPECT_NE(snapshot.subtitle.find("Load a ROM"), std::string::npos);
  EXPECT_TRUE(snapshot.actions.empty());
}

}  // namespace
}  // namespace yaze::editor
