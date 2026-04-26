#include "app/editor/overworld/overworld_map_metadata.h"

#include <gtest/gtest.h>

#include "core/project.h"

namespace yaze::editor {
namespace {

TEST(OverworldMapMetadataTest, ProjectMapLabelOverridesDefaultName) {
  project::YazeProject project;
  project.resource_labels["overworld_map"]["2"] = "Custom Meadow";

  const zelda3::Overworld overworld(nullptr);
  const auto metadata =
      BuildOverworldMapMetadata(overworld, nullptr, &project, 2, 0);

  EXPECT_EQ(metadata.map_name, "Custom Meadow");
  EXPECT_NE(metadata.map_title.find("Custom Meadow"), std::string::npos);
}

TEST(OverworldMapMetadataTest, RenameResourceLabelNormalizesAndClearsAliases) {
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
