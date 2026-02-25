// Unit tests for OracleSmokeCheckCommandHandler.
//
// Tests use blank synthetic ROMs (no fixture required) and assert both JSON
// field values and exit status. The D6 minecart audit runs on 4 rooms from a
// blank ROM (~2-3 s); this is acceptable since these tests are correctness-
// focused, not performance benchmarks.
//
// Key correctness assertions (beyond JSON shape):
//   - Small ROM → structural failure reported
//   - Strict-readiness mode fails when rooms lack collision data
//   - Default mode passes even when D4/D3 rooms lack collision (informational)
//   - --report writes valid JSON with all check keys

#include "cli/handlers/game/oracle_smoke_check_commands.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "cli/handlers/game/dungeon_collision_commands.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"

namespace yaze::cli {
namespace {

using json = nlohmann::json;

constexpr int kSmallRomSize = 0x100000;  // no water-fill region
constexpr int kFullRomSize = 0x200000;   // water-fill region present

// Injects a stop tile into room `room_id` so the required-room check passes.
absl::Status InjectCollisionTile(Rom* rom, int room_id, int offset) {
  const std::string body = absl::StrFormat(
      R"({"version":1,"rooms":[{"room_id":"0x%02X","tiles":[[%d,184]]}]})",
      room_id, offset);
  auto tmp = (std::filesystem::temp_directory_path() /
              "yaze_smoke_inject_collision.json").string();
  {
    std::ofstream f(tmp, std::ios::out | std::ios::binary | std::ios::trunc);
    f << body;
  }
  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--in", tmp, "--format=json"}, rom, &out);
  std::filesystem::remove(tmp);
  return status;
}

// Navigate into the "Oracle Smoke Check" wrapper that BeginObject emits at
// indent_level > 0 (from inside Execute()).
const json& GetSmoke(const json& doc) {
  static const json kEmpty = json::object();
  if (!doc.contains("Oracle Smoke Check")) return kEmpty;
  return doc.at("Oracle Smoke Check");
}

// ---------------------------------------------------------------------------
// Default-mode tests (structural only)
// ---------------------------------------------------------------------------

TEST(OracleSmokeCheckTest, FullRomPassesStructuralCheckByDefault) {
  // A 0x200000 blank ROM satisfies all structural checks.
  // Required-room gaps are informational and don't fail the default mode.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--format=json"}, &rom, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  const auto& smoke = GetSmoke(doc);
  EXPECT_TRUE(smoke.value("ok", false));
  EXPECT_EQ(smoke.value("status", ""), "pass");
  EXPECT_FALSE(smoke.value("strict_readiness", true));
}

TEST(OracleSmokeCheckTest, SmallRomFailsStructuralAndSkipsReadiness) {
  // 0x100000 ROM: no water-fill region (structural fail) and no
  // HasCustomCollisionWriteSupport (readiness checks skipped).
  // required_rooms_ok must NOT appear; required_rooms_check must be "skipped".
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kSmallRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--format=json"}, &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  const auto& smoke = GetSmoke(doc);
  EXPECT_FALSE(smoke.value("ok", true));
  EXPECT_EQ(smoke.value("status", ""), "fail");

  const auto& checks = smoke.value("checks", json::object());
  const auto& d4_check = checks.value("d4_zora_temple", json::object());

  // Structural must reflect the failure.
  EXPECT_FALSE(d4_check.value("structural_ok", true));

  // Readiness check was skipped — field must say "skipped" not "ran".
  EXPECT_EQ(d4_check.value("required_rooms_check", ""),
            std::string("skipped"));

  // required_rooms_ok must NOT be present when check was skipped.
  EXPECT_FALSE(d4_check.contains("required_rooms_ok"))
      << "required_rooms_ok must be absent when readiness check is skipped";

  // D3 readiness also skipped.
  const auto& d3_check = checks.value("d3_kalyxo_castle", json::object());
  EXPECT_EQ(d3_check.value("readiness_check", ""), std::string("skipped"));
  EXPECT_FALSE(d3_check.contains("ok"))
      << "d3.ok must be absent when readiness check is skipped";
}

// ---------------------------------------------------------------------------
// Strict-readiness tests
// ---------------------------------------------------------------------------

TEST(OracleSmokeCheckTest, StrictReadinessFailsOnBlankRom) {
  // With --strict-readiness, D4/D3 rooms lacking collision data fail overall.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--strict-readiness", "--format=json"}, &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  const auto& smoke = GetSmoke(doc);
  // ok=false AND strict_readiness=true
  EXPECT_FALSE(smoke.value("ok", true));
  EXPECT_TRUE(smoke.value("strict_readiness", false));
  // d4 structural still passes; required rooms fail
  const auto& d4 =
      smoke.value("checks", json::object())
          .value("d4_zora_temple", json::object());
  EXPECT_TRUE(d4.value("structural_ok", false));
  EXPECT_FALSE(d4.value("required_rooms_ok", true));
}

TEST(OracleSmokeCheckTest, StrictReadinessPassesWhenAllRoomsHaveCollision) {
  // Inject collision for D4 rooms 0x25, 0x27 and D3 room 0x32 so that
  // --strict-readiness passes.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  ASSERT_TRUE(InjectCollisionTile(&rom, 0x25, 100).ok());
  ASSERT_TRUE(InjectCollisionTile(&rom, 0x27, 200).ok());
  ASSERT_TRUE(InjectCollisionTile(&rom, 0x32, 300).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--strict-readiness", "--format=json"}, &rom, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  const auto& smoke = GetSmoke(doc);
  EXPECT_TRUE(smoke.value("ok", false));
  EXPECT_EQ(smoke.value("status", ""), "pass");
  // All required rooms should now report ok.
  const auto& checks = smoke.value("checks", json::object());
  EXPECT_TRUE(checks.value("d4_zora_temple", json::object())
                  .value("required_rooms_ok", false));
  EXPECT_TRUE(checks.value("d3_kalyxo_castle", json::object())
                  .value("ok", false));
}

// ---------------------------------------------------------------------------
// JSON field correctness tests
// ---------------------------------------------------------------------------

TEST(OracleSmokeCheckTest, JsonContainsAllRequiredCheckKeys) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--format=json"}, &rom, &out).ok());

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  const auto& smoke = GetSmoke(doc);

  // Top-level fields
  EXPECT_TRUE(smoke.contains("ok"));
  EXPECT_TRUE(smoke.contains("status"));
  EXPECT_TRUE(smoke.contains("strict_readiness"));
  EXPECT_TRUE(smoke.contains("checks"));

  // Per-subsystem check keys
  const auto& checks = smoke.at("checks");
  EXPECT_TRUE(checks.contains("d4_zora_temple"));
  EXPECT_TRUE(checks.contains("d6_goron_mines"));
  EXPECT_TRUE(checks.contains("d3_kalyxo_castle"));

  // D4 must have both structural and readiness fields
  EXPECT_TRUE(checks.at("d4_zora_temple").contains("structural_ok"));
  EXPECT_TRUE(checks.at("d4_zora_temple").contains("required_rooms_ok"));

  // D6 and D3 must have ok field
  EXPECT_TRUE(checks.at("d6_goron_mines").contains("ok"));
  EXPECT_TRUE(checks.at("d3_kalyxo_castle").contains("ok"));
}

TEST(OracleSmokeCheckTest, DefaultModeD4StructuralOkTrueOnFullRom) {
  // Correctness: full ROM → structural_ok=true, required_rooms_check="ran",
  // required_rooms_ok=false (blank ROM = no collision authored).
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--format=json"}, &rom, &out).ok());

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  const auto& d4 = GetSmoke(doc)
                       .value("checks", json::object())
                       .value("d4_zora_temple", json::object());
  EXPECT_TRUE(d4.value("structural_ok", false));
  // Readiness check ran (ROM is expanded).
  EXPECT_EQ(d4.value("required_rooms_check", ""), std::string("ran"));
  // Rooms 0x25/0x27 lack collision on a blank ROM.
  EXPECT_FALSE(d4.value("required_rooms_ok", true));

  // D3 also ran and is false on blank ROM.
  const auto& d3 = GetSmoke(doc)
                       .value("checks", json::object())
                       .value("d3_kalyxo_castle", json::object());
  EXPECT_EQ(d3.value("readiness_check", ""), std::string("ran"));
  EXPECT_FALSE(d3.value("ok", true));
}

// ---------------------------------------------------------------------------
// --report path tests
// ---------------------------------------------------------------------------

TEST(OracleSmokeCheckTest, ReportWriteFailsOnUnwritablePathWithEmptyStdout) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--report", "/nonexistent_yaze_dir/smoke.json", "--format=json"},
      &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied);
  // stdout must be empty — probe fires in ValidateArgs, before formatter opens.
  EXPECT_TRUE(out.empty())
      << "Expected empty stdout on PermissionDenied: " << out;
}

