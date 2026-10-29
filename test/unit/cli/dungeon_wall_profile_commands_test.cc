#include "cli/handlers/game/dungeon_wall_profile_commands.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "cli/service/command_registry.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "unique_temp_path.h"
#include "zelda3/dungeon/dungeon_tile_patterns.h"

namespace yaze::cli {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

std::string Read(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

void Write(const fs::path& path, const std::string& value) {
  std::ofstream(path, std::ios::binary | std::ios::trunc) << value;
}

class DungeonWallProfileCommandsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    directory_ = test::UniqueTempPath("wall_profile_commands");
    fs::create_directories(directory_);
    source_ = directory_ / "profiles.json";
    legacy_ = directory_ / "legacy.asm";
    output_ = directory_ / "preview.asm";
    Write(source_, R"({"schema_version":1,"profiles":[
      {"id":"timber","patterns":[{"name":"upper_n","pc_offset":7738,
       "words":[0,65535]}]}],"bindings":[{"room":260,"profile":"timber"}]})");
    Write(legacy_, "lorom\norg $009E3A\n dw $0890, $0891, $0892\n");
  }
  void TearDown() override {
    std::error_code error;
    fs::remove_all(directory_, error);
  }
  absl::Status Run(std::vector<std::string> extra, std::string* output) {
    std::vector<std::string> args{"--source=" + source_.string(),
                                  "--room=0x104", "--mock-rom",
                                  "--format=json"};
    args.insert(args.end(), extra.begin(), extra.end());
    handlers::DungeonWallProfileCommandHandler handler;
    return handler.Run(args, nullptr, output);
  }
  fs::path directory_;
  fs::path source_;
  fs::path legacy_;
  fs::path output_;
};

TEST_F(DungeonWallProfileCommandsTest,
       ValidateOnlyReportsSelectedAndWordProvenance) {
  std::string output;
  ASSERT_TRUE(Run({"--legacy-source=" + legacy_.string()}, &output).ok())
      << output;
  const auto result = Json::parse(output);
  EXPECT_EQ(result["selected_profile"], "timber");
  EXPECT_EQ(result["resolved_word_count"], 3);
  EXPECT_EQ(result["profile_word_count"], 2);
  EXPECT_EQ(result["legacy_word_count"], 1);
  EXPECT_FALSE(result["preview_written"].get<bool>());
  EXPECT_FALSE(result["rom_written"].get<bool>());
  EXPECT_EQ(result["words"][0]["word"], 0);
  EXPECT_EQ(result["words"][0]["origin"], "profile");
  EXPECT_EQ(result["words"][2]["origin"], "legacy");
  EXPECT_FALSE(fs::exists(output_));
}

TEST_F(DungeonWallProfileCommandsTest,
       FreshPreviewPreservesLiteralZeroAndInputs) {
  const auto source_before = Read(source_);
  const auto legacy_before = Read(legacy_);
  std::string output;
  ASSERT_TRUE(Run({"--legacy-source=" + legacy_.string(),
                   "--output=" + output_.string()},
                  &output)
                  .ok())
      << output;
  auto parsed = zelda3::LoadDungeonTilePatterns(output_.string());
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->words.at(0x1E3A), 0);
  EXPECT_EQ(parsed->words.at(0x1E3C), 0xFFFF);
  EXPECT_EQ(parsed->words.at(0x1E3E), 0x0892);
  EXPECT_NE(Read(output_).find("dw $0000"), std::string::npos);
  EXPECT_EQ(Read(source_), source_before);
  EXPECT_EQ(Read(legacy_), legacy_before);
  const auto published = Read(output_);
  EXPECT_FALSE(Run({"--legacy-source=" + legacy_.string(),
                    "--output=" + output_.string()},
                   &output)
                   .ok());
  EXPECT_EQ(Read(output_), published);
}

TEST_F(DungeonWallProfileCommandsTest,
       UnselectedCanEmitCompleteLegacySnapshot) {
  handlers::DungeonWallProfileCommandHandler handler;
  std::string output;
  ASSERT_TRUE(
      handler
          .Run({"--source=" + source_.string(), "--room=0x28",
                "--legacy-source=" + legacy_.string(),
                "--output=" + output_.string(), "--mock-rom", "--format=json"},
               nullptr, &output)
          .ok())
      << output;
  auto original = zelda3::LoadDungeonTilePatterns(legacy_.string());
  auto generated = zelda3::LoadDungeonTilePatterns(output_.string());
  ASSERT_TRUE(original.ok());
  ASSERT_TRUE(generated.ok());
  EXPECT_EQ(original->words, generated->words);
  EXPECT_FALSE(Json::parse(output)["selected"].get<bool>());
}

TEST_F(DungeonWallProfileCommandsTest, RejectsExistingOutputAndInputAliases) {
  const auto source_before = Read(source_);
  const auto legacy_before = Read(legacy_);
  std::string output;
  for (const auto& destination : {source_, legacy_}) {
    EXPECT_FALSE(Run({"--legacy-source=" + legacy_.string(),
                      "--output=" + destination.string()},
                     &output)
                     .ok());
  }
  Write(output_, "foreign output must survive");
  EXPECT_FALSE(Run({"--output=" + output_.string()}, &output).ok());
  EXPECT_EQ(Read(output_), "foreign output must survive");
  EXPECT_EQ(Read(source_), source_before);
  EXPECT_EQ(Read(legacy_), legacy_before);
}

