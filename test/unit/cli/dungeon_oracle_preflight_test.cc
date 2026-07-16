// Unit tests for DungeonOraclePreflightCommandHandler.
//
// All tests use synthetic ROMs (0x200000 blank or 0x100000) so no fixture
// is required. Integration tests against real Oracle ROMs live in
// test/integration/zelda3/minecart_audit_integration_test.cc.

#include "cli/handlers/game/oracle_menu_commands.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "cli/handlers/game/dungeon_collision_commands.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::cli {
namespace {

using ::testing::HasSubstr;
using json = nlohmann::json;

constexpr int kSmallRomSize = 0x100000;  // no water-fill region
constexpr int kFullRomSize = 0x200000;   // water-fill region present

// Injects a stop tile into a room via the import handler so the
// required-room check can succeed on a blank ROM.
absl::Status InjectCollisionTile(Rom* rom, int room_id, int offset,
                                 int tile_value) {
  const std::string json = absl::StrFormat(
      R"({"version":1,"rooms":[{"room_id":"0x%02X","tiles":[[%d,%d]]}]})",
      room_id, offset, tile_value);
  auto tmp = (std::filesystem::temp_directory_path() /
              "yaze_oracle_preflight_inject.json")
                 .string();
  {
    std::ofstream f(tmp, std::ios::out | std::ios::binary | std::ios::trunc);
    f << json;
  }
  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--in", tmp, "--format=json"}, rom, &out);
  std::filesystem::remove(tmp);
  return status;
}

absl::Status WriteD4WaterFillTable(Rom* rom, bool include_room_27) {
  std::vector<zelda3::WaterFillZoneEntry> zones = {
      {.room_id = 0x25, .sram_bit_mask = 0x02, .fill_offsets = {0x0B45}}};
  if (include_room_27) {
    zones.push_back(
        {.room_id = 0x27, .sram_bit_mask = 0x01, .fill_offsets = {0x03EA}});
  }
  return zelda3::WriteWaterFillTable(rom, zones);
}

const json& GetPreflightReport(const json& document) {
  return document.at("Dungeon Oracle Preflight");
}

// ---------------------------------------------------------------------------
// Basic pass/fail cases
// ---------------------------------------------------------------------------

TEST(DungeonOraclePreflightTest, FullRomPassesWithDefaultOptions) {
  // A 0x200000 blank ROM satisfies all default checks: water-fill region
  // present, zero-zone table valid, collision maps load cleanly (null ptrs).
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--format=json"}, &rom, &out);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(out, HasSubstr("\"ok\": true"));
  EXPECT_THAT(out, HasSubstr("\"status\": \"pass\""));
  EXPECT_THAT(out, HasSubstr("\"error_count\": 0"));
}

TEST(DungeonOraclePreflightTest, SmallRomFailsWaterFillRegionMissing) {
  // ROM smaller than 0x130000 lacks the water-fill reserved region.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kSmallRomSize, 0)).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--format=json"}, &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(out, HasSubstr("\"ok\": false"));
  EXPECT_THAT(out, HasSubstr("\"status\": \"fail\""));
  EXPECT_THAT(out, HasSubstr("ORACLE_WATER_FILL_REGION_MISSING"));
  EXPECT_THAT(out, HasSubstr("\"water_fill_region_ok\": false"));
}

// ---------------------------------------------------------------------------
// Per-check boolean fields
// ---------------------------------------------------------------------------

TEST(DungeonOraclePreflightTest, JsonOutputContainsPerCheckBooleans) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--format=json"}, &rom, &out).ok());

  EXPECT_THAT(out, HasSubstr("\"water_fill_region_ok\""));
  EXPECT_THAT(out, HasSubstr("\"water_fill_table_ok\""));
  EXPECT_THAT(out, HasSubstr("\"custom_collision_maps_ok\""));
  EXPECT_THAT(out, HasSubstr("\"errors\""));
}

TEST(DungeonOraclePreflightTest, SkipCollisionMapsOmitsMapCheck) {
  // --skip-collision-maps should still emit custom_collision_maps_ok: true
  // (because no map errors were generated) and succeed.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--skip-collision-maps", "--format=json"}, &rom, &out);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(out, HasSubstr("\"custom_collision_maps_ok\": true"));
}

// ---------------------------------------------------------------------------
// Required-room checks
// ---------------------------------------------------------------------------

TEST(DungeonOraclePreflightTest, RequiredCollisionRoomsFailsWhenRoomHasNoData) {
  // Room 0x32 (D3 prison) has no authored collision on a blank ROM.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--required-collision-rooms=0x32", "--format=json"}, &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(out, HasSubstr("\"required_rooms_ok\": false"));
  EXPECT_THAT(out, HasSubstr("ORACLE_REQUIRED_ROOM_MISSING_COLLISION"));
  EXPECT_THAT(out, HasSubstr("\"required_rooms_checked\": 1"));
  EXPECT_THAT(out, HasSubstr("\"required_rooms_check\": \"ran\""));
}

