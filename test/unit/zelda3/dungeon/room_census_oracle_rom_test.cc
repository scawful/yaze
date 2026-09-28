// Room census on real ROMs. Skipped unless the ROM env vars are set.
//
// YAZE_TEST_ROM_OOS: a COPY of the Oracle of Secrets base ROM (oos168.sfc).
// The census only reads the ROM. Ownership comes from the fixture copy of
// Oracle's dungeons.json (test/fixtures/room_census/oracle_dungeon_owners.json)
// so the result does not depend on the Oracle checkout.
//
// Expected lists: the Oracle notes (Docs/oracle.org "Room census", 2026-09-26)
// count 13 free and 6 reclaimable rooms; the census agent's per-room table is
// test/fixtures/room_census/oracle_census_2026_09_26.csv.

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rom/rom.h"
#include "zelda3/dungeon/room_census.h"
#include "zelda3/dungeon/room_census_vanilla_fingerprints.h"

namespace yaze::zelda3 {
namespace {

namespace fs = std::filesystem;

fs::path FixtureDir() {
  return fs::path(YAZE_TEST_FIXTURE_DIR) / "room_census";
}

std::vector<RoomCensusOwnerGroup> LoadOwnerFixture() {
  std::ifstream file(FixtureDir() / "oracle_dungeon_owners.json");
  const auto json = nlohmann::json::parse(file);
  std::vector<RoomCensusOwnerGroup> groups;
  for (const auto& dungeon : json.at("dungeons")) {
    RoomCensusOwnerGroup group;
    group.id = dungeon.at("id");
    group.name = dungeon.at("name");
    for (const auto& room : dungeon.at("rooms")) {
      group.rooms.push_back(
          std::stoi(room.at("id").get<std::string>(), nullptr, 16));
    }
    groups.push_back(std::move(group));
  }
  return groups;
}

std::set<int> RoomsWith(const RoomCensus& census, RoomCensusStatus status) {
  std::set<int> out;
  for (const auto& entry : census.rooms) {
    if (entry.status == status) {
      out.insert(entry.room_id);
    }
  }
  return out;
}

std::string Describe(const RoomCensus& census, int room) {
  std::ostringstream out;
  out << "room 0x" << std::hex << room << ": "
      << RoomCensusStatusName(census.rooms[room].status);
  for (const auto& reason : census.rooms[room].reasons) {
    out << " | " << reason;
  }
  return out.str();
}

TEST(RoomCensusOracleRomTest, MatchesOracleNotesWithExplainedDifferences) {
  const char* rom_path = std::getenv("YAZE_TEST_ROM_OOS");
  if (rom_path == nullptr || !fs::exists(rom_path)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to a copy of oos168.sfc";
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path).ok());
  auto input_or = CollectRoomCensusInput(&rom);
  ASSERT_TRUE(input_or.ok()) << input_or.status();
  RoomCensusInput input = std::move(input_or).value();
  input.project_owners = LoadOwnerFixture();
  input.warp_tag_ids.insert(0x3A);  // Oracle WarpTag (hack_manifest room_tags)
  const RoomCensus census = BuildRoomCensus(input);

  EXPECT_TRUE(census.expanded_entrance_tables);
  EXPECT_TRUE(census.has_vanilla_baseline);

  // Reclaimable: the notes' six minus two that the game still reaches.
  // - 0x01: vanilla teleport doors (type 0x46) in the Hyrule Castle dream
  //   rooms 0x50 (east wall, stair slot 4) and 0x52 (west wall, stair slot
  //   3) lead to it; both header slots hold 0x01, as in vanilla. The notes
  //   only saw its one-sided grid doors.
  // - 0x30: drawn on the pause map of dungeon ID 0x08, whose other rooms
  //   are reached. Pause-map evidence no longer depends on the project.
  EXPECT_EQ(RoomsWith(census, RoomCensusStatus::kReclaimable),
            (std::set<int>{0x10, 0xA7, 0x106, 0x127}));
  bool teleport_from_50 = false;
  for (const auto& ref : census.rooms[0x01].references) {
    teleport_from_50 |= ref.kind == RoomReferenceKind::kTeleportDoor &&
                        ref.from_room == 0x50 && ref.strong;
  }
  EXPECT_TRUE(teleport_from_50) << Describe(census, 0x01);
  EXPECT_TRUE(census.rooms[0x01].reached);
  EXPECT_EQ(census.rooms[0x30].status, RoomCensusStatus::kInUse);
  EXPECT_THAT(census.rooms[0x30].reasons,
              ::testing::Contains(
                  ::testing::HasSubstr("on the pause map of dungeon ID 0x08")));

  // Free: the notes' 13 plus 0x31.
  // 0x31 (Dream 3 placeholder, 0 objects) is referenced only by
  // Sprites/NPCs/maple.asm Link_WarpToRoom, which is ASM, not ROM data the
  // census reads; the notes counted it as in use from an ASM scan.
  const std::set<int> notes_free = {0x02, 0x1F, 0x20, 0x93, 0x94, 0x95, 0x96,
                                    0xA0, 0xA6, 0xAB, 0xB0, 0xE9, 0xF7};
  std::set<int> expected_free = notes_free;
  expected_free.insert(0x31);
  const auto free_rooms = RoomsWith(census, RoomCensusStatus::kFree);
  EXPECT_EQ(free_rooms, expected_free);
  for (int room : free_rooms) {
    EXPECT_TRUE(census.rooms[room].empty) << Describe(census, room);
  }
  EXPECT_EQ(census.free_count, 14);
  EXPECT_EQ(census.reclaimable_count, 4);

