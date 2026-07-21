#include "app/editor/oracle/oracle_hack_workflow_backend.h"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "cli/handlers/game/oracle_smoke_check_commands.h"
#include "gtest/gtest.h"
#include "rom/rom.h"

#ifndef _WIN32
#include <sys/resource.h>
#endif

namespace yaze::editor {
namespace {

std::filesystem::path UniqueTempPath(const char* stem) {
  static int sequence = 0;
  const auto ticks =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         absl::StrFormat("%s_%lld_%d", stem, static_cast<long long>(ticks),
                         sequence++);
}

#ifndef _WIN32
bool SetZeroFileSizeLimit() {
  if (std::signal(SIGXFSZ, SIG_IGN) == SIG_ERR) {
    return false;
  }

  struct rlimit file_size_limit{};
  if (getrlimit(RLIMIT_FSIZE, &file_size_limit) != 0) {
    return false;
  }
  file_size_limit.rlim_cur = 0;
  return setrlimit(RLIMIT_FSIZE, &file_size_limit) == 0;
}

int RunLateReportWriteFailureChild(const std::filesystem::path& report_path) {
  if (!SetZeroFileSizeLimit()) {
    return 2;
  }

  Rom rom;
  if (!rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok()) {
    return 3;
  }

  oracle_validation::PreflightOptions preflight_options;
  preflight_options.skip_collision_maps = true;
  preflight_options.report_path = report_path.string();

  OracleHackWorkflowBackend backend;
  const auto result = backend.RunValidation(
      oracle_validation::RunMode::kPreflight, oracle_validation::SmokeOptions{},
      preflight_options, &rom);
  const auto expected_error = absl::StrFormat(
      "dungeon-oracle-preflight: failed while writing report file: %s",
      report_path.string());

  const bool passed =
      !result.command_ok && result.status_code == absl::StatusCode::kInternal &&
      result.error_message == expected_error && !result.json_parse_failed &&
      result.preflight.has_value() && result.preflight->ok &&
      !result.raw_output.empty();
  return passed ? 0 : 1;
}

int RunLateSmokeReportWriteFailureChild(
    const std::filesystem::path& report_path) {
  if (!SetZeroFileSizeLimit()) {
    return 2;
  }

  Rom rom;
  if (!rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok()) {
    return 3;
  }

  cli::handlers::OracleSmokeCheckCommandHandler handler;
  std::string raw_output;
  const auto status = handler.Run(
      {"--report=" + report_path.string(), "--format=json"}, &rom, &raw_output);
  const auto expected_error = absl::StrFormat(
      "oracle-smoke-check: failed writing report: %s", report_path.string());

  const bool passed = status.code() == absl::StatusCode::kInternal &&
                      status.message() == expected_error && !raw_output.empty();
  return passed ? 0 : 1;
}
#endif

TEST(OracleHackWorkflowBackendTest,
     ParsesStructuredPreflightForPreciseFailureStatus) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  oracle_validation::PreflightOptions preflight_options;
  preflight_options.required_collision_rooms = "0x128,0x129";
  preflight_options.skip_collision_maps = true;

  OracleHackWorkflowBackend backend;
  const auto result = backend.RunValidation(
      oracle_validation::RunMode::kPreflight, oracle_validation::SmokeOptions{},
      preflight_options, &rom);

  EXPECT_TRUE(result.command_ok);
  EXPECT_EQ(result.status_code, absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(result.error_message.empty());
  EXPECT_FALSE(result.json_parse_failed);
  ASSERT_TRUE(result.preflight.has_value());
  EXPECT_FALSE(result.preflight->ok);
  ASSERT_EQ(result.preflight->errors.size(), 2);
  EXPECT_EQ(result.preflight->errors.front().code,
            "ORACLE_REQUIRED_ROOM_OUT_OF_RANGE");
}

TEST(OracleHackWorkflowBackendTest, AcceptsMatchingOkStructuredPreflight) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  oracle_validation::PreflightOptions preflight_options;
  preflight_options.skip_collision_maps = true;

