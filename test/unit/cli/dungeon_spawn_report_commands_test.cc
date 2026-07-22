#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/dungeon_graph_commands.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"

namespace yaze::cli {
namespace {

using ::testing::HasSubstr;

class DungeonSpawnReportCommandsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

    auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom_, kSpawnId);
    ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
    auto spawn = spawn_or.value();
    spawn.room_id = 0x01AB;
    spawn.camera_scroll_boundaries = {0x10, 0x11, 0x12, 0x13,
                                      0x14, 0x15, 0x16, 0x17};
    spawn.horizontal_scroll = 0x1234;
    spawn.vertical_scroll = 0x2345;
    spawn.y_coordinate = 0x3456;
    spawn.x_coordinate = 0x4567;
    spawn.camera_trigger_y = 0x5678;
    spawn.camera_trigger_x = 0x6789;
    spawn.main_gfx = 0x98;
    spawn.floor = 0xA9;
    spawn.dungeon_id = 0x1A;
    spawn.layer = 0x02;
    spawn.camera_scroll_controller = 0xBC;
    spawn.quadrant = 0x21;
    spawn.overworld_door_tilemap = 0xA5C3;
    spawn.entrance_id = 0x0084;
    spawn.song = 0xDE;
    ASSERT_TRUE(spawn.Save(&rom_, kSpawnId).ok());
  }

  void ExpectDedicatedSpawnSchema(const nlohmann::json& report) {
    EXPECT_EQ(report.at("spawn_id"), "0x06");
    EXPECT_TRUE(report.at("is_spawn_point"));
    EXPECT_EQ(report.at("room_id"), "0x01AB");
    EXPECT_EQ(report.at("dungeon_id"), "0x1A");
    EXPECT_EQ(report.at("quadrant"), "0x21");
    EXPECT_EQ(report.at("overworld_door_tilemap"), "0xA5C3");
    EXPECT_EQ(report.at("entrance_id"), "0x0084");
    EXPECT_EQ(report.at("position").at("x"), 0x4567);
    EXPECT_EQ(report.at("position").at("y"), 0x3456);
    EXPECT_EQ(report.at("camera").at("horizontal_scroll"), 0x1234);
    EXPECT_EQ(report.at("camera").at("vertical_scroll"), 0x2345);
    EXPECT_EQ(report.at("camera").at("trigger_x"), 0x6789);
    EXPECT_EQ(report.at("camera").at("trigger_y"), 0x5678);
    EXPECT_EQ(report.at("properties").at("main_gfx"), "0x98");
    EXPECT_EQ(report.at("properties").at("floor"), "0xA9");
    EXPECT_EQ(report.at("properties").at("layer"), "0x02");
    EXPECT_EQ(report.at("properties").at("camera_scroll_controller"), "0xBC");
    EXPECT_EQ(report.at("properties").at("song"), "0xDE");
    EXPECT_EQ(report.at("camera_boundaries").at("qn"), "0x10");
    EXPECT_EQ(report.at("camera_boundaries").at("fe"), "0x17");
    EXPECT_FALSE(report.contains("exit_id"));
    EXPECT_FALSE(report.contains("door"));
    EXPECT_FALSE(report.at("properties").contains("door"));
  }

  static constexpr int kSpawnId = 0x06;
  Rom rom_;
};

TEST_F(DungeonSpawnReportCommandsTest,
       DungeonGetEntranceReportsDedicatedSpawnSchema) {
  handlers::DungeonGetEntranceCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--entrance=0x06", "--spawn", "--format=json"}, &rom_, &output);

  ASSERT_TRUE(status.ok()) << status;
  ExpectDedicatedSpawnSchema(nlohmann::json::parse(output));
}

TEST_F(DungeonSpawnReportCommandsTest,
       EntranceInfoReportsDedicatedSpawnSchema) {
  handlers::EntranceInfoCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--entrance=0x06", "--spawn", "--format=json"}, &rom_, &output);

  ASSERT_TRUE(status.ok()) << status;
  const auto json = nlohmann::json::parse(output);
  ExpectDedicatedSpawnSchema(json.at("entrance"));
}

TEST_F(DungeonSpawnReportCommandsTest, DungeonGetEntranceRejectsSpawnIdSeven) {
  handlers::DungeonGetEntranceCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--entrance=0x07", "--spawn", "--format=json"}, &rom_, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(status.message()), HasSubstr("0x00-0x06"));
  EXPECT_NO_THROW(nlohmann::json::parse(output));
}

TEST_F(DungeonSpawnReportCommandsTest, EntranceInfoRejectsSpawnIdSeven) {
  handlers::EntranceInfoCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--entrance=0x07", "--spawn", "--format=json"}, &rom_, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(status.message()), HasSubstr("0x00-0x06"));
  EXPECT_NO_THROW(nlohmann::json::parse(output));
}

}  // namespace
}  // namespace yaze::cli