  // Largest free block, as seen in the rendered census PNG.
  ASSERT_FALSE(census.free_clusters.empty());
  EXPECT_EQ(census.free_clusters.front().summary,
            "Row 9: 0x93-0x96 + 0xA0/0xA6/0xB0, 7 rooms");

  // Mayor's House stair: $119 slot 1 byte $1D reaches $11D (not $1D).
  EXPECT_TRUE(census.rooms[0x11D].reached) << Describe(census, 0x11D);
  bool stair_from_mayor = false;
  for (const auto& ref : census.rooms[0x11D].references) {
    stair_from_mayor |= ref.kind == RoomReferenceKind::kStair &&
                        ref.from_room == 0x119 && ref.strong;
  }
  EXPECT_TRUE(stair_from_mayor);

  // The game reads ZScream's bank $0F entrance table: entrance 0x29 leads to
  // 0x90 (old $02:C813 table: 0x60), so the 0x90-0x92 block is in use, and
  // entrance 0x68 leads to 0x0F (old: 0x11A), which leaves 0x11A an orphan.
  EXPECT_TRUE(census.rooms[0x90].reached) << Describe(census, 0x90);
  EXPECT_TRUE(census.rooms[0x0F].reached);
  EXPECT_EQ(census.rooms[0x11A].status, RoomCensusStatus::kInUse);
  EXPECT_TRUE(census.rooms[0x11A].orphan) << Describe(census, 0x11A);

  // D5 boss room 0xAC: bombable floor in 0xAD uses its holewarp.
  EXPECT_TRUE(census.rooms[0xAC].reached) << Describe(census, 0xAC);

  // Fortress of Secrets (D8) owns rooms in two separate grid regions.
  const auto& fos = census.owners[census.rooms[0x0C].owner_index];
  EXPECT_EQ(fos.id, "FOS");
  EXPECT_EQ(census.owners[census.rooms[0x9D].owner_index].id, "FOS");
}

TEST(RoomCensusOracleRomTest, CensusAgentUnusedRoomsAreFreeOrReclaimable) {
  const char* rom_path = std::getenv("YAZE_TEST_ROM_OOS");
  if (rom_path == nullptr || !fs::exists(rom_path)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to a copy of oos168.sfc";
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path).ok());
  auto input_or = CollectRoomCensusInput(&rom);
  ASSERT_TRUE(input_or.ok());
  input_or->project_owners = LoadOwnerFixture();
  input_or->warp_tag_ids.insert(0x3A);
  const RoomCensus census = BuildRoomCensus(*input_or);

  // Every room the census agent marked "N" (not used) is free or
  // reclaimable here, except 0x01 (teleport doors from 0x50/0x52, see
  // above). Its "Y" rooms are in use, except 0x11A (see above).
  std::ifstream csv(FixtureDir() / "oracle_census_2026_09_26.csv");
  std::string line;
  std::getline(csv, line);  // header
  int checked = 0;
  while (std::getline(csv, line)) {
    const std::string room_hex = line.substr(0, line.find(','));
    const std::string used = line.substr(line.rfind(',') + 1);
    const int room = std::stoi(room_hex, nullptr, 16);
    const auto status = census.rooms[room].status;
    if (used == "N" && room != 0x01) {
      EXPECT_NE(status, RoomCensusStatus::kInUse) << Describe(census, room);
    } else if (used == "Y" && room != 0x11A && room != 0x31) {
      EXPECT_EQ(status, RoomCensusStatus::kInUse) << Describe(census, room);
    }
    ++checked;
  }
  EXPECT_EQ(checked, kRoomCensusRoomCount);
}

TEST(RoomCensusVanillaRomTest, BuiltinFingerprintsMatchVanillaRom) {
  const char* rom_path = std::getenv("YAZE_TEST_ROM_VANILLA");
  if (rom_path == nullptr || !fs::exists(rom_path)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_VANILLA to a US 1.0 ROM";
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path).ok());
  auto fingerprints = ComputeRoomFingerprints(&rom);
  ASSERT_TRUE(fingerprints.ok());
  for (int room = 0; room < kRoomCensusRoomCount; ++room) {
    EXPECT_EQ((*fingerprints)[room], BuiltinVanillaRoomFingerprint(room))
        << "room " << room;
  }
  // Vanilla shows the vanilla dungeons, owners derived from entrances.
  auto census = ComputeRoomCensus(&rom);
  ASSERT_TRUE(census.ok());
  EXPECT_FALSE(census->owners_from_project);
  const auto& hera = census->rooms[0x77];  // Tower of Hera entrance room
  ASSERT_GE(hera.owner_index, 0);
  EXPECT_EQ(census->owners[hera.owner_index].name, "Tower of Hera");
  EXPECT_EQ(census->rooms[0xA7].status, RoomCensusStatus::kReclaimable);
}

}  // namespace
}  // namespace yaze::zelda3
