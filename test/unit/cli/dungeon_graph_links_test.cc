// z3ed dungeon-graph / dungeon-discover / dungeon-room-graph / entrance-info
// and the room census build their edges from zelda3::CollectRoomLinks.
// Review findings (PR #261): the graph commands disagreed with the census on
// ZScream entrance tables, header byte 0, out-of-range room IDs, teleport
// doors and the column-15 neighbor wrap.

#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "cli/handlers/game/dungeon_graph_commands.h"
#include "rom/rom.h"
#include "unit/zelda3/dungeon/room_census_test_rom.h"
#include "zelda3/dungeon/room_census.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_links.h"

namespace yaze::cli {
namespace {

using zelda3::test::SetTestRoom;
using zelda3::test::TestDoorBytes;
using zelda3::test::TestObjectBytes;
using zelda3::test::TestRoomStream;

constexpr int kNorth = 0;
constexpr int kWest = 2;
constexpr int kEast = 3;
constexpr int kStreamPc = zelda3::test::kTestObjectDataPc + 0x400;

std::array<uint8_t, 14> Header(uint8_t holewarp,
                               std::array<uint8_t, 4> stairs = {}) {
  std::array<uint8_t, 14> header{};
  header[zelda3::test::kTestHeaderHolewarp] = holewarp;
  for (int i = 0; i < 4; ++i) {
    header[zelda3::test::kTestHeaderStair1 + i] = stairs[i];
  }
  return header;
}

// The census test ROM plus:
// - ZScream's entrance hook ($02:D99F JSL $0FF008) and bank $0F tables:
//   entrance 0x05 -> 0x020, 0x06 -> 0x02F, 0x07 -> 0x119. The vanilla table
//   says 0x021 for entrance 0x05.
// - 0x020: teleport doors (type 0x46) east (stair4 = 0x33) and west (stair3
//   = 0x34), and a type 0x46 door on the north wall, which is a plain door.
// - 0x02F (column 15) east door; 0x030 (next row, column 0) west door.
// - 0x006: warp tile, holewarp byte 0x00 -> 0x000.
// - 0x119: warp tile, holewarp byte 0xFF (0x1FF, not a room), stair1 byte
//   0x1D with no stair object.
std::vector<uint8_t> BuildGraphTestRom() {
  auto data = zelda3::test::BuildRoomCensusTestRom();
  data[0x1599F] = 0x22;
  data[0x159A0] = 0x08;
  data[0x159A1] = 0xF0;
  data[0x159A2] = 0x0F;
  auto expanded = [&](int entrance, uint16_t room, uint8_t dungeon) {
    data[0x078000 + entrance * 2] = room & 0xFF;
    data[0x078000 + entrance * 2 + 1] = room >> 8;
    data[0x079800 + entrance] = dungeon;
  };
  expanded(0x05, 0x020, 0x04);
  expanded(0x06, 0x02F, 0x04);
  expanded(0x07, 0x119, 0xFF);
  data[zelda3::kEntranceRoom + 0x05 * 2] = 0x21;  // vanilla table: 0x021

  int pc = kStreamPc;
  uint16_t header = 0x9100;
  auto room = [&](int id, const std::vector<uint8_t>& stream,
                  const std::array<uint8_t, 14>& bytes) {
    SetTestRoom(data, id, pc, stream, header, bytes);
    pc += 0x40;
    header += 0x10;
  };
  room(0x020,
       TestRoomStream(
           {}, {TestDoorBytes(6, kEast, 0x46), TestDoorBytes(0, kWest, 0x46),
                TestDoorBytes(0, kNorth, 0x46)}),
       Header(0, {0, 0, 0x34, 0x33}));
  room(0x02F, TestRoomStream({}, {TestDoorBytes(6, kEast, 0x00)}), Header(0));
  room(0x030, TestRoomStream({}, {TestDoorBytes(0, kWest, 0x00)}), Header(0));
  room(0x006, TestRoomStream({TestObjectBytes(0xFCA, 10, 10)}), Header(0x00));
  room(0x119, TestRoomStream({TestObjectBytes(0xFCA, 10, 10)}),
       Header(0xFF, {0x1D, 0, 0, 0}));
  return data;
}

class DungeonGraphLinksTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(BuildGraphTestRom()).ok());
  }

  nlohmann::json Run(resources::CommandHandler& handler,
                     const std::vector<std::string>& args, const char* root) {
    std::string output;
    std::vector<std::string> all = args;
    all.push_back("--format=json");
    const auto status = handler.Run(all, &rom_, &output);
    EXPECT_TRUE(status.ok()) << status;
    if (!status.ok()) {
      return nlohmann::json::object();
    }
    return nlohmann::json::parse(output).at(root);
  }

  Rom rom_;
};

