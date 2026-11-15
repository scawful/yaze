#include "cli/service/resources/command_context.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "core/features.h"
#include "core/project.h"
#include "mocks/mock_rom.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace cli {
namespace resources {

class CommandContextTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize mock ROM for testing
    std::vector<uint8_t> test_data(1024, 0);  // 1KB of empty data
    auto status = mock_rom_.SetTestData(test_data);
    ASSERT_TRUE(status.ok());

    previous_feature_flags_ = core::FeatureFlags::get();
    previous_custom_object_state_ =
        zelda3::CustomObjectManager::Get().SnapshotState();

    temp_root_ =
        std::filesystem::temp_directory_path() /
        ("yaze_command_context_test_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp_root_);
  }

  void TearDown() override {
    core::FeatureFlags::get() = previous_feature_flags_;
    zelda3::CustomObjectManager::Get().RestoreState(
        previous_custom_object_state_);
    zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    std::filesystem::remove_all(temp_root_);
  }

  std::string CreateProjectWithCustomObjectConfig(bool feature_flag_enabled,
                                                  bool include_track_map) {
    const std::string project_name =
        "ctxproj_" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());

    project::YazeProject project;
    auto create_status = project.Create(project_name, temp_root_.string());
    EXPECT_TRUE(create_status.ok());

    std::filesystem::path custom_dir = temp_root_ / project_name / "custom";
    std::filesystem::create_directories(custom_dir);
    {
      // Oracle permits a terminator-only custom object as an intentional
      // draw-nothing runtime slot.
      std::ofstream out(custom_dir / "track_LR.bin",
                        std::ios::out | std::ios::binary | std::ios::trunc);
      out.put('\0');
      out.put('\0');
    }

    project.feature_flags.kEnableCustomObjects = feature_flag_enabled;
    project.custom_objects_folder = custom_dir.string();
    if (include_track_map) {
      project.custom_object_files[0x31] = {"track_LR.bin"};
    }
    auto save_status = project.Save();
    EXPECT_TRUE(save_status.ok());
    return project.filepath;
  }

  yaze::test::MockRom mock_rom_;
  core::FeatureFlags::Flags previous_feature_flags_;
  zelda3::CustomObjectManager::State previous_custom_object_state_;
  std::filesystem::path temp_root_;
};

TEST_F(CommandContextTest, LoadsRomFromConfig) {
  CommandContext::Config config;
  config.use_mock_rom = true;

  CommandContext context(config);

  auto rom_or = context.GetRom();
  ASSERT_TRUE(rom_or.ok());
  EXPECT_TRUE(rom_or.value()->is_loaded());
}

TEST_F(CommandContextTest,
       PatternSourceAppliesAndRestoresWithCustomObjectsOff) {
  project::YazeProject project;
  ASSERT_TRUE(project.Create("patterns", temp_root_.string()).ok());
  const auto path =
      std::filesystem::path(project.filepath).parent_path() / "walls.asm";
  {
    std::ofstream out(path);
    out << "org $009E3A\ndw $4890\n";
  }
  project.custom_objects_folder.clear();
  project.custom_object_files.clear();
  project.dungeon_tile_patterns = path.string();
  project.feature_flags.kEnableCustomObjects = false;
  ASSERT_TRUE(project.Save().ok());
  auto& manager = zelda3::CustomObjectManager::Get();
  const auto state = manager.SnapshotState();
  {
    CommandContext::Config config;
    config.use_mock_rom = true;
    config.project_context_path = project.filepath;
    CommandContext context(config);
    ASSERT_TRUE(context.GetRom().ok());
    EXPECT_FALSE(core::FeatureFlags::get().kEnableCustomObjects);
    const auto patterns = manager.GetTilePatterns();
    ASSERT_TRUE(patterns.ok());
    ASSERT_NE(*patterns, nullptr);
    EXPECT_EQ((*patterns)->words.at(0x1E3A), 0x4890);
  }
  EXPECT_EQ(manager.SnapshotState(), state);
}

TEST_F(CommandContextTest,
       InvalidPatternSourceFailsEveryRetryAndRestoresState) {
  project::YazeProject project;
  ASSERT_TRUE(project.Create("bad_patterns", temp_root_.string()).ok());
  const auto path =
      std::filesystem::path(project.filepath).parent_path() / "walls.asm";
  {
    std::ofstream out(path);
    out << "org $009E3A\ndw $4890\nincsrc \"unsupported.asm\"\n";
  }
  project.dungeon_tile_patterns = path.string();
  ASSERT_TRUE(project.Save().ok());
  auto& manager = zelda3::CustomObjectManager::Get();
  const auto state = manager.SnapshotState();
  CommandContext::Config config;
  config.use_mock_rom = true;
  config.project_context_path = project.filepath;
  CommandContext context(config);
  for (int retry = 0; retry < 2; ++retry) {
    EXPECT_TRUE(absl::IsInvalidArgument(context.GetRom().status()));
    EXPECT_EQ(manager.SnapshotState(), state);
  }
}