TEST(DungeonOraclePreflightTest, RequiredCollisionRoomsPassesWhenDataPresent) {
  // Inject collision for room 0x25, then run preflight requiring it.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  // Offset 500 = y=7, x=52 in the 64x64 grid.
  ASSERT_TRUE(InjectCollisionTile(&rom, 0x25, 500, 0xB7).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--required-collision-rooms=0x25", "--format=json"}, &rom, &out);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(out, HasSubstr("\"required_rooms_ok\": true"));
  EXPECT_THAT(out, HasSubstr("\"required_rooms_checked\": 1"));
  EXPECT_THAT(out, HasSubstr("\"required_rooms_check\": \"ran\""));
}

TEST(DungeonOraclePreflightTest, D4WaterRoomsRequiredCheck) {
  // Simulate D4 water profile validation: rooms 0x25 and 0x27 both required.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  // Only inject 0x25 — 0x27 is missing. Preflight should fail.
  ASSERT_TRUE(InjectCollisionTile(&rom, 0x25, 500, 0xB7).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--required-collision-rooms=0x25,0x27", "--format=json"}, &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(out, HasSubstr("\"required_rooms_ok\": false"));
  EXPECT_THAT(out, HasSubstr("\"required_rooms_checked\": 2"));
  // Full-size ROM: check ran, so the field must say "ran".
  EXPECT_THAT(out, HasSubstr("\"required_rooms_check\": \"ran\""));
}

TEST(DungeonOraclePreflightTest, InvalidRoomIdInRequiredListErrors) {
  // Hex parse error in the room list should return InvalidArgument.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--required-collision-rooms=not_a_hex", "--format=json"}, &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(DungeonOraclePreflightTest,
     RequiredWaterFillRoomsFailsForStructurallyValidOneRoomTable) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());
  ASSERT_TRUE(WriteD4WaterFillTable(&rom, /*include_room_27=*/false).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--required-water-fill-rooms=0x25,0x27", "--format=json"}, &rom, &out);
  EXPECT_FALSE(status.ok());

  const auto document = json::parse(out);
  const auto& report = GetPreflightReport(document);
  EXPECT_FALSE(report.at("ok").get<bool>());
  EXPECT_TRUE(report.at("water_fill_table_ok").get<bool>());
  EXPECT_EQ(report.at("required_water_fill_rooms_checked").get<int>(), 2);
  EXPECT_FALSE(report.at("required_water_fill_rooms_ok").get<bool>());
  ASSERT_EQ(report.at("errors").size(), 1u) << report.dump(2);
  EXPECT_EQ(report.at("errors").at(0).at("code").get<std::string>(),
            "ORACLE_REQUIRED_WATER_FILL_ROOM_MISSING");
  EXPECT_EQ(report.at("errors").at(0).at("room_id").get<std::string>(), "0x27");
}

TEST(DungeonOraclePreflightTest,
     RequiredWaterFillRoomsPassForTrackedD4TwoRoomTable) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());
  ASSERT_TRUE(WriteD4WaterFillTable(&rom, /*include_room_27=*/true).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--required-water-fill-rooms=0x25,0x27", "--format=json"}, &rom, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto document = json::parse(out);
  const auto& report = GetPreflightReport(document);
  EXPECT_TRUE(report.at("ok").get<bool>());
  EXPECT_TRUE(report.at("water_fill_table_ok").get<bool>());
  EXPECT_EQ(report.at("required_water_fill_rooms_checked").get<int>(), 2);
  EXPECT_TRUE(report.at("required_water_fill_rooms_ok").get<bool>());
  EXPECT_TRUE(report.at("errors").empty());

  const auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  ASSERT_EQ(zones_or->size(), 2u);
  EXPECT_EQ(zones_or->at(0).room_id, 0x25);
  EXPECT_EQ(zones_or->at(0).sram_bit_mask, 0x02);
  EXPECT_EQ(zones_or->at(1).room_id, 0x27);
  EXPECT_EQ(zones_or->at(1).sram_bit_mask, 0x01);
}

TEST(DungeonOraclePreflightTest, InvalidRequiredWaterFillRoomErrors) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--required-water-fill-rooms=not_a_hex", "--format=json"}, &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(DungeonOraclePreflightTest,
     RequiredWaterFillRoomAboveByteRangeIsInvalidArgument) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--required-water-fill-rooms=0x100",
                                   "--skip-collision-maps", "--format=json"},
                                  &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);

  const auto document = json::parse(out);
  const auto& report = GetPreflightReport(document);
  EXPECT_FALSE(report.at("ok").get<bool>());
  ASSERT_EQ(report.at("errors").size(), 1u) << report.dump(2);
  EXPECT_EQ(report.at("errors").at(0).at("code").get<std::string>(),
            "ORACLE_REQUIRED_WATER_FILL_ROOM_OUT_OF_RANGE");
  EXPECT_EQ(report.at("errors").at(0).at("status").get<std::string>(),
            "INVALID_ARGUMENT");
  EXPECT_EQ(report.at("errors").at(0).at("room_id").get<std::string>(),
            "0x100");
}

