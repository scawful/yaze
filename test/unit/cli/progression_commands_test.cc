// dungeon-key-balance and progression-audit: pure key counting, feature keys,
// spec parsing and rules, plus handler runs on the synthetic census ROM.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "cli/handlers/game/progression_commands.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "unit/zelda3/dungeon/room_census_test_rom.h"

namespace yaze::cli {
namespace {

using handlers::ComputeDungeonKeyBalances;
using handlers::DungeonKeyBalance;
using handlers::KeyItemIds;
using handlers::NormalizeProgressionFeature;
using handlers::ParseProgressionSpec;
using handlers::ProgressionDoor;
using handlers::ProgressionRoom;
using handlers::ProgressionRoomFeatures;
using handlers::RunProgressionAudit;

constexpr int kSmallKeyReceipt = 0x24;
constexpr int kBigKeyReceipt = 0x32;
constexpr int kSmallKeyPot = 0x08;

KeyItemIds TestIds() {
  KeyItemIds ids;
  ids.small_key_receipts = {kSmallKeyReceipt};
  ids.big_key_receipts = {kBigKeyReceipt};
  ids.small_key_pots = {kSmallKeyPot};
  return ids;
}

ProgressionRoom MakeRoom(int room_id, const std::string& owner) {
  ProgressionRoom room;
  room.room_id = room_id;
  room.owner_id = owner;
  room.owner_name = owner;
  room.content.room_id = room_id;
  return room;
}

ProgressionDoor Door(int index, int type, int direction, int along) {
  return {.index = index, .type = type, .direction = direction, .along = along};
}

const DungeonKeyBalance& Find(const std::vector<DungeonKeyBalance>& all,
                              const std::string& owner) {
  for (const auto& balance : all) {
    if (balance.owner_id == owner)
      return balance;
  }
  ADD_FAILURE() << "no balance for " << owner;
  return all.front();
}

TEST(KeyItemIdsTest, ResolvesKeysFromLabels) {
  const KeyItemIds ids = handlers::ResolveKeyItemIds();
  EXPECT_FALSE(ids.small_key_receipts.empty());
  EXPECT_FALSE(ids.big_key_receipts.empty());
  EXPECT_TRUE(ids.small_key_pots.count(0x08) > 0);
}

// Room 0x010 sits west of 0x011 (same row). A small-key door on 0x010's east
// wall and its mirror on 0x011's west wall are one lock.
TEST(DungeonKeyBalanceTest, TwoSidedKeyDoorIsOneLock) {
  ProgressionRoom west = MakeRoom(0x010, "D1");
  ProgressionRoom east = MakeRoom(0x011, "D1");
  west.doors.push_back(Door(0, 0x1C, 3, 10));
  east.doors.push_back(Door(0, 0x1C, 2, 11));
  west.content.chests.push_back({kSmallKeyReceipt, false, "Key"});

  const auto all = ComputeDungeonKeyBalances({west, east}, TestIds());
  const auto& d1 = Find(all, "D1");
  EXPECT_EQ(d1.small_keys, 1);
  EXPECT_EQ(d1.small_key_doors, 1);
  EXPECT_EQ(d1.small_key_door_sides, 2);
  EXPECT_EQ(d1.balance, 0);
  EXPECT_EQ(d1.status, "ok");
  ASSERT_EQ(d1.sinks.size(), 2u);
  EXPECT_EQ(d1.sinks[0].paired_room, 0x011);
  EXPECT_EQ(d1.sinks[1].paired_room, 0x010);
}

// Doors far apart on the shared wall are two locks; a key-door staircase is
// always one lock of its own.
TEST(DungeonKeyBalanceTest, UnpairedDoorsAndStairsAreShort) {
  ProgressionRoom west = MakeRoom(0x010, "D1");
  ProgressionRoom east = MakeRoom(0x011, "D1");
  west.doors.push_back(Door(0, 0x1C, 3, 4));
  east.doors.push_back(Door(0, 0x1C, 2, 20));
  east.doors.push_back(Door(1, 0x20, 0, 8));
  west.content.pots.push_back({kSmallKeyPot, 0, 0, 0, "Key"});

  const auto all = ComputeDungeonKeyBalances({west, east}, TestIds());
  const auto& d1 = Find(all, "D1");
  EXPECT_EQ(d1.small_keys, 1);
  EXPECT_EQ(d1.small_key_doors, 3);
  EXPECT_EQ(d1.balance, -2);
  EXPECT_EQ(d1.status, "short");
}

TEST(DungeonKeyBalanceTest, BigKeyStatusAndKeyDropSprites) {
  ProgressionRoom room = MakeRoom(0x020, "D2");
  room.content.chests.push_back({0x0C, true, "Boomerang"});  // big chest
  room.content.sprites.push_back({.id = 0x8F, .key_drop = 1, .name = "Popo"});
  const auto no_big = ComputeDungeonKeyBalances({room}, TestIds());
  EXPECT_EQ(Find(no_big, "D2").status, "no_big_key");
  EXPECT_EQ(Find(no_big, "D2").big_key_sinks, 1);
  EXPECT_EQ(Find(no_big, "D2").small_keys, 1);

  room.content.sprites.push_back({.id = 0x41, .key_drop = 2, .name = "Guard"});
  const auto with_big = ComputeDungeonKeyBalances({room}, TestIds());
  EXPECT_EQ(Find(with_big, "D2").big_key_sources, 1);
  // One spare small key and no locks: surplus.
  EXPECT_EQ(Find(with_big, "D2").status, "surplus");
}

TEST(DungeonKeyBalanceTest, RoomsWithoutOwnerAreSkipped) {
  ProgressionRoom room = MakeRoom(0x030, "");
  room.doors.push_back(Door(0, 0x1C, 0, 8));
  EXPECT_TRUE(ComputeDungeonKeyBalances({room}, TestIds()).empty());
}

TEST(ProgressionFeatureTest, NormalizesKinds) {
  EXPECT_EQ(*NormalizeProgressionFeature("Dark"), "dark");
  EXPECT_EQ(*NormalizeProgressionFeature("sprite:8f"), "sprite:0x8F");
  EXPECT_EQ(*NormalizeProgressionFeature("sprite", "$8F"), "sprite:0x8F");
  EXPECT_EQ(*NormalizeProgressionFeature("object:0x1A"), "object:0x01A");
  EXPECT_EQ(*NormalizeProgressionFeature("door_class:Bombable"),
            "door_class:bombable");
  EXPECT_FALSE(NormalizeProgressionFeature("dark:01").ok());
  EXPECT_FALSE(NormalizeProgressionFeature("sprite").ok());
  EXPECT_FALSE(NormalizeProgressionFeature("weather:01").ok());
  EXPECT_FALSE(NormalizeProgressionFeature("door_class:locked").ok());
}

TEST(ProgressionFeatureTest, RoomFeatures) {
  ProgressionRoom room = MakeRoom(0x055, "D1");
  room.content.is_dark = true;
  room.content.tag1 = 0x08;
  room.content.sprites.push_back({.id = 0x8F});
  room.doors.push_back(Door(0, 0x28, 0, 8));
  room.object_ids = {0x1A};
  room.collision_values = {0x0E};
  const auto features = ProgressionRoomFeatures(room);
  for (const char* expected :
       {"dark", "door:0x28", "door_class:bombable", "tag:0x08", "sprite:0x8F",
        "object:0x01A", "collision:0x0E"}) {
    EXPECT_NE(std::find(features.begin(), features.end(), expected),
              features.end())
        << expected;
  }
}

constexpr char kSpec[] = R"({
  "gates": [
    {"feature": "dark", "requires": "Lamp"},
    {"feature": "sprite", "id": "0x8F", "requires": ["Bow", "Arrows"]}
  ],
  "regions": {
    "D1": {"owners": ["D1"]},
    "D2": {"owners": ["D2"], "rooms": ["0x099"]},
    "Early": {"includes": ["D1", "D2"]}
  },
  "item_ids": {"chest": {"0x0C": "Boomerang"}},
  "rules": [
    {"name": "no dark early", "forbid_feature": "dark", "regions": ["Early"]},
    {"name": "no bow in D1", "forbid_requires": "bow", "regions": ["D1"]},
    {"name": "D1 has boomerang", "require_source": "Boomerang",
     "regions": ["D1"]},
    {"name": "D2 has lamp", "require_source": "Lamp", "regions": ["D2"]}
  ]
})";

