// dungeon-find: the all-room search over the dungeon-list-* readers, plus the
// dungeon-room-header is_dark field (byte0 bit0 set means the room is DARK;
// the older is_light field carries the same bit under the wrong name).

#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/dungeon_find_commands.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "unit/zelda3/dungeon/room_census_test_rom.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::cli {
namespace {

using handlers::DungeonFindChest;
using handlers::DungeonFindFilter;
using handlers::DungeonFindKind;
using handlers::DungeonFindPot;
using handlers::DungeonFindRoom;
using handlers::DungeonFindSprite;
using handlers::FindDungeonRows;
using handlers::ParseDungeonFindMatch;

constexpr int kDarkRoom = 0x055;
constexpr int kSpriteRoom = 0x072;
constexpr uint16_t kDarkHeader = 0x9030;

// Room kDarkRoom: header byte0 = 0x01 (dark), spriteset 0x22, tag1 0x08.
// Room kSpriteRoom: sprite 0x8F at (5,6) carrying a small key and sprite
// 0x41 at (9,8) carrying the big key (the 0xE4 marker sprites follow the
// carriers, as in vanilla lists).
std::vector<uint8_t> BuildFindTestRom() {
  auto data = zelda3::test::BuildRoomCensusTestRom();
  const uint32_t header_table_pc = SnesToPc(zelda3::test::kTestHeaderTableSnes);
  data[header_table_pc + kDarkRoom * 2] = kDarkHeader & 0xFF;
  data[header_table_pc + kDarkRoom * 2 + 1] = kDarkHeader >> 8;
  const uint32_t header_pc = SnesToPc(0x010000 | kDarkHeader);
  data[header_pc + 0] = 0x01;
  data[header_pc + 3] = 0x22;
  data[header_pc + zelda3::test::kTestHeaderTag1] = 0x08;

  const int list_pc = zelda3::kSpritesData + 0x20;
  const std::vector<uint8_t> list = {0x00,              // sort mode
                                     0x06, 0x05, 0x8F,  // carrier
                                     0xFE, 0x00, 0xE4,  // small key
                                     0x08, 0x09, 0x41,  // carrier
                                     0xFD, 0x00, 0xE4,  // big key
                                     0x0A, 0x0A, 0x8F,  // no key
                                     0xFF};
  std::copy(list.begin(), list.end(), data.begin() + list_pc);
  const uint16_t list_word = static_cast<uint16_t>(PcToSnes(list_pc));
  const int pointer_pc =
      zelda3::test::kTestSpritePointerTablePc + kSpriteRoom * 2;
  data[pointer_pc] = list_word & 0xFF;
  data[pointer_pc + 1] = list_word >> 8;
  return data;
}

class DungeonFindCommandTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(BuildFindTestRom()).ok());
  }

  nlohmann::json RunFind(const std::vector<std::string>& args) {
    handlers::DungeonFindCommandHandler handler;
    std::vector<std::string> full = args;
    full.push_back("--format=json");
    std::string output;
    const auto status = handler.Run(full, &rom_, &output);
    EXPECT_TRUE(status.ok()) << status;
    if (!status.ok()) {
      return {};
    }
    return nlohmann::json::parse(output).at("Dungeon Find");
  }

  Rom rom_;
};

TEST(DungeonFindMatchTest, HexIdsAndNames) {
  EXPECT_EQ(*ParseDungeonFindMatch("0x0C", 0xFF)->id, 0x0C);
  EXPECT_EQ(*ParseDungeonFindMatch("$12", 0xFF)->id, 0x12);
  EXPECT_EQ(*ParseDungeonFindMatch("E4", 0xFF)->id, 0xE4);
  EXPECT_EQ(*ParseDungeonFindMatch("12", 0xFF)->id, 0x12);
  // All-hex words with no decimal digit are names.
  EXPECT_FALSE(ParseDungeonFindMatch("Bee", 0xFF)->id.has_value());
  EXPECT_EQ(ParseDungeonFindMatch("Lamp", 0xFF)->name_lower, "lamp");
  EXPECT_FALSE(ParseDungeonFindMatch("0x100", 0xFF).ok());
  EXPECT_FALSE(ParseDungeonFindMatch("  ", 0xFF).ok());
}

