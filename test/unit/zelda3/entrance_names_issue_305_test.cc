#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_links.h"
#include "zelda3/zelda3_labels.h"

namespace yaze::zelda3 {
namespace {

TEST(EntranceNamesIssue305Test, EntranceId0x45IsSahasrahlaNotThievesTown) {
  // scawful/yaze#305: entrance ID $45 is Sahasrahla's hideout in vanilla ALTTP
  // (USDASM Overworld_Entrance_ID / EntranceData). Room ID $045 is Thieves'
  // Town jail — a different namespace that is easy to confuse in the editor.
  const auto& entrances = Zelda3Labels::GetEntranceNames();
  ASSERT_GT(entrances.size(), 0x45u);
  EXPECT_EQ(entrances[0x45], "Sahasrahla's Hideout");

  const auto& rooms = Zelda3Labels::GetRoomNames();
  ASSERT_GT(rooms.size(), 0x45u);
  EXPECT_NE(entrances[0x45], rooms[0x45]);
  EXPECT_NE(rooms[0x45].find("Thieves"), std::string::npos);
}

TEST(EntranceNamesIssue305Test, VanillaEntranceTablePinsSeveralIds) {
  const auto& names = Zelda3Labels::GetEntranceNames();
  ASSERT_GT(names.size(), 0x46u);
  EXPECT_EQ(names[0x0D], "Sahasrahla's House West");
  EXPECT_EQ(names[0x0E], "Sahasrahla's House East");
  EXPECT_EQ(names[0x33], "Tower of Hera");
  EXPECT_EQ(names[0x34], "Thieves's Town");
  EXPECT_EQ(names[0x44], "Kakariko Inn");
  EXPECT_EQ(names[0x46], "Kakariko's Lame Shop");
}

TEST(EntranceNamesIssue305Test, LegacyRomEntrance0x45TargetsRoom0x105) {
  std::vector<uint8_t> data(0x200000, 0);
  constexpr uint32_t kRoomPc = 0x14813 + 0x45 * 2;
  data[kRoomPc] = 0x05;
  data[kRoomPc + 1] = 0x01;

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  const auto target = ReadDungeonEntranceTarget(rom, 0x45);
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->room_id, 0x105);
}

}  // namespace
}  // namespace yaze::zelda3
