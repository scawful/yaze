#include "cli/handlers/game/dungeon_collision_commands.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
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

void SeedCustomCollisionRooms(Rom* rom, const std::vector<int>& room_ids) {
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto in_path = std::filesystem::temp_directory_path() /
                       ("yaze_seed_required_collision_" +
                        std::to_string(nonce) + ".json");

  nlohmann::json payload;
  payload["version"] = 1;
  payload["rooms"] = nlohmann::json::array();
  for (int room_id : room_ids) {
    payload["rooms"].push_back({
        {"room_id", room_id},
        {"tiles", nlohmann::json::array({nlohmann::json::array({100, 184})})},
    });
  }

  WriteFile(in_path, payload.dump(2) + "\n");

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--in", in_path.string(), "--format=json"}, rom, &output);
  ASSERT_TRUE(status.ok()) << status.message();

  std::filesystem::remove(in_path);
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
  SeedCustomCollisionRooms(&rom, {0x25, 0x27});

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

TEST(DungeonCollisionJsonCommandsTest, CustomCollisionImportDryRunDoesNotWrite) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  const auto in_path = std::filesystem::temp_directory_path() /
                       "yaze_custom_collision_import_dry_run.json";
  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"rooms\": [\n"
            "    { \"room_id\": \"0x25\", \"tiles\": [ [65, \"0xB7\"] ] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--in", in_path.string(), "--dry-run", "--format=json"}, &rom, &output);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(output, testing::HasSubstr("\"mode\": \"dry-run\""));

  auto map_or = zelda3::LoadCustomCollisionMap(&rom, 0x25);
  ASSERT_TRUE(map_or.ok()) << map_or.status().message();
  EXPECT_FALSE(map_or->has_data);

  std::filesystem::remove(in_path);
}

TEST(DungeonCollisionJsonCommandsTest, CustomCollisionReplaceAllRequiresForce) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  const auto in_path = std::filesystem::temp_directory_path() /
                       "yaze_custom_collision_replace_all_requires_force.json";
  const auto report_path =
      std::filesystem::temp_directory_path() /
      "yaze_custom_collision_replace_all_requires_force.report.json";
  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"rooms\": [\n"
            "    { \"room_id\": \"0x25\", \"tiles\": [ [65, \"0xB7\"] ] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--in", in_path.string(), "--replace-all",
                   "--report", report_path.string(), "--format=json"},
                  &rom, &output);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  const auto report = nlohmann::json::parse(ReadFile(report_path));
  EXPECT_EQ(report.value("command", ""), "dungeon-import-custom-collision-json");
  EXPECT_EQ(report.value("status", ""), "error");
  EXPECT_TRUE(report.value("replace_all", false));
  EXPECT_FALSE(report.value("force", true));
  EXPECT_EQ(report["error"].value("code", ""), "FAILED_PRECONDITION");

  std::filesystem::remove(in_path);
  std::filesystem::remove(report_path);
}

TEST(DungeonCollisionJsonCommandsTest, WaterFillImportDryRunDoesNotWrite) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  SeedCustomCollisionRooms(&rom, {0x27});

  const auto in_path =
      std::filesystem::temp_directory_path() / "yaze_water_fill_dry_run.json";
  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"zones\": [\n"
            "    { \"room_id\": \"0x27\", \"mask\": \"0x01\", \"offsets\": [100] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--in", in_path.string(), "--dry-run", "--format=json"}, &rom, &output);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_THAT(output, testing::HasSubstr("\"mode\": \"dry-run\""));

  auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  EXPECT_TRUE(zones_or->empty());

  std::filesystem::remove(in_path);
}

TEST(DungeonCollisionJsonCommandsTest,
     WaterFillImportFailsWhenRequiredD4RoomHasNoCollisionData) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  const auto in_path =
      std::filesystem::temp_directory_path() / "yaze_water_fill_required_room.json";
  const auto report_path = std::filesystem::temp_directory_path() /
                           "yaze_water_fill_required_room.report.json";
  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"zones\": [\n"
            "    { \"room_id\": \"0x25\", \"mask\": \"0x01\", \"offsets\": [100] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--in", in_path.string(), "--dry-run",
                   "--report", report_path.string(), "--format=json"},
                  &rom, &output);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  const auto report = nlohmann::json::parse(ReadFile(report_path));
  ASSERT_TRUE(report.contains("preflight"));
  ASSERT_TRUE(report["preflight"].contains("errors"));

  bool found_required_room_error = false;
  for (const auto& err : report["preflight"]["errors"]) {
    if (err.value("code", "") == "ORACLE_REQUIRED_ROOM_MISSING_COLLISION") {
      found_required_room_error = true;
      break;
    }
  }
  EXPECT_TRUE(found_required_room_error);

  std::filesystem::remove(in_path);
  std::filesystem::remove(report_path);
}