TEST(OracleSmokeCheckTest, ReportWriteSucceedsAndContainsAllCheckKeys) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  const auto report_path =
      (std::filesystem::temp_directory_path() /
       "yaze_oracle_smoke_check_report.json").string();

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  ASSERT_TRUE(
      handler.Run({"--report", report_path, "--format=json"}, &rom, &out).ok());
  ASSERT_TRUE(std::filesystem::exists(report_path));

  std::ifstream report_file(report_path);
  const json report = json::parse(report_file, nullptr, false);
  ASSERT_FALSE(report.is_discarded());

  // Report file uses flat nlohmann::json (not nested by formatter).
  EXPECT_TRUE(report.contains("ok"));
  EXPECT_TRUE(report.contains("status"));
  EXPECT_TRUE(report.contains("checks"));
  EXPECT_TRUE(report.at("checks").contains("d4_zora_temple"));
  EXPECT_TRUE(report.at("checks").contains("d6_goron_mines"));
  EXPECT_TRUE(report.at("checks").contains("d3_kalyxo_castle"));

  std::filesystem::remove(report_path);
}

// ---------------------------------------------------------------------------
// Failure message accuracy tests
// ---------------------------------------------------------------------------

TEST(OracleSmokeCheckTest, StructuralFailureMessageSaysStructural) {
  // A small ROM fails structurally. Even if --strict-readiness is set, the
  // error message must say "(structural)", not "(strict-readiness)".
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kSmallRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--strict-readiness", "--format=json"}, &rom, &out);
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()), ::testing::HasSubstr("structural"))
      << "Expected '(structural)' in error message: " << status.message();
  EXPECT_THAT(std::string(status.message()),
              ::testing::Not(::testing::HasSubstr("strict-readiness")))
      << "Structural failure must not be attributed to strict-readiness";
}

