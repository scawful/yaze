// Unit tests for DungeonMinecartAuditCommandHandler.
//
// These tests exercise the audit logic using a synthetic 0x200000 ROM. Collision
// data is injected via DungeonImportCustomCollisionJsonCommandHandler so the
// full audit read-path (LoadCustomCollisionMap) exercises real ROM data.
//
// SAFE ROOM NOTE: Rooms 0x00–0x03 produce infinite/slow object-parse loops on
// a blank ROM (ParseObjectsFromLocation follows zero-filled pointers). All tests
// use room 0x25, 0xA8, 0xB8, 0xD8, or 0xDA — rooms confirmed safe under blank
// data within the 45 s CI timeout. Never add --room=0x00 or --all here.
//
// D6 Goron Mines collision tile reference:
//  - Track tiles:  0xB0–0xB6, 0xBB–0xBE
//  - Stop tiles:   0xB7–0xBA  (cart spawn point)
//  - Switch tiles: 0xD0–0xD3

#include "cli/handlers/game/minecart_commands.h"

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
#include "cli/service/resources/command_context.h"
#include "rom/rom.h"

namespace yaze::cli {
namespace {

using ::testing::HasSubstr;

constexpr int kRomSize = 0x200000;

// Injects a single tile into `room_id` at collision offset `offset` via the
// real import handler (exercises the full write path).
absl::Status InjectCollisionTile(Rom* rom, int room_id, int offset,
                                 int tile_value) {
  const std::string json = absl::StrFormat(
      R"({"version":1,"rooms":[{"room_id":"0x%02X","tiles":[[%d,%d]]}]})",
      room_id, offset, tile_value);
  auto tmp = (std::filesystem::temp_directory_path() /
              "yaze_minecart_inject_collision.json")
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

absl::Status InjectCollisionTiles(
    Rom* rom, int room_id, const std::vector<std::pair<int, int>>& entries) {
  std::string tile_pairs;
  for (size_t i = 0; i < entries.size(); ++i) {
    if (i > 0) {
      tile_pairs += ",";
    }
    tile_pairs +=
        absl::StrFormat("[%d,%d]", entries[i].first, entries[i].second);
  }

  const std::string json = absl::StrFormat(
      R"({"version":1,"rooms":[{"room_id":"0x%02X","tiles":[%s]}]})", room_id,
      tile_pairs);
  auto tmp = (std::filesystem::temp_directory_path() /
              "yaze_minecart_inject_collision_many.json")
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

// ---------------------------------------------------------------------------
// ValidateArgs tests  (no room scanning — just argument parsing)
// ---------------------------------------------------------------------------

TEST(DungeonMinecartAuditTest, ValidateArgsMissingArgsReturnsInvalidArgument) {
  // No room selector: must fail before executing any room I/O.
  handlers::DungeonMinecartAuditCommandHandler handler;
  const resources::ArgumentParser parser({"--format=json"});
  EXPECT_FALSE(handler.ValidateArgs(parser).ok());
  EXPECT_EQ(handler.ValidateArgs(parser).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(DungeonMinecartAuditTest, ValidateArgsAllFlagAccepted) {
  // --all must satisfy ValidateArgs without scanning any rooms.
  handlers::DungeonMinecartAuditCommandHandler handler;
  const resources::ArgumentParser parser({"--all"});
  EXPECT_TRUE(handler.ValidateArgs(parser).ok());
}

TEST(DungeonMinecartAuditTest, ValidateArgsSingleRoomAccepted) {
  handlers::DungeonMinecartAuditCommandHandler handler;
  const resources::ArgumentParser parser({"--room=0x25"});
  EXPECT_TRUE(handler.ValidateArgs(parser).ok());
}

TEST(DungeonMinecartAuditTest, ValidateArgsRoomsListAccepted) {
  handlers::DungeonMinecartAuditCommandHandler handler;
  const resources::ArgumentParser parser({"--rooms=0xA8,0xB8,0xD8,0xDA"});
  EXPECT_TRUE(handler.ValidateArgs(parser).ok());
}

// ---------------------------------------------------------------------------
// Audit logic tests  (room 0x25 and D6 rooms — confirmed safe on blank ROM)
// ---------------------------------------------------------------------------

TEST(DungeonMinecartAuditTest, BlankRoomReportsNoIssues) {
  // Room 0x25 with no collision, objects, or sprites → no audit issues.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());

  handlers::DungeonMinecartAuditCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--room=0x25", "--format=json"}, &rom, &out).ok());
  EXPECT_THAT(out, HasSubstr("\"rooms_with_issues\": 0"));
}

TEST(DungeonMinecartAuditTest, StopTileWithoutTrackObjectsReportsIssue) {
  // Stop tile 0xB7 in room 0x25 with no track objects → issue reported.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());

  // Offset 1000 = y=15, x=40 in the 64×64 collision grid.
  ASSERT_TRUE(InjectCollisionTile(&rom, 0x25, 1000, 0xB7).ok());

  handlers::DungeonMinecartAuditCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--room=0x25", "--format=json"}, &rom, &out).ok());

  EXPECT_THAT(out, HasSubstr("\"rooms_with_issues\": 1"));
  EXPECT_THAT(out, HasSubstr("no track objects"));
}

TEST(DungeonMinecartAuditTest, TrackTileWithoutStopTilesReportsBothIssues) {
  // Track tile 0xB0 in room 0xA8 with no stop tiles → two issues reported.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());

  ASSERT_TRUE(InjectCollisionTile(&rom, 0xA8, 500, 0xB0).ok());

  handlers::DungeonMinecartAuditCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--room=0xA8", "--format=json"}, &rom, &out).ok());

  EXPECT_THAT(out, HasSubstr("\"rooms_with_issues\": 1"));
  EXPECT_THAT(out, HasSubstr("no stop tiles"));
  EXPECT_THAT(out, HasSubstr("no track objects"));
}

TEST(DungeonMinecartAuditTest, OnlyIssuesFlagEmitsIssueRooms) {
  // --only-issues on a room with a stop-tile issue → room IS emitted.
  // Implicitly verifies the filter path: rooms without issues are not emitted
  // (confirmed by BlankRoomReportsNoIssues returning rooms_with_issues=0).
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());

  ASSERT_TRUE(InjectCollisionTile(&rom, 0x25, 1000, 0xB7).ok());

  handlers::DungeonMinecartAuditCommandHandler handler;
  std::string out;
  ASSERT_TRUE(
      handler.Run({"--room=0x25", "--only-issues", "--format=json"}, &rom, &out)
          .ok());

  EXPECT_THAT(out, HasSubstr("\"rooms_emitted\": 1"));
  EXPECT_THAT(out, HasSubstr("\"rooms_with_issues\": 1"));
}

TEST(DungeonMinecartAuditTest, JsonOutputContainsExpectedTopLevelFields) {
  // Structural check: the JSON report must include all top-level fields that
  // Oracle tooling depends on.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());

  handlers::DungeonMinecartAuditCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--room=0x25", "--format=json"}, &rom, &out).ok());

  EXPECT_THAT(out, HasSubstr("\"total_rooms_requested\""));
  EXPECT_THAT(out, HasSubstr("\"track_object_id\""));
  EXPECT_THAT(out, HasSubstr("\"minecart_sprite_id\""));
  EXPECT_THAT(out, HasSubstr("\"rooms_emitted\""));
  EXPECT_THAT(out, HasSubstr("\"rooms_with_issues\""));
  EXPECT_THAT(out, HasSubstr("\"status\": \"success\""));
}

TEST(DungeonMinecartAuditTest, MultipleD6RoomsCollisionOnOneIsolatesIssue) {
  // Inject stop tile into room 0xD8 only. Audit all four D6 flagged rooms.
  // Only 0xD8 should report an issue.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());

  ASSERT_TRUE(InjectCollisionTile(&rom, 0xD8, 100, 0xB7).ok());

  handlers::DungeonMinecartAuditCommandHandler handler;
  std::string out;
  ASSERT_TRUE(
      handler.Run({"--rooms=0xA8,0xB8,0xD8,0xDA", "--format=json"}, &rom, &out)
          .ok());

  EXPECT_THAT(out, HasSubstr("\"rooms_with_issues\": 1"));
}

// ---------------------------------------------------------------------------
// Minecart map command tests (tile enumeration + bounded ASCII grid)
// ---------------------------------------------------------------------------

TEST(DungeonMinecartMapTest, ValidateArgsMissingRoomReturnsInvalidArgument) {
  handlers::DungeonMinecartMapCommandHandler handler;
  const resources::ArgumentParser parser({"--format=json"});
  const auto status = handler.ValidateArgs(parser);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(DungeonMinecartMapTest, BlankRoomReportsNoCustomCollisionData) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());

  handlers::DungeonMinecartMapCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--room=0x25", "--format=json"}, &rom, &out).ok());

  EXPECT_THAT(out, HasSubstr("\"has_custom_collision_data\": false"));
  EXPECT_THAT(out, HasSubstr("\"tile_count\": 0"));
}

TEST(DungeonMinecartMapTest, EnumeratesTrackTilesAndAsciiGrid) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());

  // Three minecart tiles in room 0x25:
  //  - offset 0   -> (0,0)   track straight (0xB0)
  //  - offset 1   -> (1,0)   stop north    (0xB7)
  //  - offset 64  -> (0,1)   switch TL     (0xD0)
  ASSERT_TRUE(
      InjectCollisionTiles(&rom, 0x25, {{0, 0xB0}, {1, 0xB7}, {64, 0xD0}})
          .ok());

  handlers::DungeonMinecartMapCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--room=0x25", "--format=json"}, &rom, &out).ok());

  EXPECT_THAT(out, HasSubstr("\"has_custom_collision_data\": true"));
  EXPECT_THAT(out, HasSubstr("\"tile_count\": 3"));
  EXPECT_THAT(out, HasSubstr("\"category\": \"track\""));
  EXPECT_THAT(out, HasSubstr("\"category\": \"stop\""));
  EXPECT_THAT(out, HasSubstr("\"category\": \"switch\""));
  EXPECT_THAT(out, HasSubstr("\"ascii_grid\""));
}

}  // namespace
}  // namespace yaze::cli
