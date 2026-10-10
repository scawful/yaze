#include "zelda3/overworld/special_entrance_inspector.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "oracle_rom_fixture.h"
#include "rom/rom.h"
#include "unit/zelda3/overworld_allocation_test_rom.h"

namespace yaze::zelda3 {
namespace {

void WriteFourRowReader(std::vector<uint8_t>& data) {
  constexpr std::array<uint8_t, 17> kPrefix = {
      0xC2, 0x31, 0x20, 0x9A, 0xDE, 0xBF, 0x00, 0x80, 0xBD,
      0x29, 0xFF, 0x01, 0x85, 0x00, 0xA2, 0x08, 0x00};
  constexpr std::array<uint8_t, 18> kComparisons = {
      0xDF, 0x29, 0xDE, 0x0E, 0xD0, 0xF6, 0xA5, 0x8A, 0xDF,
      0x31, 0xDE, 0x0E, 0xD0, 0xEC, 0xBF, 0x41, 0xDE, 0x0E};
  std::copy(kPrefix.begin(), kPrefix.end(), data.begin() + 0x75E49);
  std::copy(kComparisons.begin(), kComparisons.end(), data.begin() + 0x75E60);
}

void SetRow(std::vector<uint8_t>& data, int row, uint16_t trigger,
            uint16_t source, uint16_t destination) {
  test::TestWriteWord(data, 0x75E29 + 2 * row, trigger);
  test::TestWriteWord(data, 0x75E31 + 2 * row, source);
  test::TestWriteWord(data, 0x75E39 + 2 * row, 0x0008);
  test::TestWriteWord(data, 0x75E41 + 2 * row, destination);
}

struct Fixture {
  Rom rom;
  Overworld overworld{&rom};
  Fixture() {
    auto bytes = test::BuildOverworldAllocationTestRom(true);
    WriteFourRowReader(bytes);
    SetRow(bytes, 0, 0x01EF, 0x2A, 0x0180);
    SetRow(bytes, 1, 0x01EF, 0x18, 0x0181);
    SetRow(bytes, 2, 0x00AD, 0x0F, 0x0182);
    SetRow(bytes, 3, 0x00B9, 0x81, 0x0189);
    EXPECT_TRUE(rom.LoadFromData(bytes).ok());
    EXPECT_TRUE(overworld.Load(&rom).ok());
    // $2C2's editable first word is $1CAD. Only its low nine bits compare.
    overworld.mutable_tiles16()->at(0x2C2).tile0_ = gfx::TileInfo(0xAD, 0x1C);
  }
  void Place(int screen, int x, int y, uint16_t tile) {
    const int world = screen / 0x40;
    overworld.GetMapTiles(
        world)[(screen % 8) * 32 + x][((screen % 0x40) / 8) * 32 + y] = tile;
  }
};

TEST(SpecialEntranceInspectorTest, MatchesMaskedDefinitionAndEveryPosition) {
  Fixture f;
  f.Place(0x0F, 2, 3, 0x2C2);
  f.Place(0x0F, 3, 3, 0x2C2);
  auto result = InspectSpecialEntrances(f.rom, f.overworld, 0x0F);
  ASSERT_TRUE(result.available);
  ASSERT_EQ(result.matches.size(), 2);
  EXPECT_EQ(result.matches[0].first_word, 0x1CAD);
  EXPECT_EQ(result.matches[0].masked_trigger, 0x00AD);
  EXPECT_EQ(result.matches[0].tile16_id, 0x2C2);
  EXPECT_EQ(result.rows[result.matches[0].row_index].destination_marker,
            0x0182);
  EXPECT_EQ(result.matches[1].tile_x, 3);
  f.Place(0x0F, 2, 3, 0);  // model edit; undo restores the old value
  EXPECT_EQ(InspectSpecialEntrances(f.rom, f.overworld, 0x0F).matches.size(),
            1);
  f.Place(0x0F, 2, 3, 0x2C2);
  EXPECT_EQ(InspectSpecialEntrances(f.rom, f.overworld, 0x0F).matches.size(),
            2);
}

TEST(SpecialEntranceInspectorTest, WrongAreaAndNoMatch) {
  Fixture f;
  f.Place(0x0E, 2, 3, 0x2C2);
  EXPECT_TRUE(
      InspectSpecialEntrances(f.rom, f.overworld, 0x0F).matches.empty());
  EXPECT_TRUE(
      InspectSpecialEntrances(f.rom, f.overworld, 0x0E).matches.empty());
}

TEST(SpecialEntranceInspectorTest, MaskIsExactlyNineBits) {
  Fixture f;
  EXPECT_TRUE(f.rom.WriteShort(0x75E29 + 2 * 2, 0x01FF).ok());
  f.overworld.mutable_tiles16()->at(0x2C4).tile0_ =
      gfx::TileInfo(0xFF, 0x1D);  // raw $1DFF, masked $01FF
  f.Place(0x0F, 5, 6, 0x2C4);
  auto result = InspectSpecialEntrances(f.rom, f.overworld, 0x0F);
  ASSERT_EQ(result.matches.size(), 1);
  EXPECT_EQ(result.matches[0].first_word, 0x1DFF);
  EXPECT_EQ(result.matches[0].masked_trigger, 0x01FF);
  EXPECT_TRUE(f.rom.WriteShort(0x75E29 + 2 * 2, 0x03FF).ok());
  EXPECT_TRUE(
      InspectSpecialEntrances(f.rom, f.overworld, 0x0F).matches.empty());
}

TEST(SpecialEntranceInspectorTest, ChildScreenUsesParentAreaAndWorldGrid) {
  Fixture f;
  f.overworld.mutable_tiles16()->at(0x2C5).tile0_ = gfx::TileInfo(0xB9, 0x00);
  f.Place(0x82, 7, 8, 0x2C5);
  const auto result = InspectSpecialEntrances(f.rom, f.overworld, 0x8A);
  ASSERT_EQ(result.parent_area, 0x81);
  ASSERT_EQ(result.matches.size(), 1);
  EXPECT_EQ(result.matches[0].screen_id, 0x82);
  EXPECT_EQ(result.matches[0].parent_area, 0x81);
  EXPECT_EQ(result.matches[0].world_x, 71);
  EXPECT_EQ(result.matches[0].world_y, 8);
  EXPECT_EQ(result.rows[result.matches[0].row_index].destination_area, 0x89);
}

TEST(SpecialEntranceInspectorTest, DuplicatePairUsesRuntimeReverseOrder) {
  Fixture f;
  f.Place(0x0F, 2, 3, 0x2C2);
  EXPECT_TRUE(f.rom.WriteShort(0x75E29 + 2 * 3, 0x00AD).ok());
  EXPECT_TRUE(f.rom.WriteShort(0x75E31 + 2 * 3, 0x000F).ok());
  auto result = InspectSpecialEntrances(f.rom, f.overworld, 0x0F);
  ASSERT_EQ(result.matches.size(), 1);
  EXPECT_EQ(result.matches[0].row_index, 3);
  EXPECT_EQ(result.rows[3].destination_marker, 0x0189);
}

TEST(SpecialEntranceInspectorTest, LoadedBaseRowDiffersFromRcRow) {
  Fixture f;
  // This is the editable-base first row, unlike the compiled RC first row.
  EXPECT_TRUE(f.rom.WriteShort(0x75E29, 0x0105).ok());
  EXPECT_TRUE(f.rom.WriteShort(0x75E31, 0x0000).ok());
  f.overworld.mutable_tiles16()->at(0x2C3).tile0_ = gfx::TileInfo(0x05, 0x01);
  f.Place(0x00, 4, 5, 0x2C3);
  f.Place(0x2A, 4, 5, 0x2C3);
  auto base = InspectSpecialEntrances(f.rom, f.overworld, 0x00);
  ASSERT_EQ(base.matches.size(), 1);
  EXPECT_EQ(base.rows[0].trigger, 0x0105);
  EXPECT_EQ(base.matches[0].screen_id, 0x00);
  EXPECT_TRUE(
      InspectSpecialEntrances(f.rom, f.overworld, 0x2A).matches.empty());
  EXPECT_TRUE(f.rom.WriteShort(0x75E29, 0x01EF).ok());
  EXPECT_TRUE(f.rom.WriteShort(0x75E31, 0x002A).ok());
  f.overworld.mutable_tiles16()->at(0x2C3).tile0_ = gfx::TileInfo(0xEF, 0x01);
  auto rc = InspectSpecialEntrances(f.rom, f.overworld, 0x2A);
  ASSERT_EQ(rc.matches.size(), 1);
  EXPECT_EQ(rc.rows[0].source_area, 0x2A);
  EXPECT_TRUE(
      InspectSpecialEntrances(f.rom, f.overworld, 0x00).matches.empty());
}

TEST(SpecialEntranceInspectorTest, UnsupportedReaderDoesNotBorrowKnownRows) {
  Fixture f;
  EXPECT_TRUE(f.rom.WriteByte(0x75E4E, 0x00).ok());
  auto result = InspectSpecialEntrances(f.rom, f.overworld, 0x0F);
  EXPECT_FALSE(result.available);
  EXPECT_TRUE(result.matches.empty());
}

}  // namespace
}  // namespace yaze::zelda3
