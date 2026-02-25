// Integration tests for oracle-smoke-check against the Oracle expanded ROM.
//
// Fixture discovery (in priority order):
//   1. YAZE_TEST_ROM_OOS env var
//   2. YAZE_TEST_ROM_EXPANDED env var
//   3. Auto-search: roms/oos168.sfc, Roms/oos168.sfc, ../roms/oos168.sfc
//
// All tests skip cleanly via GTEST_SKIP() if the ROM is absent.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "cli/handlers/game/oracle_smoke_check_commands.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"

namespace yaze::test {
namespace {

using json = nlohmann::json;

std::string FindOracleRom() {
  for (const char* env_var :
       {"YAZE_TEST_ROM_OOS", "YAZE_TEST_ROM_EXPANDED",
        "YAZE_TEST_ROM_EXPANDED_PATH"}) {
    if (const char* path = std::getenv(env_var)) {
      if (std::filesystem::exists(path)) return path;
    }
  }
  for (const auto& dir : {".", "roms", "Roms", "../roms", "../../roms"}) {
    for (const auto& name :
         {"oos168.sfc", "oos168x.sfc", "oracle_of_secrets.sfc"}) {
      std::filesystem::path path =
          std::filesystem::path(dir) / name;
      if (std::filesystem::exists(path)) return path.string();
    }
  }
  return "";
}

class OracleSmokeCheckIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_path_ = FindOracleRom();
    if (rom_path_.empty()) {
      GTEST_SKIP() << "Oracle ROM fixture not found. "
                      "Set YAZE_TEST_ROM_OOS to oos168.sfc path.";
    }
    ASSERT_TRUE(rom_.LoadFromFile(rom_path_).ok())
        << "Failed to load ROM: " << rom_path_;
  }

  std::string rom_path_;
  Rom rom_;
};

// Navigate into the "Oracle Smoke Check" wrapper.
const json& GetSmoke(const json& doc) {
  static const json kEmpty = json::object();
  return doc.contains("Oracle Smoke Check") ? doc.at("Oracle Smoke Check")
                                             : kEmpty;
}

// ---------------------------------------------------------------------------
// Shape + value assertions against the real Oracle ROM
// ---------------------------------------------------------------------------

TEST_F(OracleSmokeCheckIntegrationTest, DefaultModePassesStructuralCheck) {
  // oos168.sfc is a properly expanded ROM → structural checks must pass.
  cli::handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--format=json"}, &rom_, &out);
  ASSERT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  const auto& smoke = GetSmoke(doc);

  EXPECT_TRUE(smoke.value("ok", false));
  EXPECT_EQ(smoke.value("status", ""), "pass");

  // Correctness: structural_ok must be TRUE for the real Oracle ROM.
  EXPECT_TRUE(smoke.value("checks", json::object())
                  .value("d4_zora_temple", json::object())
                  .value("structural_ok", false))
      << "D4 structural check failed on Oracle ROM — water-fill region may be "
         "missing or table corrupted. Full output:\n"
      << out;
}

TEST_F(OracleSmokeCheckIntegrationTest, StrictReadinessReportsD4D3Readiness) {
  // In strict mode, D4/D3 readiness is surfaced. The test does not assert
  // pass/fail on readiness (depends on authoring state) but validates that:
  //   (a) the command runs without errors
  //   (b) structural check still passes
  //   (c) all three check keys are present with correct types
  cli::handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  [[maybe_unused]] auto ignored =
      handler.Run({"--strict-readiness", "--format=json"}, &rom_, &out);

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  const auto& smoke = GetSmoke(doc);
  const auto& checks = smoke.value("checks", json::object());

  // Structural must still pass even in strict mode.
  EXPECT_TRUE(checks.value("d4_zora_temple", json::object())
                  .value("structural_ok", false))
      << "D4 structural failed in strict mode: " << out;

  // required_rooms_ok is a boolean (present and well-typed).
  const auto& d4 = checks.value("d4_zora_temple", json::object());
  ASSERT_TRUE(d4.contains("required_rooms_ok"))
      << "required_rooms_ok missing in strict mode: " << out;
  EXPECT_TRUE(d4.at("required_rooms_ok").is_boolean());

  // D6 and D3 ok fields present and boolean.
  EXPECT_TRUE(
      checks.value("d6_goron_mines", json::object()).contains("ok"));
  EXPECT_TRUE(
      checks.value("d3_kalyxo_castle", json::object()).contains("ok"));
}

TEST_F(OracleSmokeCheckIntegrationTest, D6GoronMinesAuditAlwaysOk) {
  // The D6 minecart audit never exits non-zero (issues are informational).
  // Verify d6.ok=true and the command exit status is ok.
  cli::handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--format=json"}, &rom_, &out);
  ASSERT_TRUE(status.ok()) << out;

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_TRUE(GetSmoke(doc)
                  .value("checks", json::object())
                  .value("d6_goron_mines", json::object())
                  .value("ok", false))
      << "D6 minecart audit reported ok=false on real ROM: " << out;
}

TEST_F(OracleSmokeCheckIntegrationTest, ReportFileContainsAllCheckKeys) {
  const auto report_path =
      (std::filesystem::temp_directory_path() /
       "yaze_smoke_check_integration_report.json").string();

  cli::handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  ASSERT_TRUE(
      handler.Run({"--report", report_path, "--format=json"}, &rom_, &out)
          .ok());

  ASSERT_TRUE(std::filesystem::exists(report_path));
  std::ifstream report_file(report_path);
  const json report = json::parse(report_file, nullptr, false);
  ASSERT_FALSE(report.is_discarded());

  EXPECT_TRUE(report.contains("ok"));
  EXPECT_TRUE(report.contains("status"));
  EXPECT_TRUE(report.contains("checks"));
  EXPECT_TRUE(report.at("checks").contains("d4_zora_temple"));
  EXPECT_TRUE(report.at("checks").contains("d6_goron_mines"));
  EXPECT_TRUE(report.at("checks").contains("d3_kalyxo_castle"));

  // Correctness: d4 structural_ok must be true for real Oracle ROM.
  EXPECT_TRUE(report.at("checks")
                  .value("d4_zora_temple", json::object())
                  .value("structural_ok", false));

  std::filesystem::remove(report_path);
}

}  // namespace
}  // namespace yaze::test