TEST(ProgressionSpecTest, ParsesAndRejects) {
  auto spec = ParseProgressionSpec(kSpec);
  ASSERT_TRUE(spec.ok()) << spec.status();
  ASSERT_EQ(spec->gates.size(), 2u);
  EXPECT_EQ(spec->gates[1].feature, "sprite:0x8F");
  EXPECT_EQ(spec->gates[1].required_items.size(), 2u);
  EXPECT_EQ(spec->regions.at("D2").rooms.count(0x099), 1u);
  EXPECT_EQ(spec->chest_item_names.at(0x0C), "Boomerang");
  EXPECT_EQ(spec->rules.size(), 4u);

  EXPECT_FALSE(ParseProgressionSpec("{").ok());
  EXPECT_FALSE(ParseProgressionSpec(R"({"gates":[{"feature":"dark"}]})").ok());
  EXPECT_FALSE(
      ParseProgressionSpec(R"({"regions":{"A":{"includes":["B"]}}})").ok());
  EXPECT_FALSE(ParseProgressionSpec(
                   R"({"regions":{"A":{}},"rules":[{"forbid_feature":"dark",
                   "require_source":"Lamp","regions":["A"]}]})")
                   .ok());
  EXPECT_FALSE(ParseProgressionSpec(
                   R"({"rules":[{"forbid_feature":"dark","regions":["X"]}]})")
                   .ok());
  // A number where a name belongs is an error, not an exception.
  EXPECT_FALSE(
      ParseProgressionSpec(R"({"gates":[{"feature":5,"requires":"x"}]})").ok());
}