TEST(DungeonOraclePreflightTest,
     StructurallyValidWaterFillTableStillPassesWithoutMembershipOptIn) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());
  ASSERT_TRUE(WriteD4WaterFillTable(&rom, /*include_room_27=*/false).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--skip-collision-maps", "--format=json"}, &rom, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto document = json::parse(out);
  const auto& report = GetPreflightReport(document);
  EXPECT_TRUE(report.at("ok").get<bool>());
  EXPECT_TRUE(report.at("water_fill_table_ok").get<bool>());
  EXPECT_FALSE(report.contains("required_water_fill_rooms_checked"));
  EXPECT_FALSE(report.contains("required_water_fill_rooms_ok"));
}

// ---------------------------------------------------------------------------
// required_rooms_check: "skipped" when ROM lacks write-support region
// ---------------------------------------------------------------------------

TEST(DungeonOraclePreflightTest, RequiredRoomsCheckReportsSkippedOnSmallRom) {
  // A small ROM (0x100000) lacks HasCustomCollisionWriteSupport.
  // The preflight library skips the required-room check; the command must
  // report "required_rooms_check": "skipped" rather than silently claiming
  // required_rooms_ok: true.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kSmallRomSize, 0)).ok());

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  // Ignore the status — small ROM fails water-fill region check first.
  // We only care about the shape of the output, not the exit code.
  [[maybe_unused]] auto ignored_status = handler.Run(
      {"--required-collision-rooms=0x25", "--format=json"}, &rom, &out);

  EXPECT_THAT(out, HasSubstr("\"required_rooms_check\": \"skipped\""));
  // required_rooms_ok must NOT be present when the check was skipped.
  EXPECT_THAT(out, ::testing::Not(HasSubstr("\"required_rooms_ok\"")));
}

// ---------------------------------------------------------------------------
// --report write path: fail loudly on unwritable path
// ---------------------------------------------------------------------------

TEST(DungeonOraclePreflightTest, ReportWriteFailsOnUnwritablePath) {
  // An unwritable --report path must:
  //   (a) return PermissionDenied
  //   (b) produce EMPTY stdout (not "{}" — ValidateArgs runs before the
  //       formatter, so no wrapper object is emitted on this failure path)
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  const std::string bad_path = "/nonexistent_yaze_test_dir_zzzz/report.json";

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--report", bad_path, "--format=json"}, &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied)
      << status.message();
  // stdout must be empty — the probe fires in ValidateArgs() before the
  // formatter wrapper object is opened by CommandHandler::Run().
  EXPECT_TRUE(out.empty()) << "Expected empty stdout on PermissionDenied, got: "
                           << out;
}

TEST(DungeonOraclePreflightTest, InvalidRequiredRoomsDoesNotClobberReportFile) {
  // Validation probe must not truncate an existing report file if Execute()
  // later fails argument parsing (invalid required-collision-rooms token).
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  const auto report_path = (std::filesystem::temp_directory_path() /
                            "yaze_oracle_preflight_no_clobber_report.json")
                               .string();
  const std::string sentinel = "SENTINEL_REPORT_CONTENT\n";
  {
    std::ofstream f(report_path,
                    std::ios::out | std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(f.is_open());
    f << sentinel;
    ASSERT_TRUE(f.good());
  }

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--report", report_path,
                   "--required-collision-rooms=not_a_hex", "--format=json"},
                  &rom, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument)
      << status.message();

  std::ifstream f(report_path, std::ios::in | std::ios::binary);
  const std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
  EXPECT_EQ(content, sentinel);
  f.close();

  std::error_code cleanup_ec;
  std::filesystem::remove(report_path, cleanup_ec);
}

TEST(DungeonOraclePreflightTest, ReportWriteSucceedsAndContainsFullJson) {
  // A valid temp path must produce a well-formed JSON file with all fields.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kFullRomSize, 0)).ok());

  const auto report_path = (std::filesystem::temp_directory_path() /
                            "yaze_oracle_preflight_report.json")
                               .string();

  handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  const auto status =
      handler.Run({"--report", report_path, "--format=json"}, &rom, &out);
  ASSERT_TRUE(status.ok()) << status.message();

  // Report file must exist and contain the expected keys.
  ASSERT_TRUE(std::filesystem::exists(report_path));
  std::ifstream f(report_path);
  const std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
  EXPECT_THAT(content, HasSubstr("\"ok\""));
  EXPECT_THAT(content, HasSubstr("\"status\""));
  EXPECT_THAT(content, HasSubstr("\"errors\""));
  EXPECT_THAT(content, HasSubstr("\"water_fill_region_ok\""));
  f.close();

  std::error_code cleanup_ec;
  std::filesystem::remove(report_path, cleanup_ec);
}

}  // namespace
}  // namespace yaze::cli