TEST_F(CommandContextTest, UsesExternalRomContext) {
  CommandContext::Config config;
  config.external_rom_context = &mock_rom_;

  CommandContext context(config);

  auto rom_or = context.GetRom();
  ASSERT_TRUE(rom_or.ok());
  EXPECT_EQ(rom_or.value(), &mock_rom_);
}

TEST_F(CommandContextTest, LoadsRomFromPath) {
  CommandContext::Config config;
  config.rom_path = "test_rom.sfc";  // This would need a real ROM file

  CommandContext context(config);

  // This test would need a real ROM file to pass
  // For now, we expect it to fail gracefully
  auto rom_or = context.GetRom();
  EXPECT_FALSE(rom_or.ok());
}

TEST_F(CommandContextTest, EnsuresLabelsLoaded) {
  CommandContext::Config config;
  config.use_mock_rom = true;

  CommandContext context(config);

  auto rom_or = context.GetRom();
  ASSERT_TRUE(rom_or.ok());

  auto status = context.EnsureLabelsLoaded(rom_or.value());
  EXPECT_TRUE(status.ok());
}

TEST_F(CommandContextTest, GetFormatReturnsConfigFormat) {
  CommandContext::Config config;
  config.format = "text";

  CommandContext context(config);

  EXPECT_EQ(context.GetFormat(), "text");
}

TEST_F(CommandContextTest, IsVerboseReturnsConfigVerbose) {
  CommandContext::Config config;
  config.verbose = true;

  CommandContext context(config);

  EXPECT_TRUE(context.IsVerbose());
}

TEST_F(CommandContextTest, ProjectContextAppliesCustomObjectRuntimeState) {
  core::FeatureFlags::get().kEnableCustomObjects = false;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  EXPECT_EQ(zelda3::DrawRoutineRegistry::Get().GetRoutineIdForObject(0x31),
            zelda3::DrawRoutineIds::kNothing);

  {
    CommandContext::Config config;
    config.use_mock_rom = true;
    config.project_context_path =
        CreateProjectWithCustomObjectConfig(true, true);

    CommandContext context(config);
    auto rom_or = context.GetRom();
    ASSERT_TRUE(rom_or.ok());
    ASSERT_NE(context.GetProjectContext(), nullptr);

    EXPECT_TRUE(core::FeatureFlags::get().kEnableCustomObjects);
    EXPECT_EQ(zelda3::DrawRoutineRegistry::Get().GetRoutineIdForObject(0x31),
              zelda3::DrawRoutineIds::kCustomObject);
    EXPECT_FALSE(
        zelda3::CustomObjectManager::Get().GetEffectiveFileList(0x31).empty());
    EXPECT_FALSE(zelda3::CustomObjectManager::Get().GetBasePath().empty());
  }

  EXPECT_FALSE(core::FeatureFlags::get().kEnableCustomObjects);
  EXPECT_EQ(zelda3::DrawRoutineRegistry::Get().GetRoutineIdForObject(0x31),
            zelda3::DrawRoutineIds::kNothing);
}

TEST_F(CommandContextTest,
       ProjectContextAutoEnablesCustomObjectsWhenProjectHasCustomData) {
  core::FeatureFlags::get().kEnableCustomObjects = false;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();

  CommandContext::Config config;
  config.use_mock_rom = true;
  config.project_context_path =
      CreateProjectWithCustomObjectConfig(false, true);

  CommandContext context(config);
  auto rom_or = context.GetRom();
  ASSERT_TRUE(rom_or.ok());

  EXPECT_TRUE(core::FeatureFlags::get().kEnableCustomObjects);
  EXPECT_EQ(zelda3::DrawRoutineRegistry::Get().GetRoutineIdForObject(0x31),
            zelda3::DrawRoutineIds::kCustomObject);
}

