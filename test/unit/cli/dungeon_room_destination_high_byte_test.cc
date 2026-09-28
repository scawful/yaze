// Room header stair and holewarp bytes keep the source room's high byte.
// Regression: z3ed printed the Mayor's House ($119) stair as $01D instead of
// $11D (the game writes the byte to $A0 and keeps $A1).

#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/dungeon_graph_commands.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_header_destination.h"

namespace yaze::cli {
namespace {

constexpr int kMayorsHouse = 0x119;

static_assert(zelda3::ResolveHeaderDestinationRoom(0x119, 0x1D) == 0x11D);
static_assert(zelda3::ResolveHeaderDestinationRoom(0x019, 0x1D) == 0x01D);
static_assert(zelda3::EncodeHeaderDestinationRoom(0x119, 0x11D) == 0x1D);
static_assert(zelda3::EncodeHeaderDestinationRoom(0x119, 0x01D) == -1);

// Builds a 2 MB ROM whose room-header table points room `room_id` at a header
// with the given holewarp and stair bytes.
std::vector<uint8_t> BuildRomWithHeader(int room_id, uint8_t holewarp,
                                        std::array<uint8_t, 4> stairs) {
  std::vector<uint8_t> data(0x200000, 0);
  constexpr uint32_t kHeaderTableSnes = 0x018000;
  constexpr uint8_t kHeaderBank = 0x01;
  constexpr uint16_t kHeaderWordAddr = 0x9000;
  data[zelda3::kRoomHeaderPointer + 0] = kHeaderTableSnes & 0xFF;
  data[zelda3::kRoomHeaderPointer + 1] = (kHeaderTableSnes >> 8) & 0xFF;
  data[zelda3::kRoomHeaderPointer + 2] = (kHeaderTableSnes >> 16) & 0xFF;
  data[zelda3::kRoomHeaderPointerBank] = kHeaderBank;
  const uint32_t table_pc = SnesToPc(kHeaderTableSnes);
  data[table_pc + room_id * 2 + 0] = kHeaderWordAddr & 0xFF;
  data[table_pc + room_id * 2 + 1] = (kHeaderWordAddr >> 8) & 0xFF;
  const uint32_t header_pc = SnesToPc((kHeaderBank << 16) | kHeaderWordAddr);
  data[header_pc + 9] = holewarp;
  for (int i = 0; i < 4; ++i) {
    data[header_pc + 10 + i] = stairs[i];
  }
  return data;
}

class DungeonRoomDestinationHighByteTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(BuildRomWithHeader(kMayorsHouse, 0x1E,
                                                     {0x1D, 0x00, 0x00, 0x00}))
                    .ok());
  }
  Rom rom_;
};

TEST_F(DungeonRoomDestinationHighByteTest, RoomAccessorsKeepHighByte) {
  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(&rom_, kMayorsHouse);
  EXPECT_EQ(room.staircase_room(0), 0x1D);  // raw header byte
  EXPECT_EQ(room.staircase_destination_room(0), 0x11D);
  EXPECT_EQ(room.holewarp(), 0x1E);
  EXPECT_EQ(room.holewarp_destination_room(), 0x11E);
}

TEST_F(DungeonRoomDestinationHighByteTest, RoomHeaderReportsFullDestination) {
  handlers::DungeonRoomHeaderCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--room=0x119", "--format=json"}, &rom_, &output);
  ASSERT_TRUE(status.ok()) << status;
  const auto json = nlohmann::json::parse(output);
  const auto& decoded = json.at("Room Header Debug").at("decoded");
  EXPECT_EQ(decoded.at("stair1_room"), 0x1D);
  EXPECT_EQ(decoded.at("stair1_destination_room"), "0x11D");
  EXPECT_EQ(decoded.at("holewarp_destination_room"), "0x11E");
}

TEST_F(DungeonRoomDestinationHighByteTest, DungeonGraphReportsFullDestination) {
  handlers::DungeonGraphCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--room=0x119", "--format=json"}, &rom_, &output);
  ASSERT_TRUE(status.ok()) << status;
  const auto json = nlohmann::json::parse(output);
  const auto& graph = json.at("dungeon_graph");
  const auto& node = graph.at("nodes").at(0);
  EXPECT_EQ(node.at("room_id"), "0x119");
  EXPECT_EQ(node.at("stairs").at(0), "0x11D");
  EXPECT_EQ(node.at("holewarp"), "0x11E");
  bool saw_stair = false;
  for (const auto& edge : graph.at("edges")) {
    if (edge.at("type") == "stair1") {
      saw_stair = true;
      EXPECT_EQ(edge.at("from"), "0x119");
      EXPECT_EQ(edge.at("to"), "0x11D");
    }
  }
  EXPECT_TRUE(saw_stair);
}

}  // namespace
}  // namespace yaze::cli