TEST(DungeonCollisionJsonCommandsTest,
     WaterFillImportStrictMasksFailsAndWritesReport) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  SeedCustomCollisionRooms(&rom, {0x25, 0x27});

  const auto in_path =
      std::filesystem::temp_directory_path() / "yaze_water_fill_strict.json";
  const auto report_path =
      std::filesystem::temp_directory_path() / "yaze_water_fill_strict.report.json";
  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"zones\": [\n"
            "    { \"room_id\": \"0x25\", \"mask\": \"0x01\", \"offsets\": [10] },\n"
            "    { \"room_id\": \"0x27\", \"mask\": \"0x01\", \"offsets\": [20] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--in", in_path.string(), "--strict-masks",
                   "--report", report_path.string(), "--format=json"},
                  &rom, &output);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  const auto report = nlohmann::json::parse(ReadFile(report_path));
  EXPECT_EQ(report.value("command", ""), "dungeon-import-water-fill-json");
  EXPECT_EQ(report.value("status", ""), "error");
  EXPECT_EQ(report.value("normalized_masks", 0), 1);
  EXPECT_TRUE(report.value("strict_masks", false));
  EXPECT_EQ(report["error"].value("code", ""), "FAILED_PRECONDITION");

  std::filesystem::remove(in_path);
  std::filesystem::remove(report_path);
}

TEST(DungeonCollisionJsonCommandsTest,
     WaterFillImportFailsPreflightOnDuplicateExistingMasks) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  std::vector<uint8_t> region(
      static_cast<size_t>(zelda3::kWaterFillTableReservedSize), 0x00);
  region[0] = 2;      // zone_count
  region[1] = 0x25;   // room 0
  region[2] = 0x01;   // mask 0 (duplicate)
  region[3] = 0x09;   // data_off lo
  region[4] = 0x00;   // data_off hi
  region[5] = 0x27;   // room 1
  region[6] = 0x01;   // mask 1 (duplicate)
  region[7] = 0x0C;   // data_off lo
  region[8] = 0x00;   // data_off hi
  region[9] = 0x01;   // count
  region[10] = 0x64;  // offset lo
  region[11] = 0x00;  // offset hi
  region[12] = 0x01;  // count
  region[13] = 0xC8;  // offset lo
  region[14] = 0x00;  // offset hi
  ASSERT_TRUE(rom.WriteVector(zelda3::kWaterFillTableStart, region).ok());

  const auto in_path =
      std::filesystem::temp_directory_path() / "yaze_water_fill_preflight.json";
  const auto report_path = std::filesystem::temp_directory_path() /
                           "yaze_water_fill_preflight.report.json";
  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"zones\": [\n"
            "    { \"room_id\": \"0x25\", \"mask\": \"0x01\", \"offsets\": [32] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--in", in_path.string(), "--dry-run",
                   "--report", report_path.string(), "--format=json"},
                  &rom, &output);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  const auto report = nlohmann::json::parse(ReadFile(report_path));
  EXPECT_EQ(report.value("status", ""), "error");
  ASSERT_TRUE(report.contains("preflight"));
  EXPECT_FALSE(report["preflight"].value("ok", true));
  ASSERT_TRUE(report["preflight"].contains("errors"));
  ASSERT_FALSE(report["preflight"]["errors"].empty());
  EXPECT_EQ(report["preflight"]["errors"][0].value("code", ""),
            "ORACLE_WATER_FILL_TABLE_INVALID");

  std::filesystem::remove(in_path);
  std::filesystem::remove(report_path);
}

// ---------------------------------------------------------------------------
// D4 Zora Temple water-profile tests
//
// D4 has two water-fill zones: room 0x25 (Water Grate, mask 0x02) and
// room 0x27 (Water Gate, mask 0x01). These tests verify the profile constraint:
// both zones must round-trip correctly and must receive unique, deterministic
// SRAM bit masks.
// ---------------------------------------------------------------------------