TEST_F(CommandContextTest,
       ProjectContextSeedsTrackObjectMapWhenFolderConfiguredWithoutMap) {
  CommandContext::Config config;
  config.use_mock_rom = true;
  config.project_context_path = CreateProjectWithCustomObjectConfig(
      /*feature_flag_enabled=*/true, /*include_track_map=*/false);

  CommandContext context(config);
  auto rom_or = context.GetRom();
  ASSERT_TRUE(rom_or.ok());

  const auto state = zelda3::CustomObjectManager::Get().SnapshotState();
  auto track_it = state.custom_file_map.find(0x31);
  ASSERT_NE(track_it, state.custom_file_map.end());
  EXPECT_THAT(
      track_it->second,
      ::testing::ElementsAreArray(
          zelda3::CustomObjectManager::DefaultSubtypeFilenamesForObject(0x31)));
}

TEST_F(CommandContextTest, ProjectContextBindsLabelsToTheLoadedProject) {
  project::YazeProject project;
  ASSERT_TRUE(project.Create("labels", temp_root_.string()).ok());
  {
    std::ofstream out(std::filesystem::path(project.filepath).parent_path() /
                      "hack_manifest.json");
    out << R"json({"manifest_version":2,"room_tags":{"tags":[
      {"tag_id":"0x39","address":"0x000000","name":"Context_Tag"}]}})json";
  }
  auto& labels = zelda3::GetResourceLabels();
  {
    CommandContext::Config config;
    config.use_mock_rom = true;
    config.project_context_path = project.filepath;
    CommandContext context(config);
    ASSERT_TRUE(context.GetRom().ok());
    project::YazeProject* loaded = context.GetProjectContext();
    ASSERT_NE(loaded, nullptr);
    ASSERT_TRUE(loaded->hack_manifest.loaded());
    EXPECT_EQ(labels.hack_manifest(), &loaded->hack_manifest);
    EXPECT_EQ(labels.GetLabel(zelda3::ResourceType::kRoomTag, 0x39),
              "Context_Tag");
  }
  EXPECT_EQ(labels.hack_manifest(), nullptr);
}

TEST_F(CommandContextTest, ProjectContextAppliesAndRestoresSpriteLabels) {
  project::YazeProject project;
  ASSERT_TRUE(project.Create("sprite_labels", temp_root_.string()).ok());
  project.resource_labels["sprite"]["82"] = "Collectible";  // slot $52
  ASSERT_TRUE(project.Save().ok());

  auto& labels = zelda3::GetResourceLabels();
  const auto before = labels.SnapshotState();
  const std::string default_name =
      labels.GetDefaultLabel(zelda3::ResourceType::kSprite, 0x52);
  {
    CommandContext::Config config;
    config.use_mock_rom = true;
    config.project_context_path = project.filepath;
    CommandContext context(config);
    ASSERT_TRUE(context.GetRom().ok());
    const auto resolved = zelda3::ResolveSpriteLabel(0x52);
    EXPECT_EQ(resolved.name, "Collectible");
    EXPECT_TRUE(resolved.from_project);
    EXPECT_EQ(resolved.default_name, default_name);
  }
  // The provider must not keep pointing into the destroyed project.
  const auto after = labels.SnapshotState();
  EXPECT_EQ(after.project_labels, before.project_labels);
  EXPECT_EQ(after.hack_manifest, before.hack_manifest);
  EXPECT_EQ(after.prefer_hmagic, before.prefer_hmagic);
  EXPECT_EQ(zelda3::ResolveSpriteLabel(0x52).name, default_name);
}

TEST_F(CommandContextTest, InvalidProjectContextRestoresLabelProvider) {
  auto& labels = zelda3::GetResourceLabels();
  const auto before = labels.SnapshotState();
  CommandContext::Config config;
  config.use_mock_rom = true;
  config.project_context_path = (temp_root_ / "missing.yaze").string();
  CommandContext context(config);
  EXPECT_FALSE(context.GetRom().ok());
  EXPECT_EQ(labels.SnapshotState().project_labels, before.project_labels);
  EXPECT_EQ(labels.SnapshotState().hack_manifest, before.hack_manifest);
}

TEST_F(CommandContextTest, InvalidProjectContextFailsInitialization) {
  CommandContext::Config config;
  config.use_mock_rom = true;
  config.project_context_path = (temp_root_ / "does_not_exist.yaze").string();

  CommandContext context(config);
  auto rom_or = context.GetRom();
  EXPECT_FALSE(rom_or.ok());
  EXPECT_EQ(rom_or.status().code(), absl::StatusCode::kFailedPrecondition);
}

// ArgumentParser Tests
class ArgumentParserTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(ArgumentParserTest, ParsesStringArguments) {
  std::vector<std::string> args = {"--type=dungeon", "--format", "json"};
  ArgumentParser parser(args);

