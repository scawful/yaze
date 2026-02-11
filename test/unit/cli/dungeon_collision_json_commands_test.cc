#include "cli/handlers/game/dungeon_collision_commands.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::cli {
namespace {

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::in | std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

void WriteFile(const std::filesystem::path& path, const std::string& contents) {
  std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
  out << contents;
}

TEST(DungeonCollisionJsonCommandsTest, CustomCollisionImportExportRoundTrip) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  const auto in_path = std::filesystem::temp_directory_path() /
                       "yaze_custom_collision_import_test.json";
  const auto out_path = std::filesystem::temp_directory_path() /
                        "yaze_custom_collision_export_test.json";

  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"rooms\": [\n"
            "    { \"room_id\": \"0x25\", \"tiles\": [ [65, 8], [66, \"0xB7\"] ] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportCustomCollisionJsonCommandHandler import_handler;
  std::string import_output;
  auto import_status =
      import_handler.Run({"--in", in_path.string(), "--format=json"}, &rom,
                         &import_output);
  ASSERT_TRUE(import_status.ok()) << import_status.message();

  handlers::DungeonExportCustomCollisionJsonCommandHandler export_handler;
  std::string export_output;
  auto export_status =
      export_handler.Run({"--room=0x25", "--out", out_path.string(),
                          "--format=json"},
                         &rom, &export_output);
  ASSERT_TRUE(export_status.ok()) << export_status.message();

  auto rooms_or =
      zelda3::LoadCustomCollisionRoomsFromJsonString(ReadFile(out_path));
  ASSERT_TRUE(rooms_or.ok()) << rooms_or.status().message();
  ASSERT_EQ(rooms_or->size(), 1u);
  EXPECT_EQ((*rooms_or)[0].room_id, 0x25);
  ASSERT_EQ((*rooms_or)[0].tiles.size(), 2u);
  EXPECT_EQ((*rooms_or)[0].tiles[0].offset, 65);
  EXPECT_EQ((*rooms_or)[0].tiles[0].value, 8);
  EXPECT_EQ((*rooms_or)[0].tiles[1].offset, 66);
  EXPECT_EQ((*rooms_or)[0].tiles[1].value, 0xB7);

  std::filesystem::remove(in_path);
  std::filesystem::remove(out_path);
}

TEST(DungeonCollisionJsonCommandsTest,
     CustomCollisionImportFailsWithoutExpandedCollisionRegion) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0x00)).ok());

  const auto in_path = std::filesystem::temp_directory_path() /
                       "yaze_custom_collision_import_fail.json";
  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"rooms\": [\n"
            "    { \"room_id\": \"0x25\", \"tiles\": [ [65, 8] ] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const auto status = handler.Run({"--in", in_path.string(), "--format=json"},
                                  &rom, &output);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  std::filesystem::remove(in_path);
}

TEST(DungeonCollisionJsonCommandsTest, WaterFillImportNormalizesMasks) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  const auto in_path = std::filesystem::temp_directory_path() /
                       "yaze_water_fill_import_test.json";
  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"zones\": [\n"
            "    { \"room_id\": \"0x27\", \"mask\": \"0x01\", \"offsets\": [100] },\n"
            "    { \"room_id\": \"0x25\", \"mask\": \"0x01\", \"offsets\": [200] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--in", in_path.string(), "--format=json"}, &rom, &output);
  ASSERT_TRUE(status.ok()) << status.message();

  auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  ASSERT_EQ(zones_or->size(), 2u);
  EXPECT_EQ((*zones_or)[0].room_id, 0x25);
  EXPECT_EQ((*zones_or)[0].sram_bit_mask, 0x01);
  EXPECT_EQ((*zones_or)[1].room_id, 0x27);
  EXPECT_EQ((*zones_or)[1].sram_bit_mask, 0x02);

  std::filesystem::remove(in_path);
}

TEST(DungeonCollisionJsonCommandsTest, WaterFillExportRespectsRoomFilter) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  std::vector<zelda3::WaterFillZoneEntry> zones;
  zones.push_back(zelda3::WaterFillZoneEntry{
      .room_id = 0x25, .sram_bit_mask = 0x01, .fill_offsets = {1, 2, 3}});
  zones.push_back(zelda3::WaterFillZoneEntry{
      .room_id = 0x27, .sram_bit_mask = 0x02, .fill_offsets = {10, 11}});
  ASSERT_TRUE(zelda3::WriteWaterFillTable(&rom, zones).ok());

  const auto out_path = std::filesystem::temp_directory_path() /
                        "yaze_water_fill_export_test.json";
  handlers::DungeonExportWaterFillJsonCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--room=0x27", "--out", out_path.string(), "--format=json"},
                  &rom, &output);
  ASSERT_TRUE(status.ok()) << status.message();

  auto exported_or =
      zelda3::LoadWaterFillZonesFromJsonString(ReadFile(out_path));
  ASSERT_TRUE(exported_or.ok()) << exported_or.status().message();
  ASSERT_EQ(exported_or->size(), 1u);
  EXPECT_EQ((*exported_or)[0].room_id, 0x27);
  EXPECT_EQ((*exported_or)[0].sram_bit_mask, 0x02);

  std::filesystem::remove(out_path);
}

}  // namespace
}  // namespace yaze::cli
