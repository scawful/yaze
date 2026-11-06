#include "cli/handlers/graphics/palette_commands.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "cli/service/command_registry.h"
#include "core/project.h"
#include "core/rom_settings.h"
#include "rom/rom.h"
#include "zelda3/resource_labels.h"

namespace yaze::cli {
namespace {

using ::testing::HasSubstr;

// ---------------------------------------------------------------------------
// Registration smoke tests
// ---------------------------------------------------------------------------

TEST(PaletteCommandsTest, PaletteGetColorsIsRegistered) {
  auto& registry = CommandRegistry::Instance();
  EXPECT_TRUE(registry.HasCommand("palette-get-colors"));
}

TEST(PaletteCommandsTest, PaletteSetColorIsRegistered) {
  auto& registry = CommandRegistry::Instance();
  EXPECT_TRUE(registry.HasCommand("palette-set-color"));
}

TEST(PaletteCommandsTest, PaletteAnalyzeIsRegistered) {
  auto& registry = CommandRegistry::Instance();
  EXPECT_TRUE(registry.HasCommand("palette-analyze"));
}

TEST(PaletteCommandsTest, PaletteCommandsInGraphicsCategory) {
  auto& registry = CommandRegistry::Instance();
  auto commands = registry.GetCommandsInCategory("graphics");
  EXPECT_THAT(commands, ::testing::Contains("palette-get-colors"));
  EXPECT_THAT(commands, ::testing::Contains("palette-set-color"));
  EXPECT_THAT(commands, ::testing::Contains("palette-analyze"));
}

// ---------------------------------------------------------------------------
// Validation tests (no ROM required)
// ---------------------------------------------------------------------------

TEST(PaletteCommandsTest, GetColorsRequiresGroupArg) {
  handlers::PaletteGetColorsCommandHandler handler;
  std::string output;
  // Empty args => should fail validation with missing 'group'
  absl::Status status = handler.Run({}, nullptr, &output);
  EXPECT_FALSE(status.ok());
}

TEST(PaletteCommandsTest, SetColorRequiresAllArgs) {
  handlers::PaletteSetColorCommandHandler handler;
  std::string output;
  // Missing required args
  absl::Status status = handler.Run({"--group", "swords"}, nullptr, &output);
  EXPECT_FALSE(status.ok());
}

TEST(PaletteCommandsTest,
     LegacySetColorWriteFailsClosedBeforeArgsOrRomLoading) {
  handlers::PaletteSetColorCommandHandler handler;
  std::string output;
  const absl::Status status = handler.Run({"--write"}, nullptr, &output);

  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("palette-set-color --write is disabled"));
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("dungeon-set-palette-color"));
}

// ---------------------------------------------------------------------------
// Help text tests
// ---------------------------------------------------------------------------

TEST(PaletteCommandsTest, GetColorsHelpShowsUsage) {
  auto& registry = CommandRegistry::Instance();
  std::string help = registry.GenerateHelp("palette-get-colors");
  EXPECT_THAT(help, HasSubstr("palette-get-colors"));
}

TEST(PaletteCommandsTest, SetColorHelpShowsUsage) {
  auto& registry = CommandRegistry::Instance();
  std::string help = registry.GenerateHelp("palette-set-color");
  EXPECT_THAT(help, HasSubstr("palette-set-color"));
  EXPECT_THAT(help, ::testing::Not(HasSubstr("[--write]")));
}

TEST(PaletteCommandsTest, AnalyzeHelpShowsUsage) {
  auto& registry = CommandRegistry::Instance();
  std::string help = registry.GenerateHelp("palette-analyze");
  EXPECT_THAT(help, HasSubstr("palette-analyze"));
}

// ---------------------------------------------------------------------------
// Metadata tests
// ---------------------------------------------------------------------------

TEST(PaletteCommandsTest, GetColorsMetadataHasCorrectCategory) {
  auto& registry = CommandRegistry::Instance();
  const auto* metadata = registry.GetMetadata("palette-get-colors");
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(metadata->category, "graphics");
}

TEST(PaletteCommandsTest, SetColorMetadataHasCorrectCategory) {
  auto& registry = CommandRegistry::Instance();
  const auto* metadata = registry.GetMetadata("palette-set-color");
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(metadata->category, "graphics");
}

TEST(PaletteCommandsTest, AnalyzeMetadataHasCorrectCategory) {
  auto& registry = CommandRegistry::Instance();
  const auto* metadata = registry.GetMetadata("palette-analyze");
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(metadata->category, "graphics");
}

// ---------------------------------------------------------------------------
// Handler name/usage tests
// ---------------------------------------------------------------------------