  EXPECT_EQ(parser.GetString("type").value(), "dungeon");
  EXPECT_EQ(parser.GetString("format").value(), "json");
}

TEST_F(ArgumentParserTest, ParsesIntArguments) {
  std::vector<std::string> args = {"--room=0x12", "--count", "42"};
  ArgumentParser parser(args);

  auto room_or = parser.GetInt("room");
  ASSERT_TRUE(room_or.ok());
  EXPECT_EQ(room_or.value(), 0x12);

  auto count_or = parser.GetInt("count");
  ASSERT_TRUE(count_or.ok());
  EXPECT_EQ(count_or.value(), 42);
}

TEST_F(ArgumentParserTest, ParsesHexArguments) {
  std::vector<std::string> args = {"--address=0x1234", "--value", "0xFF"};
  ArgumentParser parser(args);

  auto addr_or = parser.GetHex("address");
  ASSERT_TRUE(addr_or.ok());
  EXPECT_EQ(addr_or.value(), 0x1234);

  auto value_or = parser.GetHex("value");
  ASSERT_TRUE(value_or.ok());
  EXPECT_EQ(value_or.value(), 0xFF);
}

TEST_F(ArgumentParserTest, DetectsFlags) {
  std::vector<std::string> args = {"--verbose", "--debug", "--format=json"};
  ArgumentParser parser(args);

  EXPECT_TRUE(parser.HasFlag("verbose"));
  EXPECT_TRUE(parser.HasFlag("debug"));
  EXPECT_FALSE(parser.HasFlag("format"));  // format is a value, not a flag
}

TEST_F(ArgumentParserTest, GetsPositionalArguments) {
  std::vector<std::string> args = {"command", "--flag", "value", "positional1",
                                   "positional2"};
  ArgumentParser parser(args);

  auto positional = parser.GetPositional();
  EXPECT_THAT(positional,
              ::testing::ElementsAre("command", "positional1", "positional2"));
}

TEST_F(ArgumentParserTest, ValidatesRequiredArguments) {
  std::vector<std::string> args = {"--type=dungeon"};
  ArgumentParser parser(args);

  auto status = parser.RequireArgs({"type"});
  EXPECT_TRUE(status.ok());

  status = parser.RequireArgs({"type", "missing"});
  EXPECT_FALSE(status.ok());
}

TEST_F(ArgumentParserTest, HandlesMissingArguments) {
  std::vector<std::string> args = {"--type=dungeon"};
  ArgumentParser parser(args);

  auto missing = parser.GetString("missing");
  EXPECT_FALSE(missing.has_value());

  auto int_missing = parser.GetInt("missing");
  EXPECT_FALSE(int_missing.ok());
}

// OutputFormatter Tests
class OutputFormatterTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(OutputFormatterTest, CreatesFromString) {
  auto json_formatter = OutputFormatter::FromString("json");
  ASSERT_TRUE(json_formatter.ok());
  EXPECT_TRUE(json_formatter.value().IsJson());

  auto text_formatter = OutputFormatter::FromString("text");
  ASSERT_TRUE(text_formatter.ok());
  EXPECT_TRUE(text_formatter.value().IsText());

  auto invalid_formatter = OutputFormatter::FromString("invalid");
  EXPECT_FALSE(invalid_formatter.ok());
}

TEST_F(OutputFormatterTest, GeneratesValidJson) {
  auto formatter_or = OutputFormatter::FromString("json");
  ASSERT_TRUE(formatter_or.ok());
  auto formatter = std::move(formatter_or.value());

  formatter.BeginObject("Test");
  formatter.AddField("string_field", "value");
  formatter.AddField("int_field", 42);
  formatter.AddField("bool_field", true);
  formatter.AddHexField("hex_field", 0x1234, 4);

  formatter.BeginArray("array_field");
  formatter.AddArrayItem("item1");
  formatter.AddArrayItem("item2");
  formatter.EndArray();

  formatter.EndObject();

  std::string output = formatter.GetOutput();

  EXPECT_THAT(output, ::testing::HasSubstr("\"string_field\": \"value\""));
  EXPECT_THAT(output, ::testing::HasSubstr("\"int_field\": 42"));
  EXPECT_THAT(output, ::testing::HasSubstr("\"bool_field\": true"));
  EXPECT_THAT(output, ::testing::HasSubstr("\"hex_field\": \"0x1234\""));
  EXPECT_THAT(output, ::testing::HasSubstr("\"array_field\": ["));
  EXPECT_THAT(output, ::testing::HasSubstr("\"item1\""));
  EXPECT_THAT(output, ::testing::HasSubstr("\"item2\""));
}