TEST(ProgressionAuditTest, RulesPassAndFail) {
  auto spec = ParseProgressionSpec(kSpec);
  ASSERT_TRUE(spec.ok()) << spec.status();

  ProgressionRoom d1_chest = MakeRoom(0x02A, "D1");
  d1_chest.content.chests.push_back({0x0C, false, "Item 0C"});
  ProgressionRoom d1_bow = MakeRoom(0x02B, "D1");
  d1_bow.content.sprites.push_back({.id = 0x8F});
  ProgressionRoom d2_dark = MakeRoom(0x099, "");  // listed by room ID
  d2_dark.content.is_dark = true;
  ProgressionRoom elsewhere = MakeRoom(0x0C0, "D7");
  elsewhere.content.is_dark = true;

  const auto audit =
      RunProgressionAudit({d1_chest, d1_bow, d2_dark, elsewhere}, *spec);
  ASSERT_EQ(audit.rules.size(), 4u);
  EXPECT_FALSE(audit.rules[0].pass);  // 0x099 is dark and in Early via D2
  EXPECT_EQ(audit.rules[0].offending_rooms, std::vector<int>{0x099});
  EXPECT_FALSE(audit.rules[1].pass);  // 0x02B needs Bow (case-insensitive)
  EXPECT_EQ(audit.rules[1].offending_rooms, std::vector<int>{0x02B});
  EXPECT_TRUE(audit.rules[2].pass);   // item_ids renames chest 0x0C
  EXPECT_FALSE(audit.rules[3].pass);  // nothing gives Lamp in D2
  EXPECT_EQ(audit.rules_failed, 3);

  // Every owned room is audited: 0x0C0 (D7) needs the Lamp too, but it is in
  // no rule region, so it does not fail "no dark early".
  EXPECT_EQ(audit.items.at("Lamp").required_by,
            (std::vector<int>{0x099, 0x0C0}));
  EXPECT_EQ(audit.items.at("Boomerang").given_by, std::vector<int>{0x02A});
}

// --- Handlers on the synthetic census ROM ---------------------------------

constexpr int kDarkRoom = 0x055;
constexpr uint16_t kDarkHeader = 0x9030;

std::vector<uint8_t> BuildProgressionTestRom() {
  auto data = zelda3::test::BuildRoomCensusTestRom();
  const uint32_t header_table_pc = SnesToPc(zelda3::test::kTestHeaderTableSnes);
  data[header_table_pc + kDarkRoom * 2] = kDarkHeader & 0xFF;
  data[header_table_pc + kDarkRoom * 2 + 1] = kDarkHeader >> 8;
  data[SnesToPc(0x010000 | kDarkHeader)] = 0x01;  // header byte0 bit0: dark
  return data;
}

class ProgressionHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(BuildProgressionTestRom()).ok());
  }

  std::string WriteSpec(const std::string& text) {
    const auto path = std::filesystem::path(::testing::TempDir()) /
                      "progression_handler_spec.json";
    std::ofstream(path) << text;
    return path.string();
  }

  Rom rom_;
};

TEST_F(ProgressionHandlerTest, KeyBalanceRunsWithoutLocks) {
  handlers::DungeonKeyBalanceCommandHandler handler;
  std::string output;
  const auto status = handler.Run({"--format=json"}, &rom_, &output);
  ASSERT_TRUE(status.ok()) << status << output;
  const auto json = nlohmann::json::parse(output).at("Dungeon Key Balance");
  EXPECT_EQ(json.at("failing").get<int>(), 0);
  for (const auto& dungeon : json.at("dungeons")) {
    EXPECT_EQ(dungeon.at("small_key_doors").get<int>(), 0);
  }
}

TEST_F(ProgressionHandlerTest, AuditFailsOnDarkRoomAndExitsNonZero) {
  const std::string spec = WriteSpec(R"({
    "gates": [{"feature": "dark", "requires": "Lamp"}],
    "regions": {"Early": {"rooms": ["0x055", "0x056"]}},
    "rules": [{"name": "no dark early", "forbid_feature": "dark",
               "regions": ["Early"]}]
  })");
  handlers::ProgressionAuditCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--spec=" + spec, "--format=json"}, &rom_, &output);
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  const auto json = nlohmann::json::parse(output).at("Progression Audit");
  ASSERT_EQ(json.at("rules").size(), 1u);
  EXPECT_FALSE(json.at("rules")[0].at("pass").get<bool>());
  EXPECT_EQ(json.at("rules")[0].at("offending_rooms"),
            nlohmann::json::array({"0x055"}));
  EXPECT_EQ(json.at("summary").at("rules_failed").get<int>(), 1);

  const std::string clean = WriteSpec(R"({
    "regions": {"Early": {"rooms": ["0x056"]}},
    "rules": [{"forbid_feature": "dark", "regions": ["Early"]}]
  })");
  EXPECT_TRUE(
      handler.Run({"--spec=" + clean, "--format=json"}, &rom_, &output).ok())
      << output;
}

}  // namespace
}  // namespace yaze::cli
