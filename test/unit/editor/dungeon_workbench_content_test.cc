#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "app/editor/dungeon/dungeon_project_labels.h"
#include "core/project.h"

namespace yaze::editor {
namespace {

constexpr float kMinCanvasWidth = 420.0f;
constexpr float kMinSidebarWidth = 320.0f;
constexpr float kSplitterWidth = 8.0f;
constexpr float kCompactLeftWidth = 230.4f;
constexpr float kCompactRightWidth = 272.0f;

std::filesystem::path MakeTempProjectRoot() {
  return std::filesystem::temp_directory_path() /
         ("yaze_dungeon_project_labels_" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()));
}

TEST(DungeonWorkbenchContentLayoutTest,
     PrefersCompactingAndHidingLeftPaneBeforeRightPane) {
  const auto full = ResolveDungeonWorkbenchResponsiveLayout(
      1076.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(full.show_left);
  EXPECT_TRUE(full.show_right);
  EXPECT_FALSE(full.compact_left);
  EXPECT_FALSE(full.compact_right);

  const auto compact_left = ResolveDungeonWorkbenchResponsiveLayout(
      1075.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(compact_left.show_left);
  EXPECT_TRUE(compact_left.show_right);
  EXPECT_TRUE(compact_left.compact_left);
  EXPECT_FALSE(compact_left.compact_right);

  const auto both_compact = ResolveDungeonWorkbenchResponsiveLayout(
      979.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(both_compact.show_left);
  EXPECT_TRUE(both_compact.show_right);
  EXPECT_TRUE(both_compact.compact_left);
  EXPECT_TRUE(both_compact.compact_right);

  const auto hide_left = ResolveDungeonWorkbenchResponsiveLayout(
      931.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_FALSE(hide_left.show_left);
  EXPECT_TRUE(hide_left.show_right);
  EXPECT_FALSE(hide_left.compact_left);
  EXPECT_TRUE(hide_left.compact_right);

  const auto hide_both = ResolveDungeonWorkbenchResponsiveLayout(
      699.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_FALSE(hide_both.show_left);
  EXPECT_FALSE(hide_both.show_right);
}

TEST(DungeonWorkbenchContentLayoutTest,
     PaneLayoutUsesSharedCompactWidthsAtResponsiveThresholds) {
  const auto compact_both = ResolveDungeonWorkbenchPaneLayout(
      938.4f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 280.0f, 320.0f,
      true, true);
  EXPECT_TRUE(compact_both.responsive.show_left);
  EXPECT_TRUE(compact_both.responsive.show_right);
  EXPECT_TRUE(compact_both.responsive.compact_left);
  EXPECT_TRUE(compact_both.responsive.compact_right);
  EXPECT_NEAR(compact_both.left_width, kCompactLeftWidth, 0.001f);
  EXPECT_NEAR(compact_both.right_width, kCompactRightWidth, 0.001f);
  EXPECT_NEAR(compact_both.center_width, 420.0f, 0.001f);

  const auto hide_left = ResolveDungeonWorkbenchPaneLayout(
      931.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 280.0f, 320.0f,
      true, true);
  EXPECT_FALSE(hide_left.responsive.show_left);
  EXPECT_TRUE(hide_left.responsive.show_right);
  EXPECT_NEAR(hide_left.left_width, 0.0f, 0.001f);
  EXPECT_NEAR(hide_left.right_width, kCompactRightWidth, 0.001f);
  EXPECT_NEAR(hide_left.center_width,
              931.0f - kCompactRightWidth - kSplitterWidth, 0.001f);
}

TEST(DungeonWorkbenchContentLayoutTest,
     OversizedPaneMemoryStillProtectsMinimumCanvasWidth) {
  const auto layout = ResolveDungeonWorkbenchPaneLayout(
      1000.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 500.0f,
      360.0f, true, true);
  EXPECT_TRUE(layout.responsive.show_left);
  EXPECT_TRUE(layout.responsive.show_right);
  EXPECT_TRUE(layout.responsive.compact_left);
  EXPECT_FALSE(layout.responsive.compact_right);
  EXPECT_NEAR(layout.left_width, kCompactLeftWidth, 0.001f);
  EXPECT_NEAR(layout.right_width, 333.6f, 0.001f);
  EXPECT_GE(layout.center_width, kMinCanvasWidth);
}

TEST(DungeonWorkbenchProjectLabelsTest,
     UsesOracleRegistryDungeonAndRoomNamesWhenProjectIsOpen) {
  const std::filesystem::path root = MakeTempProjectRoot();
  const std::filesystem::path planning = root / "Docs" / "Dev" / "Planning";
  ASSERT_TRUE(std::filesystem::create_directories(planning));
  {
    std::ofstream out(planning / "dungeons.json");
    out << R"json({
      "dungeons": [
        {
          "id": "D4",
          "name": "Zora Temple",
          "vanilla_name": "Thieves' Town",
          "rooms": [
            {
              "id": "0x25",
              "name": "Water Grate",
              "grid_row": 1,
              "grid_col": 2,
              "type": "connector",
              "palette": 1,
              "blockset": 6,
              "spriteset": 3,
              "tag1": 0,
              "tag2": 0
            }
          ]
        }
      ]
    })json";
  }

  project::YazeProject project;
  project.name = "Oracle of Secrets";
  project.filepath = (root / "Oracle-of-Secrets.yaze").string();
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json({
                    "manifest_version": 1,
                    "hack_name": "Oracle of Secrets"
                  })json")
                  .ok());
  ASSERT_TRUE(project.hack_manifest.LoadProjectRegistry(root.string()).ok());

  EXPECT_EQ(dungeon_project_labels::GetDungeonNameForRoom(&project, 0x25),
            "D4 Zora Temple");
  EXPECT_EQ(dungeon_project_labels::GetRoomLabel(&project, 0x25),
            "Water Grate");

  zelda3::ResourceLabelProvider::ProjectLabels stale_labels;
  stale_labels["room"]["37"] = "Thieves' Town";
  zelda3::GetResourceLabels().SetProjectLabels(&stale_labels);
  zelda3::GetResourceLabels().SetHackManifest(&project.hack_manifest);
  EXPECT_EQ(dungeon_project_labels::GetRoomLabel(&project, 0x25),
            "Water Grate");
  EXPECT_EQ(zelda3::GetRoomLabel(0x25), "Water Grate");
  zelda3::GetResourceLabels().SetHackManifest(nullptr);
  zelda3::GetResourceLabels().SetProjectLabels(nullptr);

  std::filesystem::remove_all(root);
}

}  // namespace
}  // namespace yaze::editor