TEST_F(OutputFormatterTest, GeneratesValidJsonArrayOfObjects) {
  auto formatter_or = OutputFormatter::FromString("json");
  ASSERT_TRUE(formatter_or.ok());
  auto formatter = std::move(formatter_or.value());

  formatter.BeginObject("Root");
  formatter.BeginArray("items");
  formatter.BeginObject("item");
  formatter.AddField("name", "first");
  formatter.BeginArray("children");
  formatter.AddArrayItem("leaf");
  formatter.EndArray();
  formatter.EndObject();
  formatter.BeginObject("item");
  formatter.AddField("name", "second");
  formatter.EndObject();
  formatter.EndArray();
  formatter.EndObject();

  const std::string output = formatter.GetOutput();
  EXPECT_THAT(output, ::testing::HasSubstr("\"items\": ["));
  EXPECT_THAT(output, ::testing::HasSubstr("\"children\": ["));
  EXPECT_THAT(output, ::testing::HasSubstr("\"leaf\""));
  EXPECT_THAT(output, ::testing::Not(::testing::HasSubstr("\"item\": {")));
  EXPECT_THAT(output, ::testing::HasSubstr("},\n    {"));
}

TEST_F(OutputFormatterTest, GeneratesValidText) {
  auto formatter_or = OutputFormatter::FromString("text");
  ASSERT_TRUE(formatter_or.ok());
  auto formatter = std::move(formatter_or.value());

  formatter.BeginObject("Test Object");
  formatter.AddField("string_field", "value");
  formatter.AddField("int_field", 42);
  formatter.AddField("bool_field", true);
  formatter.AddHexField("hex_field", 0x1234, 4);

  formatter.BeginArray("array_field");
  formatter.AddArrayItem("item1");
  formatter.AddArrayItem("item2");
  formatter.EndArray();

  formatter.EndObject();

  std::string output = formatter.GetOutput();

  EXPECT_THAT(output, ::testing::HasSubstr("=== Test Object ==="));
  EXPECT_THAT(output, ::testing::HasSubstr("string_field         : value"));
  EXPECT_THAT(output, ::testing::HasSubstr("int_field            : 42"));
  EXPECT_THAT(output, ::testing::HasSubstr("bool_field           : yes"));
  EXPECT_THAT(output, ::testing::HasSubstr("hex_field            : 0x1234"));
  EXPECT_THAT(output, ::testing::HasSubstr("array_field:"));
  EXPECT_THAT(output, ::testing::HasSubstr("- item1"));
  EXPECT_THAT(output, ::testing::HasSubstr("- item2"));
}

TEST_F(OutputFormatterTest, EscapesJsonStrings) {
  auto formatter_or = OutputFormatter::FromString("json");
  ASSERT_TRUE(formatter_or.ok());
  auto formatter = std::move(formatter_or.value());

  formatter.BeginObject("Test");
  formatter.AddField("quotes", "He said \"Hello\"");
  formatter.AddField("newlines", "Line1\nLine2");
  formatter.AddField("backslashes", "Path\\to\\file");
  formatter.EndObject();

  std::string output = formatter.GetOutput();

  EXPECT_THAT(output, ::testing::HasSubstr("\\\""));
  EXPECT_THAT(output, ::testing::HasSubstr("\\n"));
  EXPECT_THAT(output, ::testing::HasSubstr("\\\\"));
}

TEST_F(OutputFormatterTest, HandlesEmptyObjects) {
  auto formatter_or = OutputFormatter::FromString("json");
  ASSERT_TRUE(formatter_or.ok());
  auto formatter = std::move(formatter_or.value());

  formatter.BeginObject("Empty");
  formatter.EndObject();

  std::string output = formatter.GetOutput();
  EXPECT_THAT(output, ::testing::HasSubstr("{}"));
}

TEST_F(OutputFormatterTest, HandlesEmptyArrays) {
  auto formatter_or = OutputFormatter::FromString("json");
  ASSERT_TRUE(formatter_or.ok());
  auto formatter = std::move(formatter_or.value());

  formatter.BeginObject("Test");
  formatter.BeginArray("empty_array");
  formatter.EndArray();
  formatter.EndObject();

  std::string output = formatter.GetOutput();
  EXPECT_THAT(output, ::testing::HasSubstr("\"empty_array\": []"));
}

}  // namespace resources
}  // namespace cli
}  // namespace yaze
