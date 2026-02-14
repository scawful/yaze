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

  EXPECT_THAT(help, ::testing::HasSubstr("Set one or more custom collision tiles"));
  EXPECT_THAT(help, ::testing::HasSubstr("10,5,0xB7;50,45,0xBA"));
}

}  // namespace
}  // namespace yaze::cli