TEST(OracleSmokeCheckTest, StrictReadinessFailureMessageSaysStrictReadiness) {
  // A full expanded ROM passes structural checks. With --strict-readiness, the
  // failure is readiness-only and the message must say "(strict-readiness)".
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--strict-readiness", "--format=json"}, &rom, &out);
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()),
              ::testing::HasSubstr("strict-readiness"))
      << "Readiness-only failure must say '(strict-readiness)': "
      << status.message();
  EXPECT_THAT(std::string(status.message()),
              ::testing::Not(::testing::HasSubstr("(structural)")))
      << "Readiness-only failure must not be attributed to structural";
}

// ---------------------------------------------------------------------------
// --min-d6-track-rooms tests
// ---------------------------------------------------------------------------

TEST(OracleSmokeCheckTest, MinD6TrackRoomsZeroPreservesDefaultBehavior) {
  // min=0 (default) — blank ROM has no track objects but still passes D6 gate.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--min-d6-track-rooms=0", "--format=json"}, &rom, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  const auto& d6 = GetSmoke(doc)
                       .value("checks", json::object())
                       .value("d6_goron_mines", json::object());
  EXPECT_EQ(d6.value("track_rooms_found", -1), 0);
  EXPECT_EQ(d6.value("min_track_rooms", -1), 0);
  EXPECT_TRUE(d6.value("meets_min_track_rooms", false));
}

TEST(OracleSmokeCheckTest, MinD6TrackRoomsFailsWhenThresholdNotMet) {
  // Blank ROM has no track objects — threshold of 1 must cause failure.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--min-d6-track-rooms=1", "--format=json"}, &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  const auto& smoke = GetSmoke(doc);
  EXPECT_FALSE(smoke.value("ok", true));

  const auto& d6 = smoke.value("checks", json::object())
                       .value("d6_goron_mines", json::object());
  EXPECT_EQ(d6.value("track_rooms_found", -1), 0);
  EXPECT_EQ(d6.value("min_track_rooms", -1), 1);
  EXPECT_FALSE(d6.value("meets_min_track_rooms", true));
}

TEST(OracleSmokeCheckTest,
     MinD6TrackRoomsThresholdFailureAttributedAsStructural) {
  // A threshold miss must produce "(structural)" in the error message, not
  // "(strict-readiness)" — the D6 gate is a structural concern.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--min-d6-track-rooms=999", "--format=json"}, &rom, &out);
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()),
              ::testing::HasSubstr("structural"))
      << "D6 threshold miss must be attributed to structural: "
      << status.message();
}

TEST(OracleSmokeCheckTest, MinD6TrackRoomsNegativeIsRejectedInValidateArgs) {
  // ValidateArgs must reject negative values before the formatter opens.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::OracleSmokeCheckCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--min-d6-track-rooms=-1", "--format=json"}, &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  // stdout must be empty — error fires in ValidateArgs before formatter opens.
  EXPECT_TRUE(out.empty())
      << "Expected empty stdout on InvalidArgument: " << out;
}

}  // namespace
}  // namespace yaze::cli
