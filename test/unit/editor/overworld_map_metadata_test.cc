#include "app/editor/overworld/overworld_map_metadata.h"

#include <gtest/gtest.h>

#include "core/project.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {
namespace {

// RenameProjectResourceLabel calls YazeProject::InitializeResourceLabelProvider,
// which stores &project.resource_labels in the global ResourceLabelProvider.
// The stack-local YazeProject in these tests dies at end-of-scope, leaving a
// dangling pointer that crashes any later test that calls GetLabel() (e.g. via
// Sprite construction). TearDown clears the singleton even on assertion
// failure.
class OverworldMapMetadataTest : public ::testing::Test {
 protected:
  void TearDown() override {
    auto& provider = zelda3::GetResourceLabels();
    provider.SetProjectLabels(nullptr);
    provider.SetHackManifest(nullptr);
  }
};

TEST_F(OverworldMapMetadataTest, ProjectMapLabelOverridesDefaultName) {
  project::YazeProject project;
  project.resource_labels["overworld_map"]["2"] = "Custom Meadow";

  const zelda3::Overworld overworld(nullptr);
  const auto metadata =
      BuildOverworldMapMetadata(overworld, nullptr, &project, 2, 0);

  EXPECT_EQ(metadata.map_name, "Custom Meadow");
  EXPECT_NE(metadata.map_title.find("Custom Meadow"), std::string::npos);
}

TEST_F(OverworldMapMetadataTest,
       RenameResourceLabelNormalizesAndClearsAliases) {
  project::YazeProject project;
  project.resource_labels["overworld_map"]["0x02"] = "Hex Meadow";
  project.resource_labels["overworld_map"]["2"] = "Old Meadow";

  auto status =
      RenameProjectResourceLabel(&project, "overworld_map", 2, " New Meadow ");
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(GetProjectResourceLabel(&project, "overworld_map", 2),
            "New Meadow");
  EXPECT_FALSE(project.resource_labels["overworld_map"].contains("0x02"));

  status = RenameProjectResourceLabel(&project, "overworld_map", 2, "");
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(GetProjectResourceLabel(&project, "overworld_map", 2), "");
}

}  // namespace
}  // namespace yaze::editor
