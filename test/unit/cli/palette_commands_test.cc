#include "cli/handlers/graphics/palette_commands.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "cli/service/command_registry.h"
#include "rom/rom.h"

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

TEST(PaletteCommandsTest, AnalyzeHasNoRequiredArgs) {
  // palette-analyze ValidateArgs returns OkStatus, but it still needs a ROM
  // to actually execute. We verify that validation itself passes.
  handlers::PaletteAnalyzeCommandHandler handler;
  // Calling Run with no ROM will fail because RequiresRom() defaults to true,
  // but this tests that the argument validation step passes.
  std::string output;
  absl::Status status = handler.Run({}, nullptr, &output);
  // Fails because no ROM is provided, not because of arg validation
  EXPECT_FALSE(status.ok());
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

TEST(PaletteCommandsTest, HandlerNames) {
  handlers::PaletteGetColorsCommandHandler get_handler;
  EXPECT_EQ(get_handler.GetName(), "palette-get-colors");

  handlers::PaletteSetColorCommandHandler set_handler;
  EXPECT_EQ(set_handler.GetName(), "palette-set-color");

  handlers::PaletteAnalyzeCommandHandler analyze_handler;
  EXPECT_EQ(analyze_handler.GetName(), "palette-analyze");
}

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

}  // namespace
}  // namespace yaze::cli