TEST(PaletteCommandsTest, HandlerUsageStrings) {
  handlers::PaletteGetColorsCommandHandler get_handler;
  EXPECT_THAT(get_handler.GetUsage(), HasSubstr("--group"));

  handlers::PaletteSetColorCommandHandler set_handler;
  EXPECT_THAT(set_handler.GetUsage(), HasSubstr("--group"));
  EXPECT_THAT(set_handler.GetUsage(), HasSubstr("--palette"));
  EXPECT_THAT(set_handler.GetUsage(), HasSubstr("--index"));
  EXPECT_THAT(set_handler.GetUsage(), HasSubstr("--color"));

  handlers::PaletteAnalyzeCommandHandler analyze_handler;
  EXPECT_THAT(analyze_handler.GetUsage(), HasSubstr("--group"));
}

TEST(PaletteCommandsTest, GetColorsInvalidIndexReturnsInvalidArgument) {
  handlers::PaletteGetColorsCommandHandler handler;
  Rom rom;
  std::vector<uint8_t> rom_data(0x200000, 0x00);
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  std::string output;
  absl::Status status =
      handler.Run({"--group", "ow_main", "--index", "bad"}, &rom, &output);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("Invalid integer for '--index'"));
}

TEST(PaletteCommandsTest, GetColorsWithoutIndexStillSucceeds) {
  handlers::PaletteGetColorsCommandHandler handler;
  Rom rom;
  std::vector<uint8_t> rom_data(0x200000, 0x00);
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  std::string output;
  absl::Status status = handler.Run({"--group", "ow_main"}, &rom, &output);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(output, HasSubstr("ow_main"));
}

TEST(PaletteCommandsTest, ProjectContextBindsAuxiliaryRowAndRestoresCaller) {
  const auto root =
      std::filesystem::temp_directory_path() /
      ("yaze_palette_binding_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto previous_addresses = core::RomSettings::Get().address_overrides();
  const auto previous_labels = zelda3::GetResourceLabels().SnapshotState();
  struct Restore {
    std::filesystem::path root;
    core::RomAddressOverrides addresses;
    zelda3::ResourceLabelProvider::State labels;
    ~Restore() {
      core::RomSettings::Get().SetAddressOverrides(addresses);
      zelda3::GetResourceLabels().RestoreState(labels);
      std::filesystem::remove_all(root);
    }
  } restore{root, previous_addresses, previous_labels};
  project::YazeProject project;
  ASSERT_TRUE(project.Create("bound", root.string()).ok());
  project.rom_address_overrides.addresses = {
      {core::RomAddressKey::kOverworldAuxPaletteExtensionPc, 0x22F900},
      {core::RomAddressKey::kOverworldAuxPaletteExtensionCount, 32},
      {core::RomAddressKey::kOverworldAuxPaletteExtensionAbi, 1}};
  ASSERT_TRUE(project.Save().ok());
  Rom rom;
  auto bytes = std::vector<uint8_t>(0x240000, 0);
  bytes[0x22F900 + 42] = 0x34;
  bytes[0x22F900 + 43] = 0x12;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  const auto before = rom.vector();
  core::RomSettings::Get().ClearOverrides();
  handlers::PaletteGetColorsCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--group=ow_aux", "--index=21",
                   "--project-context=" + project.filepath, "--format=json"},
                  &rom, &output);
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("SNES=0x1234"));
  EXPECT_THAT(output, HasSubstr("\"palette_count\": 52"));
  EXPECT_TRUE(core::RomSettings::Get().address_overrides().empty());
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(
      handler.Run({"--group=ow_aux", "--index=21"}, &rom, &output).ok());
  EXPECT_TRUE(core::RomSettings::Get().address_overrides().empty());
}

TEST(PaletteCommandsTest, LegacySetColorPreviewRemainsReadOnly) {
  handlers::PaletteSetColorCommandHandler handler;
  Rom rom;
  std::vector<uint8_t> rom_data(0x200000, 0x00);
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());
  rom.set_dirty(false);
  const std::vector<uint8_t> before = rom.vector();

  std::string output;
  const absl::Status status =
      handler.Run({"--group=dungeon_main", "--palette=0", "--index=0",
                   "--color=FF0000", "--format=json"},
                  &rom, &output);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("\"status\": \"dry_run\""));
  EXPECT_THAT(output, HasSubstr("Legacy preview only"));
  EXPECT_THAT(output, ::testing::Not(HasSubstr("Use --write")));
  EXPECT_THAT(output, ::testing::Not(HasSubstr("\"status\": \"written\"")));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

}  // namespace
}  // namespace yaze::cli
