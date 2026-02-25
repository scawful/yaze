// Oracle-of-Secrets workflow integration tests.
//
// Covers three subsystems with VALUE-LEVEL correctness assertions (beyond JSON
// shape) against the real Oracle expanded ROM (oos168.sfc).
//
// ROM fixture discovery (in priority order):
//   1. YAZE_TEST_ROM_OOS env var
//   2. YAZE_TEST_ROM_EXPANDED env var
//   3. Auto-search: roms/oos168.sfc, Roms/oos168.sfc, ../roms/oos168.sfc
//
// If the ROM is absent, all tests skip cleanly via GTEST_SKIP().
// No full-room scans — focused room subsets only.
//
// Subsystems:
//   D4 Zora Temple  — water-fill table invariants (mask uniqueness, power-of-2)
//   D6 Goron Mines  — track rail objects present in known rooms
//   D3 Kalyxo Castle — preflight check ran (not skipped) on expanded ROM

#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "cli/handlers/game/minecart_commands.h"
#include "cli/handlers/game/oracle_menu_commands.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::test {
namespace {

using ::testing::HasSubstr;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// ROM discovery (same logic as oracle_smoke.sh)
// ---------------------------------------------------------------------------

std::string FindOracleRom() {
  for (const char* env_var :
       {"YAZE_TEST_ROM_OOS", "YAZE_TEST_ROM_EXPANDED",
        "YAZE_TEST_ROM_EXPANDED_PATH"}) {
    if (const char* path = std::getenv(env_var)) {
      if (std::filesystem::exists(path)) {
        return path;
      }
    }
  }
  static const std::vector<std::string> kDirs = {
      ".", "roms", "Roms", "../roms", "../../roms"};
  static const std::vector<std::string> kNames = {
      "oos168.sfc", "oos168x.sfc", "oracle_of_secrets.sfc"};
  for (const auto& dir : kDirs) {
    for (const auto& name : kNames) {
      std::filesystem::path path = std::filesystem::path(dir) / name;
      if (std::filesystem::exists(path)) {
        return path.string();
      }
    }
  }
  return "";
}

// ---------------------------------------------------------------------------
// Shared fixture
// ---------------------------------------------------------------------------

class OracleWorkflowTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_path_ = FindOracleRom();
    if (rom_path_.empty()) {
      GTEST_SKIP() << "Oracle ROM fixture not found. "
                      "Set YAZE_TEST_ROM_OOS to oos168.sfc path, "
                      "or place it in roms/oos168.sfc.";
    }
    ASSERT_TRUE(rom_.LoadFromFile(rom_path_).ok())
        << "Failed to load Oracle ROM from: " << rom_path_;
  }

  std::string rom_path_;
  Rom rom_;
};

// ---------------------------------------------------------------------------
// D4 Zora Temple: water-fill table invariants
//
// Correctness: the water-fill table in oos168.sfc must satisfy two invariants
// regardless of authoring state:
//   1. All SRAM bit masks are unique (no two zones share a mask bit).
//   2. Each mask is a single bit (power of 2, non-zero).
// These are structural invariants that the Oracle ASM relies on for water
// state tracking at $7EF411. A duplicate or multi-bit mask would silently
// corrupt both zones' state bits.
// ---------------------------------------------------------------------------

TEST_F(OracleWorkflowTest, D4WaterFillTableMasksAreUniqueAndSingleBit) {
  auto zones_or = zelda3::LoadWaterFillTable(&rom_);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();

  const auto& zones = *zones_or;
  // An empty table is valid in-development; still pass the test — the
  // uniqueness constraint is vacuously true for zero zones.
  if (zones.empty()) {
    return;  // nothing to check, but not a failure
  }

  std::unordered_set<uint8_t> seen_masks;
  for (const auto& zone : zones) {
    // Each mask must be non-zero.
    EXPECT_NE(zone.sram_bit_mask, 0u)
        << "Zone for room 0x" << std::hex << zone.room_id
        << " has zero SRAM mask.";

    // Each mask must be a single power-of-2 bit.
    EXPECT_EQ(zone.sram_bit_mask & (zone.sram_bit_mask - 1u), 0u)
        << "Zone for room 0x" << std::hex << zone.room_id
        << " has non-power-of-2 mask 0x" << +zone.sram_bit_mask;

    // Each mask must be unique across all zones.
    EXPECT_EQ(seen_masks.count(zone.sram_bit_mask), 0u)
        << "Duplicate SRAM mask 0x" << std::hex << +zone.sram_bit_mask
        << " (room 0x" << zone.room_id << " already seen).";
    seen_masks.insert(zone.sram_bit_mask);
  }
}

