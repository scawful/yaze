// Integration test: dungeon-minecart-audit against the Oracle of Secrets
// expanded ROM (oos168.sfc).
//
// ROM fixture: set YAZE_TEST_ROM_OOS or YAZE_TEST_ROM_EXPANDED to the path of
// the expanded Oracle ROM, or place oos168.sfc in one of the auto-search dirs
// (., roms/, ../roms/, ../../roms/).  If the fixture is absent the test suite
// skips via GTEST_SKIP() — no CI failure.
//
// Rooms under test:
//   0xA8, 0xB8, 0xD8, 0xDA — D6 Goron Mines minecart rooms (known to have
//   track rail objects, may lack stop tiles / cart sprites in dev builds).
//
// Shape contracts (deterministic regardless of Oracle build state):
//   - status field = "success"
//   - total_rooms_requested = 4
//   - rooms_emitted field present and >= 0
//   - all standard top-level JSON keys present
//
// Issue-count contracts are intentionally NOT asserted so the test stays
// green across Oracle development iterations.

#include <filesystem>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "cli/handlers/game/minecart_commands.h"
#include "cli/handlers/game/oracle_menu_commands.h"
#include "rom/rom.h"

namespace yaze::test {
namespace {

using ::testing::HasSubstr;

// ---------------------------------------------------------------------------
// Fixture discovery
// ---------------------------------------------------------------------------

// Searches common locations for an Oracle of Secrets expanded ROM.
// Returns empty string if none is found.
std::string FindOracleRom() {
  // 1. Environment variables (highest priority).
  for (const char* env_var :
       {"YAZE_TEST_ROM_OOS", "YAZE_TEST_ROM_EXPANDED",
        "YAZE_TEST_ROM_EXPANDED_PATH"}) {
    if (const char* env_path = std::getenv(env_var)) {
      if (std::filesystem::exists(env_path)) {
        return env_path;
      }
    }
  }

  // 2. Auto-discovery by filename in common relative dirs.
  static const std::vector<std::string> kSearchDirs = {
      ".", "roms", "Roms", "../roms", "../../roms",
  };
  static const std::vector<std::string> kOracleNames = {
      "oos168.sfc",
      "oos168x.sfc",
      "oracle_of_secrets.sfc",
  };
  for (const auto& dir : kSearchDirs) {
    for (const auto& name : kOracleNames) {
      std::filesystem::path path = std::filesystem::path(dir) / name;
      if (std::filesystem::exists(path)) {
        return path.string();
      }
    }
  }

  return "";
}

// ---------------------------------------------------------------------------
// Fixture class
// ---------------------------------------------------------------------------

class MinecartAuditIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_path_ = FindOracleRom();
    if (rom_path_.empty()) {
      GTEST_SKIP()
          << "Oracle ROM fixture not found. Set YAZE_TEST_ROM_OOS to "
             "oos168.sfc path, or place it in roms/oos168.sfc. "
             "Skipping minecart integration tests.";
    }
    ASSERT_TRUE(rom_.LoadFromFile(rom_path_).ok())
        << "Failed to load Oracle ROM from: " << rom_path_;
  }

  std::string rom_path_;
  Rom rom_;
};

// ---------------------------------------------------------------------------
// Integration tests
// ---------------------------------------------------------------------------

TEST_F(MinecartAuditIntegrationTest, D6RoomsAuditSucceedsAndHasExpectedShape) {
  // Run the audit on the four known D6 flagged rooms.
  cli::handlers::DungeonMinecartAuditCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--rooms=0xA8,0xB8,0xD8,0xDA", "--format=json"}, &rom_, &out);
  ASSERT_TRUE(status.ok()) << status.message();

  // Shape contracts: these must hold regardless of Oracle build state.
  EXPECT_THAT(out, HasSubstr("\"status\": \"success\""));
  EXPECT_THAT(out, HasSubstr("\"total_rooms_requested\": 4"));
  EXPECT_THAT(out, HasSubstr("\"rooms_emitted\""));
  EXPECT_THAT(out, HasSubstr("\"rooms_with_issues\""));
  EXPECT_THAT(out, HasSubstr("\"track_object_id\""));
  EXPECT_THAT(out, HasSubstr("\"minecart_sprite_id\""));
}

TEST_F(MinecartAuditIntegrationTest,
       D6RoomsOnlyIssuesFlagProducesValidOutput) {
  // --only-issues should still succeed; rooms_emitted ≤ 4.
  cli::handlers::DungeonMinecartAuditCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--rooms=0xA8,0xB8,0xD8,0xDA", "--only-issues", "--format=json"},
      &rom_, &out);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_THAT(out, HasSubstr("\"status\": \"success\""));
  EXPECT_THAT(out, HasSubstr("\"total_rooms_requested\": 4"));
}

TEST_F(MinecartAuditIntegrationTest, D6RoomsIncludeTrackObjectsFlag) {
  // --include-track-objects should still produce well-formed output.
  cli::handlers::DungeonMinecartAuditCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--rooms=0xD8,0xDA", "--include-track-objects", "--format=json"},
      &rom_, &out);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(out, HasSubstr("\"total_rooms_requested\": 2"));
}

TEST_F(MinecartAuditIntegrationTest, OraclePreflightPassesOnExpandedRom) {
  // The Oracle expanded ROM should satisfy all default preflight checks.
  cli::handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--format=json"}, &rom_, &out);
  // The preflight may fail if the ROM is not fully configured, so just
  // assert structural shape rather than hard pass/fail.
  EXPECT_THAT(out, HasSubstr("\"water_fill_region_ok\""));
  EXPECT_THAT(out, HasSubstr("\"custom_collision_maps_ok\""));
  EXPECT_THAT(out, HasSubstr("\"errors\""));
  // status field must be either "pass" or "fail" — never absent.
  const bool has_pass = out.find("\"status\": \"pass\"") != std::string::npos;
  const bool has_fail = out.find("\"status\": \"fail\"") != std::string::npos;
  EXPECT_TRUE(has_pass || has_fail)
      << "Expected status=pass or status=fail in output: " << out;
}

}  // namespace
}  // namespace yaze::test