  OracleHackWorkflowBackend backend;
  const auto result = backend.RunValidation(
      oracle_validation::RunMode::kPreflight, oracle_validation::SmokeOptions{},
      preflight_options, &rom);

  EXPECT_TRUE(result.command_ok);
  EXPECT_EQ(result.status_code, absl::StatusCode::kOk);
  EXPECT_TRUE(result.error_message.empty());
  EXPECT_FALSE(result.json_parse_failed);
  ASSERT_TRUE(result.preflight.has_value());
  EXPECT_TRUE(result.preflight->ok);
}

TEST(OracleHackWorkflowBackendTest,
     PreservesExactNoRomStatusWithoutParseError) {
  OracleHackWorkflowBackend backend;
  const auto result = backend.RunValidation(
      oracle_validation::RunMode::kPreflight, oracle_validation::SmokeOptions{},
      oracle_validation::PreflightOptions{}, nullptr);

  EXPECT_FALSE(result.command_ok);
  EXPECT_EQ(result.status_code, absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(result.error_message,
            "No ROM loaded. Use --rom=<path> or --mock-rom for testing.");
  EXPECT_FALSE(result.json_parse_failed);
  EXPECT_FALSE(result.preflight.has_value());
  EXPECT_TRUE(result.raw_output.empty());
}

TEST(OracleHackWorkflowBackendTest,
     PreservesExactReportOpenFailureWithoutParseError) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  const auto missing_dir =
      UniqueTempPath("yaze_oracle_backend_missing_report_dir");
  std::error_code remove_error;
  std::filesystem::remove_all(missing_dir, remove_error);
  const auto report_path = missing_dir / "report.json";

  oracle_validation::PreflightOptions preflight_options;
  preflight_options.report_path = report_path.string();

  OracleHackWorkflowBackend backend;
  const auto result = backend.RunValidation(
      oracle_validation::RunMode::kPreflight, oracle_validation::SmokeOptions{},
      preflight_options, &rom);

  EXPECT_FALSE(result.command_ok);
  EXPECT_EQ(result.status_code, absl::StatusCode::kPermissionDenied);
  EXPECT_EQ(
      result.error_message,
      absl::StrFormat("dungeon-oracle-preflight: cannot open report file for "
                      "writing: %s",
                      report_path.string()));
  EXPECT_FALSE(result.json_parse_failed);
  EXPECT_FALSE(result.preflight.has_value());
  EXPECT_TRUE(result.raw_output.empty());
}

TEST(OracleHackWorkflowBackendTest,
     ParsedDiagnosticsDoNotMaskLateReportWriteFailure) {
#ifdef _WIN32
  GTEST_SKIP() << "RLIMIT_FSIZE is unavailable on Windows";
#else
  const auto report_path =
      UniqueTempPath("yaze_oracle_backend_write_failure.json");
  std::error_code remove_error;
  std::filesystem::remove(report_path, remove_error);

  EXPECT_EXIT(std::_Exit(RunLateReportWriteFailureChild(report_path)),
              ::testing::ExitedWithCode(0), "");
  std::filesystem::remove(report_path, remove_error);
#endif
}

TEST(OracleHackWorkflowBackendTest,
     SmokeReportWriterSurfacesBufferedCloseFailure) {
#ifdef _WIN32
  GTEST_SKIP() << "RLIMIT_FSIZE is unavailable on Windows";
#else
  const auto report_path =
      UniqueTempPath("yaze_oracle_smoke_write_failure.json");
  std::error_code remove_error;
  std::filesystem::remove(report_path, remove_error);

  EXPECT_EXIT(std::_Exit(RunLateSmokeReportWriteFailureChild(report_path)),
              ::testing::ExitedWithCode(0), "");
  std::filesystem::remove(report_path, remove_error);
#endif
}

}  // namespace
}  // namespace yaze::editor
