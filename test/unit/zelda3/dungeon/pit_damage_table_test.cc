#include <gtest/gtest.h>

#include <array>
#include <utility>

#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/pit_damage_table.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3::test {
namespace {

constexpr std::array<uint16_t, 57> kVanillaRoomsWithPitDamage = {
    0x0072, 0x0082, 0x0040, 0x00C0, 0x0112, 0x0056, 0x0057, 0x0058, 0x0067,
    0x0068, 0x0049, 0x0098, 0x00D1, 0x00C3, 0x00A3, 0x00A2, 0x0092, 0x00A0,
    0x004E, 0x007F, 0x00AF, 0x00F0, 0x00F1, 0x00E6, 0x00E7, 0x00C6, 0x00C7,
    0x00D6, 0x00B4, 0x00B5, 0x00C5, 0x0024, 0x00D5, 0x00C9, 0x002A, 0x001A,
    0x004B, 0x00BC, 0x0044, 0x00FB, 0x007B, 0x007C, 0x008B, 0x008D, 0x009B,
    0x009C, 0x009D, 0x00A5, 0x0095, 0x001C, 0x005C, 0x007D, 0x004C, 0x0096,
    0x0120, 0x003C, 0x0123,
};

class PitDamageTableTest : public ::testing::Test {
 protected:
  void SetUp() override {
    YAZE_SKIP_IF_ROM_MISSING(::yaze::test::RomRole::kVanilla,
                             "PitDamageTableTest");
    const auto path = ::yaze::test::TestRomManager::GetRomPath(
        ::yaze::test::RomRole::kVanilla);
    ASSERT_TRUE(rom_.LoadFromFile(path).ok());
  }

  Rom rom_;
};

TEST_F(PitDamageTableTest, LoadFromRomMatchesVanillaMembership) {
  PitDamageTable table;
  ASSERT_TRUE(PitDamageTable::LoadFromRom(&rom_, &table).ok());
  ASSERT_EQ(table.room_ids().size(), kVanillaRoomsWithPitDamage.size());
  for (size_t i = 0; i < kVanillaRoomsWithPitDamage.size(); ++i) {
    EXPECT_EQ(table.room_ids()[i], kVanillaRoomsWithPitDamage[i])
        << "idx=" << i;
  }
  EXPECT_TRUE(table.Contains(0x0072));
  EXPECT_FALSE(table.Contains(0x0001));
}

TEST_F(PitDamageTableTest, DirtyRoundTripPreservesBytes) {
  PitDamageTable table;
  ASSERT_TRUE(PitDamageTable::LoadFromRom(&rom_, &table).ok());
  const auto before = rom_.vector();

  table.MarkDirty();
  ASSERT_TRUE(table.SaveToRom(&rom_).ok());

  PitDamageTable reloaded;
  ASSERT_TRUE(PitDamageTable::LoadFromRom(&rom_, &reloaded).ok());
  EXPECT_EQ(reloaded.room_ids(), table.room_ids());

  // No-op dirty save must be byte-identical for the table region.
  PitDamageTable table2;
  ASSERT_TRUE(PitDamageTable::LoadFromRom(&rom_, &table2).ok());
  table2.MarkDirty();
  ASSERT_TRUE(table2.SaveToRom(&rom_).ok());
  EXPECT_EQ(before, rom_.vector());
}

TEST_F(PitDamageTableTest, DirtySaveAllPitsWritesEditedMembership) {
  PitDamageTable table;
  ASSERT_TRUE(PitDamageTable::LoadFromRom(&rom_, &table).ok());

  auto edited = table.room_ids();
  ASSERT_GE(edited.size(), 2u);
  std::swap(edited[0], edited[1]);
  table.SetRoomIds(edited);

  ASSERT_TRUE(SaveAllPits(&rom_, &table).ok());

  PitDamageTable reloaded;
  ASSERT_TRUE(PitDamageTable::LoadFromRom(&rom_, &reloaded).ok());
  EXPECT_EQ(reloaded.room_ids(), edited);
}

TEST_F(PitDamageTableTest, SaveRejectsWrongCapacityWithoutRepointing) {
  PitDamageTable table;
  ASSERT_TRUE(PitDamageTable::LoadFromRom(&rom_, &table).ok());

  auto edited = table.room_ids();
  ASSERT_FALSE(edited.empty());
  edited.pop_back();
  table.SetRoomIds(edited);

  EXPECT_FALSE(SaveAllPits(&rom_, &table).ok());
}

TEST_F(PitDamageTableTest, LoadGameDataPopulatesTable) {
  GameData game_data;
  ASSERT_TRUE(LoadGameData(rom_, game_data).ok());
  EXPECT_EQ(game_data.pit_damage_table.room_ids().size(),
            kVanillaRoomsWithPitDamage.size());
}

}  // namespace
}  // namespace yaze::zelda3::test