TEST_F(OracleWorkflowTest, D4PreflightConfirmsExpandedRomHasWriteSupport) {
  // dungeon-oracle-preflight with D4 rooms must report
  // required_rooms_check="ran" (not "skipped"). This confirms the ROM is
  // expanded (>= 0x130000 bytes) so the collision write-support region exists.
  cli::handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  // Ignore exit code — room data may not be authored yet.
  [[maybe_unused]] auto ignored =
      handler.Run({"--required-collision-rooms=0x25,0x27", "--format=json"},
                  &rom_, &out);

  EXPECT_THAT(out, HasSubstr("\"required_rooms_check\": \"ran\""))
      << "Expected check to run on expanded ROM, got: " << out;
  EXPECT_THAT(out, HasSubstr("\"required_rooms_checked\": 2"));
}

// ---------------------------------------------------------------------------
// D6 Goron Mines: track rail objects present in known rooms
//
// Correctness: rooms 0xA8, 0xB8, 0xD8, 0xDA are known to have rail-track
// objects (Object 0x31) placed in them during dungeon design. With
// --include-track-objects, the audit must emit at least one room with a
// non-empty track_object_subtypes array. An empty array across all four rooms
// would indicate the Oracle ROM has lost its minecart room configuration.
// ---------------------------------------------------------------------------

TEST_F(OracleWorkflowTest, D6AuditWithIncludeTrackObjectsFindsRailObjects) {
  cli::handlers::DungeonMinecartAuditCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--rooms=0xA8,0xB8,0xD8,0xDA", "--include-track-objects",
       "--format=json"},
      &rom_, &out);
  ASSERT_TRUE(status.ok()) << status.message();

  const auto doc = json::parse(out, nullptr, /*allow_exceptions=*/false);
  ASSERT_FALSE(doc.is_discarded()) << "JSON parse failed: " << out;

  // The formatter wraps Execute's output one level deep:
  //   { "Dungeon Minecart Audit": { "rooms": [...], ... } }
  // (BeginObject emits "key":{} only at indent_level > 0.)
  ASSERT_TRUE(doc.contains("Dungeon Minecart Audit"))
      << "Missing 'Dungeon Minecart Audit' key; formatter structure changed? "
      << out;
  const auto& audit = doc.at("Dungeon Minecart Audit");

  // Correctness: at least one D6 room must have track rail objects.
  // (We know all four rooms have Object 0x31 tracks in the Oracle design.)
  bool found_track_objects = false;
  for (const auto& room : audit.value("rooms", json::array())) {
    const auto& subtypes = room.value("track_object_subtypes", json::array());
    if (!subtypes.empty()) {
      found_track_objects = true;
      break;
    }
  }
  EXPECT_TRUE(found_track_objects)
      << "No D6 room returned track_object_subtypes — did the minecart room "
         "configuration get lost? Full output: " << out;
}

TEST_F(OracleWorkflowTest, D6AuditEmittedCountMatchesRequestedCount) {
  // With --include-track-objects, all four rooms must appear in the output
  // because they all have rail objects, regardless of collision/sprite state.
  cli::handlers::DungeonMinecartAuditCommandHandler handler;
  std::string out;
  ASSERT_TRUE(
      handler
          .Run({"--rooms=0xA8,0xB8,0xD8,0xDA", "--include-track-objects",
                "--format=json"},
               &rom_, &out)
          .ok());

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  ASSERT_TRUE(doc.contains("Dungeon Minecart Audit"));
  const auto& audit = doc.at("Dungeon Minecart Audit");

  EXPECT_EQ(audit.value("total_rooms_requested", -1), 4);
  // rooms_emitted must equal total_rooms_requested when --include-track-objects
  // is set and all rooms have track objects.
  EXPECT_EQ(audit.value("rooms_emitted", -1), 4)
      << "Expected 4 rooms emitted (all have track objects), got: " << out;
}

// ---------------------------------------------------------------------------
// D3 Kalyxo Castle: preflight check ran, not skipped
//
// The required-room check in oracle_rom_safety_preflight is gated on
// HasCustomCollisionWriteSupport(). This test confirms that the Oracle ROM is
// large enough for the check to run (i.e., the ROM is properly expanded).
// Whether the check PASSES depends on authoring state and is not asserted.
// ---------------------------------------------------------------------------

TEST_F(OracleWorkflowTest, D3PreflightCheckRanNotSkippedOnExpandedRom) {
  cli::handlers::DungeonOraclePreflightCommandHandler handler;
  std::string out;
  // Ignore exit code: room 0x32 may not be authored yet.
  [[maybe_unused]] auto ignored =
      handler.Run({"--required-collision-rooms=0x32", "--format=json"},
                  &rom_, &out);

  // The key correctness assertion: the check RAN (not "skipped").
  // "skipped" would mean the ROM lacks the expanded collision write region,
  // which would indicate a broken/truncated Oracle ROM.
  EXPECT_THAT(out, HasSubstr("\"required_rooms_check\": \"ran\""))
      << "Check was skipped — is the Oracle ROM properly expanded? Output: "
      << out;
  EXPECT_THAT(out, HasSubstr("\"required_rooms_checked\": 1"));
}

}  // namespace
}  // namespace yaze::test