TEST(DungeonFindFilterTest, ValidateRejectsEmptyAndMismatchedKind) {
  DungeonFindFilter empty;
  EXPECT_FALSE(handlers::ValidateDungeonFindFilter(empty).ok());

  DungeonFindFilter item_sprite_kind;
  item_sprite_kind.item = *ParseDungeonFindMatch("0x0C", 0xFF);
  item_sprite_kind.kinds = {DungeonFindKind::kSprite};
  EXPECT_FALSE(handlers::ValidateDungeonFindFilter(item_sprite_kind).ok());

  DungeonFindFilter kind_only;
  kind_only.kinds = {DungeonFindKind::kChest};
  EXPECT_TRUE(handlers::ValidateDungeonFindFilter(kind_only).ok());
}

DungeonFindRoom SampleRoom() {
  DungeonFindRoom room;
  room.room_id = 0x2A;
  room.spriteset = 0x10;
  room.tag1 = 0x08;
  room.sprites.push_back(DungeonFindSprite{.id = 0xE4,
                                           .x = 3,
                                           .y = 4,
                                           .key_drop = 0,
                                           .name = "Key",
                                           .vanilla_name = "Key"});
  room.sprites.push_back(DungeonFindSprite{.id = 0x8F,
                                           .x = 5,
                                           .y = 6,
                                           .key_drop = 2,
                                           .name = "Blob",
                                           .vanilla_name = "Zol"});
  room.chests.push_back(
      DungeonFindChest{.id = 0x0C, .is_big = true, .name = "Boomerang"});
  room.chests.push_back(
      DungeonFindChest{.id = 0x34, .is_big = false, .name = "Rupee"});
  room.pots.push_back(DungeonFindPot{
      .id = 0x08, .position = 0x0204, .tile_x = 2, .tile_y = 4, .name = "Key"});
  return room;
}

TEST(FindDungeonRowsTest, ItemMatchesChestsAndPotsByIdAndName) {
  const DungeonFindRoom room = SampleRoom();
  DungeonFindFilter by_id;
  by_id.item = *ParseDungeonFindMatch("0x0C", 0xFF);
  auto rows = FindDungeonRows(room, by_id);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].kind, DungeonFindKind::kChest);
  EXPECT_EQ(rows[0].raw, 0x0C);
  EXPECT_TRUE(rows[0].is_big_chest);

  DungeonFindFilter by_name;
  by_name.item = *ParseDungeonFindMatch("key", 0xFF);
  rows = FindDungeonRows(room, by_name);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].kind, DungeonFindKind::kPot);
  EXPECT_EQ(*rows[0].x, 2);
  EXPECT_EQ(*rows[0].y, 4);
}

TEST(FindDungeonRowsTest, SpriteMatchesProjectOrVanillaNameAndKeyDrop) {
  const DungeonFindRoom room = SampleRoom();
  DungeonFindFilter vanilla;
  vanilla.sprite = *ParseDungeonFindMatch("zol", 0xFF);
  auto rows = FindDungeonRows(room, vanilla);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].raw, 0x8F);
  EXPECT_EQ(rows[0].index, 1);

  DungeonFindFilter key_drop;
  key_drop.key_drop = true;
  rows = FindDungeonRows(room, key_drop);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].key_drop, 2);
}

