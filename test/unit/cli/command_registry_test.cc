#include "cli/service/command_registry.h"

#include <memory>
#include <string>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "nlohmann/json.hpp"

namespace yaze::cli {
namespace {

class SchemaExportTestHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "schema-json-escaping-test"; }
  std::string GetUsage() const override { return GetName(); }
  bool RequiresRom() const override { return false; }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser&) override {
    return absl::OkStatus();
  }
  absl::Status Execute(Rom*, const resources::ArgumentParser&,
                       resources::OutputFormatter&) override {
    return absl::OkStatus();
  }
};

TEST(CommandRegistryTest, ExportFunctionSchemasRoundTripsJsonStrings) {
  auto& registry = CommandRegistry::Instance();
  auto handler = std::make_unique<SchemaExportTestHandler>();
  CommandRegistry::CommandMetadata metadata;
  metadata.name = handler->GetName();
  metadata.category = "test\\schemas";
  metadata.description = "Inspect \"C:\\tools\\z3ed\"";
  for (int value = 0; value < 0x20; ++value) {
    metadata.description.push_back(static_cast<char>(value));
  }
  metadata.usage = "schema-json-escaping-test --path \"C:\\tools\\z3ed\"";
  metadata.available_to_agent = true;
  metadata.requires_rom = false;
  metadata.requires_grpc = false;
  metadata.aliases = {"schema\\alias"};
  metadata.examples = {metadata.usage + "\n\t--format json"};
  metadata.todo_reference = "todo#schema\\json";
  registry.Register(std::move(handler), metadata);

  const auto schemas =
      nlohmann::json::parse(registry.ExportFunctionSchemas(), nullptr, false);
  ASSERT_FALSE(schemas.is_discarded());
  ASSERT_TRUE(schemas.contains("commands"));
  for (const auto& command : schemas["commands"]) {
    if (command["name"] != metadata.name) {
      continue;
    }
    EXPECT_EQ(command["category"], metadata.category);
    EXPECT_EQ(command["description"], metadata.description);
    EXPECT_EQ(command["usage"], metadata.usage);
    EXPECT_EQ(command["aliases"], metadata.aliases);
    EXPECT_EQ(command["examples"], metadata.examples);
    EXPECT_EQ(command["todo_reference"], metadata.todo_reference);
    return;
  }
  FAIL() << "Registered command was not exported";
}

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
  EXPECT_THAT(help, ::testing::HasSubstr("--rom oos168x.sfc"));
  EXPECT_THAT(help, ::testing::Not(::testing::HasSubstr("--rom oos168.sfc")));
  EXPECT_THAT(help, ::testing::HasSubstr("required-water-fill-rooms"));
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

TEST(CommandRegistryTest, MesenAutonomyCommandsExposeUsage) {
  auto& registry = CommandRegistry::Instance();

  const std::string await_help = registry.GenerateHelp("mesen-await");
  EXPECT_THAT(await_help, ::testing::HasSubstr("--type <frame|pc|breakpoint>"));
  EXPECT_THAT(await_help, ::testing::HasSubstr("--timeout-ms"));

  const std::string goal_help = registry.GenerateHelp("mesen-goal");
  EXPECT_THAT(
      goal_help,
      ::testing::HasSubstr("--goal <break-at|run-frames|capture-state-at-pc>"));
  EXPECT_THAT(goal_help, ::testing::HasSubstr("--address <hex>"));
  EXPECT_THAT(goal_help, ::testing::HasSubstr("--count <n>"));

  const std::string session_help = registry.GenerateHelp("mesen-session");
  EXPECT_THAT(session_help, ::testing::HasSubstr(
                                "mesen-session [--action "
                                "<show|reset|export|import>] [--file <path>]"));

  const std::string verify_help = registry.GenerateHelp("mesen-state-verify");
  EXPECT_THAT(verify_help,
              ::testing::HasSubstr(
                  "mesen-state-verify --state <path> --rom-file <path>"));

  const std::string regen_help = registry.GenerateHelp("mesen-state-regen");
  EXPECT_THAT(regen_help, ::testing::HasSubstr(
                              "mesen-state-regen --state <path> --rom-file "
                              "<path>"));

  const std::string capture_help = registry.GenerateHelp("mesen-state-capture");
  EXPECT_THAT(capture_help, ::testing::HasSubstr(
                                "mesen-state-capture --state <path> --rom-file "
                                "<path>"));
  EXPECT_THAT(capture_help, ::testing::HasSubstr("--slot <n>"));

  const std::string hook_help = registry.GenerateHelp("mesen-state-hook");
  EXPECT_THAT(hook_help,
              ::testing::HasSubstr(
                  "mesen-state-hook --state <path> --rom-file <path>"));
}

}  // namespace
}  // namespace yaze::cli