TEST_F(DungeonGraphLinksTest, EntranceTablesMatchTheGame) {
  const auto target = zelda3::ReadDungeonEntranceTarget(rom_, 0x05);
  ASSERT_TRUE(target.has_value());
  EXPECT_TRUE(target->expanded);
  EXPECT_EQ(target->room_id, 0x020);
  EXPECT_EQ(target->dungeon_id, 0x04);

  handlers::EntranceInfoCommandHandler info;
  const auto entrance = Run(info, {"--entrance=0x05"}, "entrance");
  EXPECT_EQ(entrance.at("room_id"), "0x020");
  EXPECT_EQ(entrance.at("dungeon_id"), "0x04");
  EXPECT_EQ(entrance.at("entrance_table"), "zscream_bank_0f");
  EXPECT_EQ(entrance.at("vanilla_table_room_id"), "0x021");

  handlers::DungeonDiscoverCommandHandler discover;
  EXPECT_EQ(Run(discover, {"--entrance=0x05"}, "discovery").at("start_room"),
            "0x020");
  handlers::DungeonRoomGraphCommandHandler room_graph;
  EXPECT_EQ(Run(room_graph, {"--entrance=0x05"}, "room_graph").at("start_room"),
            "0x020");

  // The census reads the same table.
  auto input = zelda3::CollectRoomCensusInput(&rom_);
  ASSERT_TRUE(input.ok()) << input.status();
  EXPECT_TRUE(input->expanded_entrance_tables);
  bool entrance_5 = false;
  for (const auto& fact : input->entrances) {
    if (fact.entrance_id == 0x05 &&
        fact.kind != zelda3::RoomReferenceKind::kSpawn) {
      entrance_5 = true;
      EXPECT_EQ(fact.room_id, 0x020);
    }
  }
  EXPECT_TRUE(entrance_5);
}

TEST_F(DungeonGraphLinksTest, TeleportDoorsUseStairSlots) {
  zelda3::RoomLinkFactsCache cache(&rom_);
  const auto* facts = cache.Get(0x020);
  ASSERT_NE(facts, nullptr);
  ASSERT_EQ(facts->doors.size(), 3u);
  EXPECT_EQ(facts->doors[0].role, zelda3::RoomDoorRole::kTeleport);  // east
  EXPECT_EQ(facts->doors[1].role, zelda3::RoomDoorRole::kTeleport);  // west
  // Type 0x46 on a north wall gets attribute 0x88, not 0x89: a plain door.
  EXPECT_EQ(facts->doors[2].role, zelda3::RoomDoorRole::kNeighbor);

  handlers::DungeonRoomGraphCommandHandler room_graph;
  const auto graph = Run(room_graph, {"--entrance=0x05"}, "room_graph");
  std::set<std::string> rooms;
  for (const auto& room : graph.at("rooms")) {
    rooms.insert(room.at("room_id"));
  }
  EXPECT_EQ(rooms, (std::set<std::string>{"0x020", "0x033", "0x034"}));
  std::set<std::string> teleports;
  for (const auto& door : graph.at("door_edges")) {
    if (door.at("role") == "teleport") {
      EXPECT_TRUE(door.at("strong"));
      teleports.insert(door.at("type").get<std::string>() + "->" +
                       door.at("to").get<std::string>());
    } else {
      EXPECT_EQ(door.at("type"), "door_north");
      EXPECT_EQ(door.at("to"), "0x010");
      EXPECT_FALSE(door.at("strong"));  // No door back in 0x010.
    }
  }
  EXPECT_EQ(teleports,
            (std::set<std::string>{"door_east->0x033", "door_west->0x034"}));

  handlers::DungeonGraphCommandHandler graph_handler;
  const auto header_graph =
      Run(graph_handler, {"--room=0x020"}, "dungeon_graph");
  int teleport_edges = 0;
  for (const auto& edge : header_graph.at("edges")) {
    if (edge.at("type") == "teleport_door") {
      ++teleport_edges;
      EXPECT_TRUE(edge.at("strong"));
    }
  }
  EXPECT_EQ(teleport_edges, 2);

  // The census sees the same link.
  auto census = zelda3::ComputeRoomCensus(&rom_);
  ASSERT_TRUE(census.ok());
  bool teleport_ref = false;
  for (const auto& ref : census->rooms[0x033].references) {
    teleport_ref |= ref.kind == zelda3::RoomReferenceKind::kTeleportDoor &&
                    ref.from_room == 0x020;
  }
  EXPECT_TRUE(teleport_ref);
}

