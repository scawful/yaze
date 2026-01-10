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

}  // namespace
}  // namespace yaze::cli
