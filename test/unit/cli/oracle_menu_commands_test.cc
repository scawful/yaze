#include "cli/handlers/game/oracle_menu_commands.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze::cli {
namespace {

using ::testing::HasSubstr;

std::filesystem::path MakeTempRoot() {
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("yaze_oracle_menu_commands_test_" + std::to_string(nonce));
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  ASSERT_FALSE(ec);

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out << text;
  ASSERT_TRUE(out.good());
}

TEST(OracleMenuCommandsTest, ValidateFailsAndEmitsStructuredFailureOutput) {
  const auto root = MakeTempRoot();
  const auto menu_asm = root / "Menu" / "menu.asm";
  WriteTextFile(menu_asm,
                "Menu_ItemCursorPositions:\n"
                "  dw menu_offset(6,2)\n"
                "  dw menu_offset(40,5)\n"
                "menu_frame: incbin \"tilemaps/missing.tilemap\"\n");

  handlers::OracleMenuValidateCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--project=" + root.string(), "--format=json"}, nullptr,
                  &output);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(output, HasSubstr("\"status\": \"fail\""));
  EXPECT_THAT(output, HasSubstr("\"failure_reason\": \"Oracle menu validation failed\""));
  EXPECT_THAT(output, HasSubstr("missing_bin"));
  EXPECT_THAT(output, HasSubstr("component_out_of_bounds"));

  std::error_code ec;
  std::filesystem::remove_all(root, ec);
}

TEST(OracleMenuCommandsTest, ValidateStrictModeFailsOnWarningsOnly) {
  const auto root = MakeTempRoot();
  const auto menu_asm = root / "Menu" / "menu.asm";
  const auto empty_bin = root / "Menu" / "tilemaps" / "empty.tilemap";
  WriteTextFile(menu_asm,
                "Menu_ItemCursorPositions:\n"
                "  dw menu_offset(6,2)\n"
                "menu_frame: incbin \"tilemaps/empty.tilemap\"\n");
  WriteTextFile(empty_bin, "");

  handlers::OracleMenuValidateCommandHandler handler;

  std::string non_strict_output;
  const auto non_strict_status =
      handler.Run({"--project=" + root.string(), "--format=json"}, nullptr,
                  &non_strict_output);
  ASSERT_TRUE(non_strict_status.ok()) << non_strict_status.message();
  EXPECT_THAT(non_strict_output, HasSubstr("\"status\": \"pass\""));
  EXPECT_THAT(non_strict_output, HasSubstr("\"warnings\": 1"));

  std::string strict_output;
  const auto strict_status = handler.Run(
      {"--project=" + root.string(), "--strict", "--format=json"}, nullptr,
      &strict_output);
  EXPECT_FALSE(strict_status.ok());
  EXPECT_EQ(strict_status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(strict_output, HasSubstr("\"status\": \"fail\""));
  EXPECT_THAT(strict_output, HasSubstr("\"errors\": 0"));
  EXPECT_THAT(strict_output, HasSubstr("\"warnings\": 1"));
  EXPECT_THAT(strict_output, HasSubstr("empty_bin"));

  std::error_code ec;
  std::filesystem::remove_all(root, ec);
}

}  // namespace
}  // namespace yaze::cli
