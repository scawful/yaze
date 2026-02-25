#include "cli/service/command_registry.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze::cli {
namespace {

TEST(CommandRegistryTest, RomCategoryIncludesCoreCommands) {
  auto& registry = CommandRegistry::Instance();
  auto categories = registry.GetCategories();

  EXPECT_THAT(categories, ::testing::Contains("rom"));

  auto commands = registry.GetCommandsInCategory("rom");
  EXPECT_THAT(commands, ::testing::Contains("rom-info"));
  EXPECT_THAT(commands, ::testing::Contains("rom-validate"));
}

TEST(CommandRegistryTest, TestCategoryIncludesCliCommands) {
  auto& registry = CommandRegistry::Instance();
  auto categories = registry.GetCategories();

  EXPECT_THAT(categories, ::testing::Contains("test"));

  auto commands = registry.GetCommandsInCategory("test");
  EXPECT_THAT(commands, ::testing::Contains("test-list"));
  EXPECT_THAT(commands, ::testing::Contains("test-run"));
  EXPECT_THAT(commands, ::testing::Contains("test-status"));
}

TEST(CommandRegistryTest, DungeonPlaceSpriteHelpIncludesExamples) {
  auto& registry = CommandRegistry::Instance();
  const std::string help = registry.GenerateHelp("dungeon-place-sprite");

  EXPECT_THAT(help, ::testing::HasSubstr("Place a dungeon sprite"));
  EXPECT_THAT(help, ::testing::HasSubstr("Examples:"));
  EXPECT_THAT(help, ::testing::HasSubstr("--subtype=4"));
  EXPECT_THAT(help, ::testing::HasSubstr("--write"));
}

TEST(CommandRegistryTest, DungeonSetCollisionTileHelpIncludesTilesSyntax) {
  auto& registry = CommandRegistry::Instance();
  const std::string help = registry.GenerateHelp("dungeon-set-collision-tile");

  EXPECT_THAT(help,
              ::testing::HasSubstr("Set one or more custom collision tiles"));
  EXPECT_THAT(help, ::testing::HasSubstr("10,5,0xB7;50,45,0xBA"));
}

TEST(CommandRegistryTest, DungeonOraclePreflightHelpIncludesExamples) {
  // dungeon-oracle-preflight starts with "dungeon-" not "oracle-"; examples
  // must be registered in the dungeon branch and thus be reachable.
  auto& registry = CommandRegistry::Instance();
  const std::string help = registry.GenerateHelp("dungeon-oracle-preflight");

  EXPECT_THAT(help, ::testing::HasSubstr("Examples:"));
  EXPECT_THAT(help, ::testing::HasSubstr("required-collision-rooms"));
  EXPECT_THAT(help, ::testing::HasSubstr("--report="));
}

TEST(CommandRegistryTest, OracleSmokeCheckHelpIncludesExamples) {
  // oracle-smoke-check starts with "oracle-" and its examples are in the
  // oracle branch — verify they're actually reachable.
  auto& registry = CommandRegistry::Instance();
  const std::string help = registry.GenerateHelp("oracle-smoke-check");

  EXPECT_THAT(help, ::testing::HasSubstr("Examples:"));
  EXPECT_THAT(help, ::testing::HasSubstr("--strict-readiness"));
  EXPECT_THAT(help, ::testing::HasSubstr("--min-d6-track-rooms="));
  EXPECT_THAT(help, ::testing::HasSubstr("--report="));
}

TEST(CommandRegistryTest, ProjectBundlePackHelpIncludesExamples) {
  auto& registry = CommandRegistry::Instance();
  const std::string help = registry.GenerateHelp("project-bundle-pack");

  EXPECT_THAT(help, ::testing::HasSubstr("Examples:"));
  EXPECT_THAT(help, ::testing::HasSubstr("--project"));
  EXPECT_THAT(help, ::testing::HasSubstr("--out"));
  EXPECT_THAT(help, ::testing::HasSubstr("--overwrite"));
}

TEST(CommandRegistryTest, ProjectBundleUnpackHelpIncludesExamples) {
  auto& registry = CommandRegistry::Instance();
  const std::string help = registry.GenerateHelp("project-bundle-unpack");

  EXPECT_THAT(help, ::testing::HasSubstr("Examples:"));
  EXPECT_THAT(help, ::testing::HasSubstr("--archive"));
  EXPECT_THAT(help, ::testing::HasSubstr("--out"));
}

TEST(CommandRegistryTest, DungeonRoomGraphRegisteredAndHasUsage) {
  auto& registry = CommandRegistry::Instance();
  auto dungeon_commands = registry.GetCommandsInCategory("dungeon");

  EXPECT_THAT(dungeon_commands, ::testing::Contains("dungeon-room-graph"));

  const std::string help = registry.GenerateHelp("dungeon-room-graph");
  EXPECT_THAT(help, ::testing::HasSubstr("dungeon-room-graph --entrance"));
  EXPECT_THAT(help, ::testing::HasSubstr("--same-blockset"));
}

}  // namespace
}  // namespace yaze::cli