TEST(FindDungeonRowsTest, RoomFiltersGateRowsAndHeaderRowsNeedNoElement) {
  DungeonFindRoom room = SampleRoom();
  DungeonFindFilter dark;
  dark.dark = true;
  EXPECT_TRUE(FindDungeonRows(room, dark).empty());
  room.is_dark = true;
  auto rows = FindDungeonRows(room, dark);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].kind, DungeonFindKind::kHeader);

  DungeonFindFilter tag_and_item;
  tag_and_item.tag = 0x09;
  tag_and_item.item = *ParseDungeonFindMatch("0x0C", 0xFF);
  EXPECT_TRUE(FindDungeonRows(room, tag_and_item).empty());
  tag_and_item.tag = 0x08;
  EXPECT_EQ(FindDungeonRows(room, tag_and_item).size(), 1u);

  // --sprite and --item together need a match in both groups.
  DungeonFindFilter both;
  both.sprite = *ParseDungeonFindMatch("0x8F", 0xFF);
  both.item = *ParseDungeonFindMatch("0x99", 0xFF);
  EXPECT_TRUE(FindDungeonRows(room, both).empty());
  both.item = *ParseDungeonFindMatch("0x34", 0xFF);
  EXPECT_EQ(FindDungeonRows(room, both).size(), 2u);
}

TEST(DungeonPotItemNameTest, CoversLowAndHighIds) {
  EXPECT_EQ(handlers::DungeonPotItemName(0x08), "Key");
  EXPECT_EQ(handlers::DungeonPotItemName(0x16), "Nothing");  // trailing space
  EXPECT_EQ(handlers::DungeonPotItemName(0x88), "Switch");
  EXPECT_EQ(handlers::DungeonPotItemName(0x81), "Unknown 0x81");
}

TEST_F(DungeonFindCommandTest, DarkListsOnlyDarkHeaderRooms) {
  const auto json = RunFind({"--dark"});
  const auto& rows = json.at("rows");
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].at("room"), "0x055");
  EXPECT_EQ(rows[0].at("kind"), "header");
  EXPECT_EQ(rows[0].at("is_dark"), true);
  EXPECT_EQ(rows[0].at("spriteset"), "0x22");
  EXPECT_EQ(rows[0].at("tag1"), "0x08");
  EXPECT_EQ(json.at("summary").at("rooms_scanned"), zelda3::kNumberOfRooms);
}

TEST_F(DungeonFindCommandTest, KeyDropListsCarriersWithKind) {
  const auto json = RunFind({"--key-drop"});
  const auto& rows = json.at("rows");
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].at("room"), "0x072");
  EXPECT_EQ(rows[0].at("raw"), "0x8F");
  EXPECT_EQ(rows[0].at("x"), 5);
  EXPECT_EQ(rows[0].at("y"), 6);
  EXPECT_EQ(rows[0].at("key_drop_kind"), "small");
  EXPECT_EQ(rows[1].at("raw"), "0x41");
  EXPECT_EQ(rows[1].at("key_drop_kind"), "big");
}

TEST_F(DungeonFindCommandTest, SpriteIdListsEverySpriteWithThatId) {
  const auto json = RunFind({"--sprite=0x8F"});
  const auto& rows = json.at("rows");
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].at("index"), 0);
  EXPECT_EQ(rows[1].at("index"), 2);
  EXPECT_FALSE(rows[1].contains("key_drop"));
}

TEST_F(DungeonFindCommandTest, NoFilterIsAnError) {
  handlers::DungeonFindCommandHandler handler;
  std::string output;
  EXPECT_FALSE(handler.Run({"--format=json"}, &rom_, &output).ok());
}

TEST_F(DungeonFindCommandTest, RoomHeaderIsDarkMatchesByte0Bit0) {
  handlers::DungeonRoomHeaderCommandHandler handler;
  std::string output;
  ASSERT_TRUE(
      handler.Run({"--room=0x055", "--format=json"}, &rom_, &output).ok());
  auto decoded =
      nlohmann::json::parse(output).at("Room Header Debug").at("decoded");
  EXPECT_EQ(decoded.at("is_dark"), true);
  EXPECT_EQ(decoded.at("is_light"), true);  // deprecated alias, same bit

  output.clear();
  ASSERT_TRUE(
      handler.Run({"--room=0x056", "--format=json"}, &rom_, &output).ok());
  decoded = nlohmann::json::parse(output).at("Room Header Debug").at("decoded");
  EXPECT_EQ(decoded.at("is_dark"), false);
}

}  // namespace
}  // namespace yaze::cli