TEST_F(DungeonGraphLinksTest, ColumnFifteenDoorDoesNotWrap) {
  handlers::DungeonRoomGraphCommandHandler room_graph;
  const auto graph = Run(room_graph, {"--entrance=0x06"}, "room_graph");
  EXPECT_EQ(graph.at("rooms_discovered"), 1);
  ASSERT_EQ(graph.at("door_edges").size(), 1u);
  EXPECT_EQ(graph.at("door_edges").at(0).at("to"), "none");

  auto census = zelda3::ComputeRoomCensus(&rom_);
  ASSERT_TRUE(census.ok());
  for (const auto& ref : census->rooms[0x030].references) {
    EXPECT_NE(ref.from_room, 0x02F) << ref.detail;
  }
}

TEST_F(DungeonGraphLinksTest, HeaderByteZeroIsRoomZeroWhenUsed) {
  handlers::DungeonGraphCommandHandler handler;
  const auto used = Run(handler, {"--room=0x006"}, "dungeon_graph");
  ASSERT_EQ(used.at("edges").size(), 1u);
  EXPECT_EQ(used.at("edges").at(0).at("type"), "holewarp");
  EXPECT_EQ(used.at("edges").at(0).at("to"), "0x000");
  EXPECT_TRUE(used.at("edges").at(0).at("strong"));

  // Default header: byte 0 and nothing uses it.
  const auto unused = Run(handler, {"--room=0x007"}, "dungeon_graph");
  EXPECT_TRUE(unused.at("edges").empty());
}

TEST_F(DungeonGraphLinksTest, OutOfRangeDestinationsAreNotRooms) {
  handlers::DungeonGraphCommandHandler handler;
  const auto graph = Run(handler, {"--room=0x119"}, "dungeon_graph");
  ASSERT_EQ(graph.at("edges").size(), 1u);
  EXPECT_EQ(graph.at("edges").at(0).at("to"), "0x11D");
  EXPECT_FALSE(graph.at("edges").at(0).at("strong"));  // No stair object.
  ASSERT_EQ(graph.at("out_of_range").size(), 1u);
  EXPECT_EQ(graph.at("out_of_range").at(0).at("to"), "0x1FF");
  EXPECT_EQ(graph.at("stats").at("out_of_range_edges"), 1);

  std::string output;
  EXPECT_FALSE(handler.Run({"--room=0x128"}, &rom_, &output).ok());

  // Discovery neither walks into 0x1FF nor follows the unused stair byte.
  handlers::DungeonDiscoverCommandHandler discover;
  const auto discovery = Run(discover, {"--entrance=0x07"}, "discovery");
  EXPECT_EQ(discovery.at("rooms_discovered"), 1);
  EXPECT_EQ(discovery.at("out_of_range").size(), 1u);

  // The census drops it too.
  auto census = zelda3::ComputeRoomCensus(&rom_);
  ASSERT_TRUE(census.ok());
  EXPECT_EQ(static_cast<int>(census->rooms.size()),
            zelda3::kRoomCensusRoomCount);
}

}  // namespace
}  // namespace yaze::cli
