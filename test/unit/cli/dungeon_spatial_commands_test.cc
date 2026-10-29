#include "cli/handlers/game/dungeon_spatial_commands.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "cli/service/command_registry.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "rom/snes.h"
#include "unit/zelda3/dungeon/room_census_test_rom.h"

namespace yaze::cli {
namespace {

using Json = nlohmann::json;
using ::testing::HasSubstr;

Json PlanReport(const std::string& output) {
  const Json value = Json::parse(output);
  if (value.contains("plan"))
    return value["plan"];
  for (const auto& [key, child] : value.items()) {
    if (child.is_object() && child.contains("plan"))
      return child["plan"];
  }
  ADD_FAILURE() << "Missing structured plan: " << output;
  return Json::object();
}

class DungeonSpatialCommandsTest : public testing::Test {
 protected:
  void SetUp() override {
    auto data = zelda3::test::BuildRoomCensusTestRom();
    const auto wall = zelda3::test::TestObjectBytes(4, 10, 10);
    const int layout_pc = SnesToPc(zelda3::kRoomLayoutPointers[0]);
    std::copy(wall.begin(), wall.end(), data.begin() + layout_pc);
    data[layout_pc + 3] = data[layout_pc + 4] = 0xFF;
    zelda3::test::SetTestRoom(
        data, 1, zelda3::test::kTestObjectDataPc + 0x200,
        zelda3::test::TestRoomStream({zelda3::test::TestObjectBytes(4, 20, 20)},
                                     {zelda3::test::TestDoorBytes(0, 0, 0)}),
        0x9100, std::array<uint8_t, 14>{});
    // The census fixture borrows PC008000 for header pointers, overlapping the
    // subtype1 art-offset table. Move that table before enabling object draws.
    constexpr int kHeaderTablePc = 0x078000;
    std::copy_n(data.begin() + SnesToPc(zelda3::test::kTestHeaderTableSnes),
                zelda3::kRoomCensusRoomCount * 2,
                data.begin() + kHeaderTablePc);
    const uint32_t header_table = PcToSnes(kHeaderTablePc);
    for (int byte = 0; byte < 3; ++byte) {
      data[zelda3::kRoomHeaderPointer + byte] =
          static_cast<uint8_t>(header_table >> (byte * 8));
    }
    std::fill_n(data.begin() + zelda3::kRoomObjectSubtype1, 0x200, 0);
    // Both 04 walls and C9 floors select a real eight-word pattern at offset0.
    for (int tile = 0; tile < 8; ++tile) {
      data[zelda3::kRoomObjectTileAddress + tile * 2] = tile + 1;
      data[zelda3::kRoomObjectTileAddress + tile * 2 + 1] = 0x20;
    }
    ASSERT_TRUE(rom_.LoadFromData(data).ok());
    path_ = std::filesystem::temp_directory_path() /
            ("spatial-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()) +
             ".json");
    std::ofstream output(path_);
    output
        << R"({"name":"Floor","description":"","category":"Test","objects":[)"
        << R"({"id":201,"rel_x":0,"rel_y":0,"size":0,"layer":0}]})";
    ASSERT_TRUE(output.good());
  }
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }
  absl::Status Run(int x, int y, const std::vector<std::string>& extra,
                   std::string* output) {
    std::vector<std::string> arguments = {
        "--room=0x01", "--template=" + path_.string(),
        "--x=" + std::to_string(x), "--y=" + std::to_string(y),
        "--format=json"};
    arguments.insert(arguments.end(), extra.begin(), extra.end());
    handlers::DungeonTemplatePlanCommandHandler handler;
    return handler.Run(arguments, &rom_, output);
  }
  Rom rom_;
  std::filesystem::path path_;
};

TEST_F(DungeonSpatialCommandsTest, RejectsWriteAndNonDecimalTile8Arguments) {
  const auto before = rom_.vector();
  handlers::DungeonTemplatePlanCommandHandler handler;
  for (const auto& extra : std::vector<std::vector<std::string>>{
           {"--x=1", "--y=1", "--write"},
           {"--x=23junk", "--y=1"},
           {"--x=1", "--y=1.5"},
           {"--x=0x10", "--y=1"},
           {"--x=1", "--y=1", "--allow-pairs=0:bad"}}) {
    std::vector<std::string> arguments = {"--room=0x01",
                                          "--template=" + path_.string()};
    arguments.insert(arguments.end(), extra.begin(), extra.end());
    const auto status = handler.Run(arguments, &rom_);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  }
  EXPECT_EQ(rom_.vector(), before);
}

TEST_F(DungeonSpatialCommandsTest, RegistryExportsPlannerAndSpatialHelp) {
  auto& registry = CommandRegistry::Instance();
  ASSERT_TRUE(registry.HasCommand("dungeon-template-plan"));
  EXPECT_THAT(registry.GenerateHelp("dungeon-template-plan"),
              HasSubstr("--x <tile8>"));
  EXPECT_THAT(registry.GenerateHelp("dungeon-template-plan"),
              HasSubstr("--allow-pairs"));
  const auto room_help = registry.GenerateHelp("dungeon-describe-room");
  EXPECT_THAT(room_help, HasSubstr("--spatial"));
  EXPECT_THAT(room_help, HasSubstr("--spatial-cells"));
  EXPECT_THAT(room_help, HasSubstr("--spatial-collision"));
  const Json schemas = Json::parse(registry.ExportFunctionSchemas());
  bool found = false;
  for (const auto& command : schemas.at("commands")) {
    if (command.at("name") == "dungeon-template-plan") {
      found = true;
      EXPECT_THAT(command.at("usage").get<std::string>(),
                  HasSubstr("--template"));
      EXPECT_THAT(command.at("description").get<std::string>(),
                  HasSubstr("Read-only"));
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(DungeonSpatialCommandsTest,
       LayoutPrefixBlocksAndExplainsExistingIndices) {
  const auto before = rom_.vector();
  const bool was_dirty = rom_.dirty();
  std::string output;
  const auto status = Run(10, 10, {}, &output);
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  const auto report = PlanReport(output);
  EXPECT_FALSE(report.at("valid").get<bool>());
  EXPECT_FALSE(report.at("write_performed").get<bool>());
  EXPECT_FALSE(report.at("runtime_qualified").get<bool>());
  ASSERT_EQ(report.at("footprints").size(), 1);
  EXPECT_EQ(report.at("footprints")[0].at("upper_write_cells"), 16);
  const auto& existing = report.at("existing_objects");
  ASSERT_EQ(existing.size(), 2);
  EXPECT_EQ(existing[0].at("source"), "layout");
  EXPECT_EQ(existing[0].at("source_index"), 0);
  EXPECT_EQ(existing[1].at("source"), "room");
  EXPECT_EQ(existing[1].at("source_index"), 0);
  bool layout_conflict = false;
  for (const auto& conflict : report.at("conflicts")) {
    if (conflict.at("kind") == "existing_overlap" &&
        conflict.value("existing_index", -1) == 0)
      layout_conflict = true;
  }
  EXPECT_TRUE(layout_conflict);
  EXPECT_EQ(rom_.vector(), before);
  EXPECT_EQ(rom_.dirty(), was_dirty);
}

TEST_F(DungeonSpatialCommandsTest,
       DoorReservationSurvivesExistingOverlapPermission) {
  const auto before = rom_.vector();
  std::string output;
  const auto status = Run(14, 4, {"--allow-existing=0,1"}, &output);
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  const auto report = PlanReport(output);
  ASSERT_EQ(report.at("footprints").size(), 1);
  EXPECT_EQ(report.at("footprints")[0].at("upper_write_cells"), 16);
  ASSERT_EQ(report.at("clearances").size(), 1);
  const auto& clearance = report.at("clearances")[0];
  EXPECT_EQ(clearance.at("source"), "door_editor_bounds");
  EXPECT_EQ(clearance.at("plane"), "both");
  EXPECT_EQ(clearance.at("bounds"),
            (Json{{"x", 14}, {"y", 4}, {"width", 4}, {"height", 3}}));
  bool blocked_by_door = false;
  for (const auto& conflict : report.at("conflicts")) {
    if (conflict.at("kind") == "clearance")
      blocked_by_door = true;
  }
  EXPECT_TRUE(blocked_by_door);
  EXPECT_EQ(rom_.vector(), before);
}

TEST_F(DungeonSpatialCommandsTest, ClearPlacementReturnsReadOnlyGeometryPlan) {
  const auto before = rom_.vector();
  std::string output;
  const auto status = Run(40, 40, {}, &output);
  ASSERT_TRUE(status.ok()) << status << "\n" << output;
  const auto report = PlanReport(output);
  EXPECT_TRUE(report.at("valid").get<bool>());
  EXPECT_FALSE(report.at("write_performed").get<bool>());
  ASSERT_EQ(report.at("placements").size(), 1);
  EXPECT_EQ(report.at("placements")[0].at("x"), 40);
  EXPECT_EQ(report.at("rom_content_sha256").get<std::string>().size(), 64);
  EXPECT_EQ(rom_.vector(), before);
}

}  // namespace
}  // namespace yaze::cli