TEST_F(DungeonWallProfileCommandsTest, RejectsSymlinkAndHardlinkInputAliases) {
  for (bool symlink : {false, true}) {
    const auto alias = directory_ / (symlink ? "sym.asm" : "hard.asm");
    std::error_code error;
    if (symlink)
      fs::create_symlink(source_, alias, error);
    else
      fs::create_hard_link(source_, alias, error);
    if (error)
      continue;  // Native CI filesystems may forbid link creation.
    const auto before = Read(source_);
    std::string output;
    EXPECT_FALSE(Run({"--output=" + alias.string()}, &output).ok());
    EXPECT_EQ(Read(source_), before);
  }
}

TEST_F(DungeonWallProfileCommandsTest, RejectsParentSymlinkResolvedInputAlias) {
  const auto real_parent = directory_ / "real";
  fs::create_directories(real_parent / "child");
  const auto protected_source = real_parent / "profiles.json";
  Write(protected_source, Read(source_));
  std::error_code error;
  fs::create_directory_symlink(real_parent / "child", directory_ / "view",
                               error);
  if (error)
    GTEST_SKIP() << error.message();
  const auto alias = directory_ / "view" / ".." / "profiles.json";
  if (!fs::equivalent(alias, protected_source, error) || error) {
    GTEST_SKIP() << "Platform resolves symlink-parent traversal differently";
  }
  source_ = protected_source;
  const auto before = Read(source_);
  std::string output;
  EXPECT_FALSE(Run({"--output=" + alias.string()}, &output).ok());
  EXPECT_EQ(Read(source_), before);
}

TEST_F(DungeonWallProfileCommandsTest, RefusesDanglingOutputSymlink) {
  const auto target = directory_ / "uncreated.asm";
  std::error_code error;
  fs::create_symlink(target, output_, error);
  if (error)
    GTEST_SKIP() << error.message();
  std::string output;
  EXPECT_FALSE(Run({"--output=" + output_.string()}, &output).ok());
  EXPECT_FALSE(fs::exists(target));
  EXPECT_TRUE(fs::is_symlink(fs::symlink_status(output_)));
}

TEST_F(DungeonWallProfileCommandsTest, RequiresSourceAndRoom) {
  handlers::DungeonWallProfileCommandHandler handler;
  std::string output;
  EXPECT_FALSE(handler.Run({"--room=0", "--mock-rom"}, nullptr, &output).ok());
  EXPECT_FALSE(
      handler
          .Run({"--source=" + source_.string(), "--mock-rom"}, nullptr, &output)
          .ok());
}

TEST_F(DungeonWallProfileCommandsTest,
       ProtectsActiveAndSandboxSourceRomIdentity) {
  const auto active = directory_ / "active.sfc";
  const auto original = directory_ / "original.sfc";
  Write(active, "active ROM sentinel");
  Write(original, "source ROM sentinel");
  for (const auto& destination : {active, original}) {
    handlers::DungeonWallProfileCommandHandler handler;
    resources::ArgumentParser parser({"--source=" + source_.string(),
                                      "--room=0x104",
                                      "--output=" + destination.string()});
    resources::OutputFormatter formatter(
        resources::OutputFormatter::Format::kJson);
    resources::CommandInvocationContext invocation;
    invocation.active_rom_path = active;
    invocation.source_rom_path = original;
    invocation.sandbox_enabled = true;
    EXPECT_FALSE(
        handler.ExecuteWithContext(nullptr, parser, formatter, invocation)
            .ok());
  }
  EXPECT_EQ(Read(active), "active ROM sentinel");
  EXPECT_EQ(Read(original), "source ROM sentinel");
}

TEST_F(DungeonWallProfileCommandsTest,
       RejectsInvalidContextsWithoutCreatingOutput) {
  for (const std::string& field :
       {"--room=-1", "--room=0x128", "--room=2junk", "--world=2",
        "--world=true", "--entrance=0x85"}) {
    handlers::DungeonWallProfileCommandHandler handler;
    std::vector<std::string> args{"--source=" + source_.string(),
                                  "--output=" + output_.string(), "--mock-rom"};
    if (field.find("--room=") != 0)
      args.push_back("--room=0x104");
    args.push_back(field);
    std::string output;
    EXPECT_FALSE(handler.Run(args, nullptr, &output).ok());
    EXPECT_FALSE(fs::exists(output_));
  }
}

TEST_F(DungeonWallProfileCommandsTest, RejectsMalformedAndAmbiguousSources) {
  std::string output;
  Write(source_, "{broken");
  EXPECT_FALSE(Run({"--output=" + output_.string()}, &output).ok());
  EXPECT_FALSE(fs::exists(output_));
  Write(source_, R"({"schema_version":1,"profiles":[{"id":"a","patterns":[
    {"name":"upper_n","pc_offset":7738,"words":[0]}]}],"bindings":[
    {"room":260,"profile":"a","world":0},
    {"room":260,"profile":"a","entrance":4}]})");
  EXPECT_FALSE(Run({"--output=" + output_.string()}, &output).ok());
  EXPECT_FALSE(fs::exists(output_));
}

TEST(DungeonWallProfileCommandDiscoveryTest, RegistryExportsAllArguments) {
  auto& registry = CommandRegistry::Instance();
  ASSERT_TRUE(registry.HasCommand("dungeon-wall-profile"));
  const auto* metadata = registry.GetMetadata("dungeon-wall-profile");
  ASSERT_NE(metadata, nullptr);
  for (const auto* option : {"--source", "--room", "--world", "--entrance",
                             "--legacy-source", "--output"}) {
    EXPECT_NE(metadata->usage.find(option), std::string::npos);
  }
  EXPECT_NE(registry.ExportFunctionSchemas().find("dungeon-wall-profile"),
            std::string::npos);
}

}  // namespace
}  // namespace yaze::cli