TEST(DungeonCollisionJsonCommandsTest,
     ZoraTempleProfileBothRoomsRoundtrip) {
  // Write both D4 zones with correct masks, then export+verify both survive.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  SeedCustomCollisionRooms(&rom, {0x25, 0x27});

  const auto in_path = std::filesystem::temp_directory_path() /
                       "yaze_d4_profile_import.json";
  const auto out_path = std::filesystem::temp_directory_path() /
                        "yaze_d4_profile_export.json";

  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"zones\": [\n"
            "    { \"room_id\": \"0x25\", \"mask\": \"0x02\", \"offsets\": [100, 101] },\n"
            "    { \"room_id\": \"0x27\", \"mask\": \"0x01\", \"offsets\": [200, 201] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler import_handler;
  std::string import_output;
  ASSERT_TRUE(
      import_handler
          .Run({"--in", in_path.string(), "--format=json"}, &rom, &import_output)
          .ok());

  handlers::DungeonExportWaterFillJsonCommandHandler export_handler;
  std::string export_output;
  ASSERT_TRUE(
      export_handler
          .Run({"--rooms=0x25,0x27", "--out", out_path.string(), "--format=json"},
               &rom, &export_output)
          .ok());

  auto zones_or =
      zelda3::LoadWaterFillZonesFromJsonString(ReadFile(out_path));
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  ASSERT_EQ(zones_or->size(), 2u);

  // Both rooms must be present with their correct masks.
  bool found_25 = false;
  bool found_27 = false;
  for (const auto& zone : *zones_or) {
    if (zone.room_id == 0x25) {
      EXPECT_EQ(zone.sram_bit_mask, 0x02u);
      ASSERT_EQ(zone.fill_offsets.size(), 2u);
      EXPECT_EQ(zone.fill_offsets[0], 100u);
      found_25 = true;
    } else if (zone.room_id == 0x27) {
      EXPECT_EQ(zone.sram_bit_mask, 0x01u);
      ASSERT_EQ(zone.fill_offsets.size(), 2u);
      EXPECT_EQ(zone.fill_offsets[0], 200u);
      found_27 = true;
    }
  }
  EXPECT_TRUE(found_25) << "Room 0x25 (Water Grate) missing from export";
  EXPECT_TRUE(found_27) << "Room 0x27 (Water Gate) missing from export";

  std::filesystem::remove(in_path);
  std::filesystem::remove(out_path);
}

TEST(DungeonCollisionJsonCommandsTest,
     ZoraTempleProfileMaskNormalizationAssignsUniquePerRoom) {
  // Both D4 zones with mask=0 (auto) must receive unique, non-zero masks.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  SeedCustomCollisionRooms(&rom, {0x25, 0x27});

  const auto in_path = std::filesystem::temp_directory_path() /
                       "yaze_d4_mask_norm.json";
  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"zones\": [\n"
            "    { \"room_id\": \"0x27\", \"mask\": \"0x00\", \"offsets\": [50] },\n"
            "    { \"room_id\": \"0x25\", \"mask\": \"0x00\", \"offsets\": [60] }\n"
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

  // Normalization is deterministic: masks are reassigned in ascending room_id
  // order, so room 0x25 gets 0x01 and room 0x27 gets 0x02.
  EXPECT_EQ((*zones_or)[0].room_id, 0x25);
  EXPECT_EQ((*zones_or)[0].sram_bit_mask, 0x01u);
  EXPECT_EQ((*zones_or)[1].room_id, 0x27);
  EXPECT_EQ((*zones_or)[1].sram_bit_mask, 0x02u);

  std::filesystem::remove(in_path);
}

TEST(DungeonCollisionJsonCommandsTest,
     ZoraTempleProfileExportRespectsSingleRoomFilter) {
  // Verify that exporting only room 0x25 does not include room 0x27.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  std::vector<zelda3::WaterFillZoneEntry> zones;
  zones.push_back({.room_id = 0x25, .sram_bit_mask = 0x02, .fill_offsets = {1, 2}});
  zones.push_back({.room_id = 0x27, .sram_bit_mask = 0x01, .fill_offsets = {3, 4}});
  ASSERT_TRUE(zelda3::WriteWaterFillTable(&rom, zones).ok());

  const auto out_path = std::filesystem::temp_directory_path() /
                        "yaze_d4_single_room_export.json";
  handlers::DungeonExportWaterFillJsonCommandHandler handler;
  std::string output;
  ASSERT_TRUE(
      handler.Run({"--room=0x25", "--out", out_path.string(), "--format=json"},
                  &rom, &output)
          .ok());

  auto exported_or =
      zelda3::LoadWaterFillZonesFromJsonString(ReadFile(out_path));
  ASSERT_TRUE(exported_or.ok()) << exported_or.status().message();
  ASSERT_EQ(exported_or->size(), 1u);
  EXPECT_EQ((*exported_or)[0].room_id, 0x25);
  EXPECT_EQ((*exported_or)[0].sram_bit_mask, 0x02u);

  std::filesystem::remove(out_path);
}

}  // namespace
}  // namespace yaze::cli
