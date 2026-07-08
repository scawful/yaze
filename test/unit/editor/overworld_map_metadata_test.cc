#include "app/editor/overworld/overworld_map_metadata.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "core/project.h"
#include "rom/rom.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_map.h"
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

std::unique_ptr<zelda3::Overworld> BuildMetadataTestOverworld(Rom* rom) {
  auto overworld = std::make_unique<zelda3::Overworld>(rom);
  auto& maps = const_cast<std::vector<zelda3::OverworldMap>&>(
      overworld->overworld_maps());
  maps.clear();
  maps.reserve(zelda3::kNumOverworldMaps);
  for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
    maps.emplace_back(i, rom);
    maps.back().SetAsSmallMap(i);
  }
  return overworld;
}

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

TEST_F(OverworldMapMetadataTest, MetadataUsesEditableProjectResourceLabels) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto overworld = BuildMetadataTestOverworld(&rom);

  auto* map = overworld->mutable_overworld_map(0x02);
  ASSERT_NE(map, nullptr);
  map->set_area_graphics(0x22);
  map->set_area_palette(0x03);
  map->set_sprite_graphics(0, 0x24);
  map->set_sprite_palette(0, 0x05);
  map->set_animated_gfx(0x26);
  map->set_message_id(0x0123);
  *map->mutable_area_music(0) = 0x07;

  project::YazeProject project;
  project.resource_labels["graphics"]["34"] = "Oracle Meadow GFX";
  project.resource_labels["graphics"]["36"] = "Oracle Sprite GFX";
  project.resource_labels["graphics"]["38"] = "Oracle Animated GFX";
  project.resource_labels["overworld_area_palette"]["3"] = "Oracle Grass Pal";
  project.resource_labels["overworld_sprite_palette"]["5"] =
      "Oracle Sprite Pal";
  project.resource_labels["message"]["291"] = "Oracle Meadow Sign";
  project.resource_labels["overworld_music"]["7"] = "Oracle Field Cue";

  const auto metadata =
      BuildOverworldMapMetadata(*overworld, &rom, &project, 0x02, 0);

  EXPECT_NE(metadata.area_gfx_label.find("Oracle Meadow GFX"),
            std::string::npos);
  EXPECT_NE(metadata.sprite_gfx_label.find("Oracle Sprite GFX"),
            std::string::npos);
  EXPECT_NE(metadata.animated_gfx_label.find("Oracle Animated GFX"),
            std::string::npos);
  EXPECT_NE(metadata.area_palette_label.find("Oracle Grass Pal"),
            std::string::npos);
  EXPECT_NE(metadata.sprite_palette_label.find("Oracle Sprite Pal"),
            std::string::npos);
  EXPECT_NE(metadata.message_label.find("Oracle Meadow Sign"),
            std::string::npos);
  EXPECT_NE(metadata.music_label.find("Oracle Field Cue"), std::string::npos);
}

}  // namespace
}  // namespace yaze::editor
