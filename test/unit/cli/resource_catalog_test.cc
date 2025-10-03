#include "cli/service/resources/resource_catalog.h"

#include <algorithm>
#include <string>

#include "gtest/gtest.h"

namespace yaze {
namespace cli {
namespace {

TEST(ResourceCatalogTest, SerializeResourceIncludesReturnsArray) {
  const auto& catalog = ResourceCatalog::Instance();
  auto overworld_schema = catalog.GetResource("overworld");
  ASSERT_TRUE(overworld_schema.ok());

  std::string output = catalog.SerializeResource(overworld_schema.value());
  EXPECT_NE(output.find("\"resources\""), std::string::npos);
  EXPECT_NE(output.find("\"returns\":"), std::string::npos);
  EXPECT_NE(output.find("\"tile\""), std::string::npos);
}

TEST(ResourceCatalogTest, SerializeAllResourcesIncludesAgentDescribeMetadata) {
  const auto& catalog = ResourceCatalog::Instance();
  std::string output = catalog.SerializeResources(catalog.AllResources());

  EXPECT_NE(output.find("\"agent\""), std::string::npos);
  EXPECT_NE(output.find("\"effects\":"), std::string::npos);
  EXPECT_NE(output.find("\"returns\":"), std::string::npos);
}

TEST(ResourceCatalogTest, RomSchemaExposesActionsAndMetadata) {
  const auto& catalog = ResourceCatalog::Instance();
  auto rom_schema = catalog.GetResource("rom");
  ASSERT_TRUE(rom_schema.ok());

  const auto& actions = rom_schema->actions;
  ASSERT_EQ(actions.size(), 3);
  EXPECT_EQ(actions[0].name, "validate");
  EXPECT_FALSE(actions[0].effects.empty());
  EXPECT_FALSE(actions[0].returns.empty());
  EXPECT_EQ(actions[1].name, "diff");
  EXPECT_EQ(actions[2].name, "generate-golden");
}

TEST(ResourceCatalogTest, PatchSchemaIncludesAsarAndCreateActions) {
  const auto& catalog = ResourceCatalog::Instance();
  auto patch_schema = catalog.GetResource("patch");
  ASSERT_TRUE(patch_schema.ok());

  const auto& actions = patch_schema->actions;
  ASSERT_GE(actions.size(), 3);
  EXPECT_EQ(actions[0].name, "apply");
  EXPECT_FALSE(actions[0].returns.empty());

  auto has_asar = std::find_if(actions.begin(), actions.end(), [](const auto& action) {
                     return action.name == "apply-asar";
                   });
  EXPECT_NE(has_asar, actions.end());

  auto has_create = std::find_if(actions.begin(), actions.end(), [](const auto& action) {
                       return action.name == "create";
                     });
  EXPECT_NE(has_create, actions.end());
}

TEST(ResourceCatalogTest, DungeonSchemaListsMetadataAndObjectsReturns) {
  const auto& catalog = ResourceCatalog::Instance();
  auto dungeon_schema = catalog.GetResource("dungeon");
  ASSERT_TRUE(dungeon_schema.ok());

  const auto& actions = dungeon_schema->actions;
  ASSERT_EQ(actions.size(), 2);
  EXPECT_EQ(actions[0].name, "export");
  EXPECT_FALSE(actions[0].returns.empty());
  EXPECT_EQ(actions[1].name, "list-objects");
  EXPECT_FALSE(actions[1].returns.empty());
}

TEST(ResourceCatalogTest, YamlSerializationIncludesMetadataAndActions) {
  const auto& catalog = ResourceCatalog::Instance();
  std::string yaml = catalog.SerializeResourcesAsYaml(
      catalog.AllResources(), "0.1.0", "2025-10-01");

  EXPECT_NE(yaml.find("version: \"0.1.0\""), std::string::npos);
  EXPECT_NE(yaml.find("name: \"patch\""), std::string::npos);
  EXPECT_NE(yaml.find("effects:"), std::string::npos);
  EXPECT_NE(yaml.find("returns:"), std::string::npos);
}

}  // namespace
}  // namespace cli
}  // namespace yaze
