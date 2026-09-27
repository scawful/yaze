// z3ed dungeon-room-census: same model as the Room Matrix census overlay.

#include "cli/handlers/game/dungeon_census_commands.h"

#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rom/rom.h"
#include "unique_temp_path.h"
#include "unit/zelda3/dungeon/room_census_test_rom.h"
#include "zelda3/dungeon/room_census.h"

namespace yaze::cli {
namespace {

namespace fs = std::filesystem;
using ::testing::HasSubstr;

nlohmann::json Unwrap(const nlohmann::json& json) {
  // Handlers may wrap output in a titled object.
  if (json.contains("counts")) {
    return json;
  }
  for (const auto& [key, value] : json.items()) {
    if (value.is_object() && value.contains("counts")) {
      return value;
    }
  }
  return json;
}

class DungeonRoomCensusCommandTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(zelda3::test::BuildRoomCensusTestRom()).ok());
  }
  Rom rom_;
};

TEST_F(DungeonRoomCensusCommandTest, JsonMatchesModel) {
  handlers::DungeonRoomCensusCommandHandler handler;
  std::string output;
  const auto status = handler.Run({"--format=json"}, &rom_, &output);
  ASSERT_TRUE(status.ok()) << status;
  const auto json = Unwrap(nlohmann::json::parse(output));

  auto census = zelda3::ComputeRoomCensus(&rom_);
  ASSERT_TRUE(census.ok());
  EXPECT_EQ(json.at("counts").at("free"), census->free_count);
  EXPECT_EQ(json.at("counts").at("reclaimable"), census->reclaimable_count);
  EXPECT_EQ(json.at("counts").at("free"), zelda3::kRoomCensusRoomCount - 4);
  EXPECT_EQ(json.at("rooms").size(),
            static_cast<size_t>(zelda3::kRoomCensusRoomCount));
  EXPECT_EQ(json.at("largest_free_block"),
            census->free_clusters.front().summary);
  EXPECT_EQ(json.at("owners_source"), "derived_from_entrances");
}

TEST_F(DungeonRoomCensusCommandTest, RoomFilterShowsHighPageWarp) {
  handlers::DungeonRoomCensusCommandHandler handler;
  std::string output;
  ASSERT_TRUE(
      handler.Run({"--room=0x105", "--format=json"}, &rom_, &output).ok());
  const auto json = Unwrap(nlohmann::json::parse(output));
  ASSERT_EQ(json.at("rooms").size(), 1u);
  const auto& room = json.at("rooms").at(0);
  EXPECT_EQ(room.at("room"), "0x105");
  EXPECT_EQ(room.at("status"), "in_use");
  EXPECT_TRUE(room.at("reached"));
  bool warp_from_104 = false;
  for (const auto& ref : room.at("references")) {
    warp_from_104 |= ref.at("kind") == "holewarp" && ref.at("from") == "0x104";
  }
  EXPECT_TRUE(warp_from_104);
}

TEST_F(DungeonRoomCensusCommandTest, StatusFilterAndTable) {
  handlers::DungeonRoomCensusCommandHandler handler;
  std::string output;
  ASSERT_TRUE(
      handler.Run({"--status=in_use", "--format=json"}, &rom_, &output).ok());
  const auto json = Unwrap(nlohmann::json::parse(output));
  std::set<std::string> rooms;
  for (const auto& room : json.at("rooms")) {
    EXPECT_EQ(room.at("status"), "in_use");
    rooms.insert(room.at("room"));
  }
  EXPECT_EQ(rooms, (std::set<std::string>{"0x00", "0x04", "0x104", "0x105"}));

  std::string table;
  ASSERT_TRUE(
      handler.Run({"--status=free", "--format=table"}, &rom_, &table).ok());
  EXPECT_THAT(table, HasSubstr("Room census: 292 free"));
  EXPECT_THAT(table, HasSubstr("Largest free block: Row"));
  EXPECT_THAT(table, HasSubstr("0x93   free"));

  EXPECT_FALSE(handler.Run({"--status=bogus"}, &rom_, &output).ok());
}

// Oracle copy + the fixture dungeons.json as a project folder: the CLI gives
// the same free/reclaimable answer as the model test in
// room_census_oracle_rom_test.cc.
TEST(DungeonRoomCensusOracleCommandTest, ProjectFolderMatchesOracleNotes) {
  const char* rom_path = std::getenv("YAZE_TEST_ROM_OOS");
  if (rom_path == nullptr || !fs::exists(rom_path)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to a copy of oos168.sfc";
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path).ok());
  const fs::path project = yaze::test::UniqueTempPath("room_census_project");
  fs::create_directories(project / "Docs" / "Dev" / "Planning");
  fs::copy_file(fs::path(YAZE_TEST_FIXTURE_DIR) / "room_census" /
                    "oracle_dungeon_owners.json",
                project / "Docs" / "Dev" / "Planning" / "dungeons.json");

  handlers::DungeonRoomCensusCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--project=" + project.string(), "--format=json"}, &rom, &output);
  fs::remove_all(project);
  ASSERT_TRUE(status.ok()) << status;
  const auto json = Unwrap(nlohmann::json::parse(output));
  EXPECT_EQ(json.at("owners_source"), "project");
  EXPECT_EQ(json.at("reclaimable_rooms"),
            nlohmann::json({"0x01", "0x10", "0x30", "0xA7", "0x106", "0x127"}));
  EXPECT_EQ(json.at("counts").at("free"), 14);
  EXPECT_EQ(json.at("largest_free_block"),
            "Row 9: 0x93-0x96 + 0xA0/0xA6/0xB0, 7 rooms");
}

}  // namespace
}  // namespace yaze::cli
