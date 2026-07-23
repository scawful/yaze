#include "cli/handlers/game/dungeon_collision_commands.h"

#include <algorithm>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "cli/service/rom/rom_sandbox_manager.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/track_collision_generator.h"
#include "zelda3/dungeon/water_fill_zone.h"

#if defined(__APPLE__)
#include <sys/stat.h>
#endif

ABSL_DECLARE_FLAG(bool, sandbox);

namespace yaze::cli {
namespace {

class ScopedSandboxFlag {
 public:
  explicit ScopedSandboxFlag(bool value)
      : original_(absl::GetFlag(FLAGS_sandbox)) {
    absl::SetFlag(&FLAGS_sandbox, value);
  }
  ~ScopedSandboxFlag() { absl::SetFlag(&FLAGS_sandbox, original_); }

 private:
  bool original_;
};

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::in | std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

void WriteFile(const std::filesystem::path& path, const std::string& contents) {
  std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
  out << contents;
}

std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::in | std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                              std::istreambuf_iterator<char>());
}

void WriteRomFile(const Rom& rom, const std::filesystem::path& path) {
  std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom.data()),
            static_cast<std::streamsize>(rom.size()));
  ASSERT_TRUE(out.good());
}

class ScopedTempDirectory {
 public:
  explicit ScopedTempDirectory(const char* label) {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("yaze_collision_json_" + std::string(label) + "_" +
             std::to_string(nonce));
    std::error_code ec;
    EXPECT_TRUE(std::filesystem::create_directories(path_, ec)) << ec.message();
  }

  ~ScopedTempDirectory() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

#if defined(__APPLE__)
class ScopedImmutableFile {
 public:
  explicit ScopedImmutableFile(const std::filesystem::path& path)
      : path_(path), enabled_(::chflags(path_.c_str(), UF_IMMUTABLE) == 0) {}

  ~ScopedImmutableFile() {
    if (enabled_) {
      ::chflags(path_.c_str(), 0);
    }
  }

  bool enabled() const { return enabled_; }

 private:
  std::filesystem::path path_;
  bool enabled_;
};
#endif

class ScopedSandboxRoot {
 public:
  explicit ScopedSandboxRoot(const std::filesystem::path& root)
      : original_root_(RomSandboxManager::Instance().RootDirectory()) {
    RomSandboxManager::Instance().SetRootDirectory(root);
  }

  ~ScopedSandboxRoot() {
    for (const auto& sandbox_id : sandbox_ids_) {
      RomSandboxManager::Instance().RemoveSandbox(sandbox_id).IgnoreError();
    }
    RomSandboxManager::Instance().SetRootDirectory(original_root_);
  }

  void Track(std::string sandbox_id) {
    sandbox_ids_.push_back(std::move(sandbox_id));
  }

 private:
  std::filesystem::path original_root_;
  std::vector<std::string> sandbox_ids_;
};

class CoordinatedCollisionExportHandler final
    : public handlers::DungeonExportCustomCollisionJsonCommandHandler {
 public:
  explicit CoordinatedCollisionExportHandler(std::barrier<>* rendezvous)
      : rendezvous_(rendezvous) {}

  absl::Status ExecuteWithContext(
      Rom* rom, const resources::ArgumentParser& parser,
      resources::OutputFormatter& formatter,
      const resources::CommandInvocationContext& invocation_context) override {
    rendezvous_->arrive_and_wait();

    const auto expected_source = parser.GetString("expected-source");
    if (!expected_source.has_value() ||
        !invocation_context.source_rom_path.has_value()) {
      return absl::FailedPreconditionError(
          "Missing command-scoped sandbox source identity");
    }

    std::error_code expected_ec;
    std::error_code actual_ec;
    const auto expected = std::filesystem::weakly_canonical(
        std::filesystem::path(*expected_source), expected_ec);
    const auto actual = std::filesystem::weakly_canonical(
        *invocation_context.source_rom_path, actual_ec);
    if (expected_ec || actual_ec || expected != actual) {
      return absl::FailedPreconditionError(
          "Command borrowed another sandbox invocation's source identity");
    }

    return handlers::DungeonExportCustomCollisionJsonCommandHandler::
        ExecuteWithContext(rom, parser, formatter, invocation_context);
  }

 private:
  std::barrier<>* rendezvous_;
};

std::vector<std::filesystem::path> FindBackupArtifacts(
    const std::filesystem::path& rom_path) {
  std::vector<std::filesystem::path> backups;
  std::error_code ec;
  const std::string prefix = rom_path.filename().string() + "_backup_";
  for (const auto& entry :
       std::filesystem::directory_iterator(rom_path.parent_path(), ec)) {
    if (ec || !entry.is_regular_file()) {
      continue;
    }
    if (entry.path().filename().string().rfind(prefix, 0) == 0) {
      backups.push_back(entry.path());
    }
  }
  std::sort(backups.begin(), backups.end());
  return backups;
}

void SetLong(Rom* rom, int address, uint32_t value) {
  ASSERT_TRUE(rom->WriteByte(address, value & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(address + 1, (value >> 8) & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(address + 2, (value >> 16) & 0xFF).ok());
}

void WriteSingleCollisionImport(const std::filesystem::path& path) {
  WriteFile(path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"rooms\": [\n"
            "    { \"room_id\": \"0x25\", \"tiles\": [ [65, \"0xB7\"] ] }\n"
            "  ]\n"
            "}\n");
}

void WriteSingleWaterFillImport(const std::filesystem::path& path) {
  WriteFile(
      path,
      "{\n"
      "  \"version\": 1,\n"
      "  \"zones\": [\n"
      "    { \"room_id\": \"0x30\", \"mask\": \"0x01\", \"offsets\": [100] }\n"
      "  ]\n"
      "}\n");
}

void SeedCustomCollisionRooms(Rom* rom, const std::vector<int>& room_ids) {
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto in_path =
      std::filesystem::temp_directory_path() /
      ("yaze_seed_required_collision_" + std::to_string(nonce) + ".json");

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
  const auto status = handler.Run(
      {"--in", in_path.string(), "--mock-rom", "--format=json"}, rom, &output);
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

  WriteFile(
      in_path,
      "{\n"
      "  \"version\": 1,\n"
      "  \"rooms\": [\n"
      "    { \"room_id\": \"0x25\", \"tiles\": [ [65, 8], [66, \"0xB7\"] ] }\n"
      "  ]\n"
      "}\n");

  handlers::DungeonImportCustomCollisionJsonCommandHandler import_handler;
  std::string import_output;
  auto import_status = import_handler.Run(
      {"--in", in_path.string(), "--mock-rom", "--format=json"}, &rom,
      &import_output);
  ASSERT_TRUE(import_status.ok()) << import_status.message();

  handlers::DungeonExportCustomCollisionJsonCommandHandler export_handler;
  std::string export_output;
  auto export_status = export_handler.Run(
      {"--room=0x25", "--out", out_path.string(), "--format=json"}, &rom,
      &export_output);
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
  const auto status =
      handler.Run({"--in", in_path.string(), "--format=json"}, &rom, &output);
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
  WriteFile(
      in_path,
      "{\n"
      "  \"version\": 1,\n"
      "  \"zones\": [\n"
      "    { \"room_id\": \"0x27\", \"mask\": \"0x01\", \"offsets\": [100] },\n"
      "    { \"room_id\": \"0x25\", \"mask\": \"0x01\", \"offsets\": [200] }\n"
      "  ]\n"
      "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--in", in_path.string(), "--mock-rom", "--format=json"}, &rom, &output);
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

TEST(DungeonCollisionJsonCommandsTest,
     CustomCollisionImportDryRunDoesNotWrite) {
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

TEST(DungeonCollisionJsonCommandsTest,
     CustomCollisionImportPersistsBackupAndReopens) {
  ScopedTempDirectory temp("custom_reopen");
  const auto rom_path = temp.path() / "target.sfc";
  const auto in_path = temp.path() / "collision.json";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  zelda3::CustomCollisionMap preserved_map;
  preserved_map.has_data = true;
  preserved_map.tiles[130] = 0xBA;
  ASSERT_TRUE(zelda3::WriteTrackCollision(&rom, 0x26, preserved_map).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);
  const std::vector<uint8_t> disk_before = ReadFileBytes(rom_path);

  WriteFile(
      in_path,
      "{\n"
      "  \"version\": 1,\n"
      "  \"rooms\": [\n"
      "    { \"room_id\": \"0x25\", \"tiles\": [ [65, \"0xB7\"], [66, 8] ] }\n"
      "  ]\n"
      "}\n");

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--in", in_path.string(), "--format=json"}, &rom, &output);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, testing::HasSubstr("\"write_status\": \"success\""));
  EXPECT_THAT(output, testing::HasSubstr("\"save_status\": \"saved\""));
  EXPECT_FALSE(rom.dirty());
  EXPECT_NE(ReadFileBytes(rom_path), disk_before);

  const auto backups = FindBackupArtifacts(rom_path);
  ASSERT_EQ(backups.size(), 1u);
  EXPECT_EQ(ReadFileBytes(backups.front()), disk_before);

  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(rom_path.string()).ok());
  auto map_or = zelda3::LoadCustomCollisionMap(&reopened, 0x25);
  ASSERT_TRUE(map_or.ok()) << map_or.status();
  ASSERT_TRUE(map_or->has_data);
  EXPECT_EQ(map_or->tiles[65], 0xB7);
  EXPECT_EQ(map_or->tiles[66], 8);

  auto preserved_or = zelda3::LoadCustomCollisionMap(&reopened, 0x26);
  ASSERT_TRUE(preserved_or.ok()) << preserved_or.status();
  ASSERT_TRUE(preserved_or->has_data);
  EXPECT_EQ(preserved_or->tiles[130], 0xBA);
}

TEST(DungeonCollisionJsonCommandsTest,
     WaterFillImportPersistsNormalizedTableBackupAndReopens) {
  ScopedTempDirectory temp("water_reopen");
  const auto rom_path = temp.path() / "target.sfc";
  const auto in_path = temp.path() / "water.json";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);
  const std::vector<uint8_t> disk_before = ReadFileBytes(rom_path);

  WriteFile(
      in_path,
      "{\n"
      "  \"version\": 1,\n"
      "  \"zones\": [\n"
      "    { \"room_id\": \"0x31\", \"mask\": \"0x01\", \"offsets\": [200] },\n"
      "    { \"room_id\": \"0x30\", \"mask\": \"0x01\", \"offsets\": [100, "
      "101] }\n"
      "  ]\n"
      "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--in", in_path.string(), "--format=json"}, &rom, &output);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, testing::HasSubstr("\"write_status\": \"success\""));
  EXPECT_THAT(output, testing::HasSubstr("\"save_status\": \"saved\""));
  EXPECT_FALSE(rom.dirty());
  EXPECT_NE(ReadFileBytes(rom_path), disk_before);

  const auto backups = FindBackupArtifacts(rom_path);
  ASSERT_EQ(backups.size(), 1u);
  EXPECT_EQ(ReadFileBytes(backups.front()), disk_before);

  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(rom_path.string()).ok());
  auto zones_or = zelda3::LoadWaterFillTable(&reopened);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status();
  ASSERT_EQ(zones_or->size(), 2u);
  EXPECT_EQ((*zones_or)[0].room_id, 0x30);
  EXPECT_EQ((*zones_or)[0].sram_bit_mask, 0x01);
  EXPECT_EQ((*zones_or)[0].fill_offsets, (std::vector<uint16_t>{100, 101}));
  EXPECT_EQ((*zones_or)[1].room_id, 0x31);
  EXPECT_EQ((*zones_or)[1].sram_bit_mask, 0x02);
  EXPECT_EQ((*zones_or)[1].fill_offsets, (std::vector<uint16_t>{200}));
}

TEST(DungeonCollisionJsonCommandsTest,
     CustomCollisionSandboxWritesOnlySandboxCopyOnce) {
  ScopedTempDirectory temp("sandbox");
  ScopedSandboxRoot sandbox_root(temp.path() / "sandboxes");
  const auto source_path = temp.path() / "source.sfc";
  const auto in_path = temp.path() / "collision.json";

  Rom source_rom;
  ASSERT_TRUE(
      source_rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(source_rom, source_path);
  source_rom.set_filename(source_path.string());
  source_rom.set_dirty(true);
  const std::vector<uint8_t> source_before = ReadFileBytes(source_path);

  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"rooms\": [\n"
            "    { \"room_id\": \"0x25\", \"tiles\": [ [65, \"0xB7\"] ] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--in", in_path.string(), "--sandbox", "--format=json"},
                  &source_rom, &output);
  auto sandbox_or = RomSandboxManager::Instance().ActiveSandbox();
  if (sandbox_or.ok()) {
    sandbox_root.Track(sandbox_or->id);
  }
  ASSERT_TRUE(status.ok()) << status;
  ASSERT_TRUE(sandbox_or.ok()) << sandbox_or.status();

  EXPECT_EQ(source_rom.vector(), source_before);
  EXPECT_EQ(ReadFileBytes(source_path), source_before);
  EXPECT_TRUE(source_rom.dirty());
  EXPECT_TRUE(FindBackupArtifacts(source_path).empty());

  const auto sandbox_backups = FindBackupArtifacts(sandbox_or->rom_path);
  ASSERT_EQ(sandbox_backups.size(), 1u);
  // A second generic auto-save would back up the already-modified sandbox.
  EXPECT_EQ(ReadFileBytes(sandbox_backups.front()), source_before);

  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(sandbox_or->rom_path.string()).ok());
  auto map_or = zelda3::LoadCustomCollisionMap(&reopened, 0x25);
  ASSERT_TRUE(map_or.ok()) << map_or.status();
  ASSERT_TRUE(map_or->has_data);
  EXPECT_EQ(map_or->tiles[65], 0xB7);
}

TEST(DungeonCollisionJsonCommandsTest,
     CustomCollisionReportNormalizedRomAliasIsRejectedWithoutMutation) {
  ScopedTempDirectory temp("normalized_alias");
  const auto rom_path = temp.path() / "target.sfc";
  const auto in_path = temp.path() / "collision.json";
  const auto lexical_parent = temp.path() / "lexical_parent";
  ASSERT_TRUE(std::filesystem::create_directory(lexical_parent));
  const auto report_alias = lexical_parent / ".." / rom_path.filename();

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);
  WriteSingleCollisionImport(in_path);

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFileBytes(rom_path);

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--in", in_path.string(), "--dry-run", "--report",
                   report_alias.string(), "--format=json"},
                  &rom, &output);

  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("aliases the active ROM"));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  EXPECT_TRUE(FindBackupArtifacts(rom_path).empty());
  EXPECT_FALSE(std::filesystem::exists(rom_path.string() + ".tmp"));
}

TEST(DungeonCollisionJsonCommandsTest,
     CustomCollisionReportSymlinkRomAliasIsRejectedWithoutMutation) {
  ScopedTempDirectory temp("symlink_alias");
  const auto rom_path = temp.path() / "target.sfc";
  const auto in_path = temp.path() / "collision.json";
  const auto report_alias = temp.path() / "report-link.json";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);
  WriteSingleCollisionImport(in_path);

  std::error_code link_ec;
  std::filesystem::create_symlink(rom_path, report_alias, link_ec);
  if (link_ec) {
    GTEST_SKIP() << "Symlinks unavailable: " << link_ec.message();
  }

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFileBytes(rom_path);

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--in", in_path.string(), "--dry-run", "--report",
                   report_alias.string(), "--format=json"},
                  &rom, &output);

  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("aliases the active ROM"));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  EXPECT_TRUE(std::filesystem::is_symlink(report_alias));
  EXPECT_TRUE(FindBackupArtifacts(rom_path).empty());
}

TEST(DungeonCollisionJsonCommandsTest,
     WaterFillReportHardlinkRomAliasIsRejectedWithoutMutation) {
  ScopedTempDirectory temp("hardlink_alias");
  const auto rom_path = temp.path() / "target.sfc";
  const auto in_path = temp.path() / "water.json";
  const auto report_alias = temp.path() / "report-hardlink.json";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);
  WriteSingleWaterFillImport(in_path);

  std::error_code link_ec;
  std::filesystem::create_hard_link(rom_path, report_alias, link_ec);
  if (link_ec) {
    GTEST_SKIP() << "Hardlinks unavailable: " << link_ec.message();
  }

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFileBytes(rom_path);

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--in", in_path.string(), "--dry-run", "--report",
                   report_alias.string(), "--format=json"},
                  &rom, &output);

  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("aliases the active ROM"));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  EXPECT_TRUE(std::filesystem::exists(report_alias));
  EXPECT_TRUE(FindBackupArtifacts(rom_path).empty());
}

TEST(DungeonCollisionJsonCommandsTest, ExportReportsCannotAliasActiveRom) {
  ScopedTempDirectory temp("export_report_alias");
  const auto rom_path = temp.path() / "target.sfc";
  const auto collision_out = temp.path() / "collision.json";
  const auto water_out = temp.path() / "water.json";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);
  const std::vector<uint8_t> disk_before = ReadFileBytes(rom_path);

  handlers::DungeonExportCustomCollisionJsonCommandHandler collision_handler;
  std::string collision_output;
  const absl::Status collision_status =
      collision_handler.Run({"--out", collision_out.string(), "--all",
                             "--report", rom_path.string(), "--format=json"},
                            &rom, &collision_output);
  EXPECT_TRUE(absl::IsInvalidArgument(collision_status)) << collision_status;
  EXPECT_THAT(std::string(collision_status.message()),
              testing::HasSubstr("aliases the active ROM"));

  handlers::DungeonExportWaterFillJsonCommandHandler water_handler;
  std::string water_output;
  const absl::Status water_status =
      water_handler.Run({"--out", water_out.string(), "--all", "--report",
                         rom_path.string(), "--format=json"},
                        &rom, &water_output);
  EXPECT_TRUE(absl::IsInvalidArgument(water_status)) << water_status;
  EXPECT_THAT(std::string(water_status.message()),
              testing::HasSubstr("aliases the active ROM"));

  EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  EXPECT_FALSE(std::filesystem::exists(collision_out));
  EXPECT_FALSE(std::filesystem::exists(water_out));
}

TEST(DungeonCollisionJsonCommandsTest,
     ExportOutRejectsDirectNormalizedSymlinkAndHardlinkRomAliases) {
  ScopedTempDirectory temp("export_out_aliases");
  const auto rom_path = temp.path() / "target.sfc";
  const auto lexical_parent = temp.path() / "lexical-parent";
  ASSERT_TRUE(std::filesystem::create_directory(lexical_parent));

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(true);

  const auto memory_before = rom.vector();
  const auto disk_before = ReadFileBytes(rom_path);
  const std::string filename_before = rom.filename();

  std::vector<std::pair<std::string, std::filesystem::path>> aliases = {
      {"direct", rom_path},
      {"normalized", lexical_parent / ".." / rom_path.filename()},
  };

  const auto symlink_alias = temp.path() / "collision-symlink.json";
  std::error_code symlink_ec;
  std::filesystem::create_symlink(rom_path, symlink_alias, symlink_ec);
  if (!symlink_ec) {
    aliases.emplace_back("symlink", symlink_alias);
  }

  const auto hardlink_alias = temp.path() / "collision-hardlink.json";
  std::error_code hardlink_ec;
  std::filesystem::create_hard_link(rom_path, hardlink_alias, hardlink_ec);
  if (!hardlink_ec) {
    aliases.emplace_back("hardlink", hardlink_alias);
  }

  handlers::DungeonExportCustomCollisionJsonCommandHandler handler;
  for (const auto& [label, alias] : aliases) {
    SCOPED_TRACE(label);
    std::string output;
    const absl::Status status = handler.Run(
        {"--out", alias.string(), "--all", "--format=json"}, &rom, &output);

    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_THAT(std::string(status.message()),
                testing::HasSubstr("--out path aliases the active ROM"));
    EXPECT_THAT(output, testing::HasSubstr("\"status\": \"error\""));
    EXPECT_THAT(output, testing::HasSubstr("INVALID_ARGUMENT"));
    EXPECT_EQ(rom.vector(), memory_before);
    EXPECT_EQ(rom.filename(), filename_before);
    EXPECT_TRUE(rom.dirty());
    EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  }
}

TEST(DungeonCollisionJsonCommandsTest,
     ExportOutAndReportAliasesFailBeforePublication) {
  ScopedTempDirectory temp("export_artifact_aliases");
  const auto rom_path = temp.path() / "target.sfc";
  const auto same_path = temp.path() / "same.json";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);

  handlers::DungeonExportCustomCollisionJsonCommandHandler handler;
  std::string output;
  absl::Status status =
      handler.Run({"--out", same_path.string(), "--report", same_path.string(),
                   "--all", "--format=json"},
                  &rom, &output);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("alias each other"));
  EXPECT_THAT(output, testing::HasSubstr("INVALID_ARGUMENT"));
  EXPECT_FALSE(std::filesystem::exists(same_path));

  const auto existing_out = temp.path() / "existing-out.json";
  const auto report_hardlink = temp.path() / "report-hardlink.json";
  WriteFile(existing_out, "previous artifact\n");
  std::error_code hardlink_ec;
  std::filesystem::create_hard_link(existing_out, report_hardlink, hardlink_ec);
  if (!hardlink_ec) {
    output.clear();
    status = handler.Run({"--out", existing_out.string(), "--report",
                          report_hardlink.string(), "--all", "--format=json"},
                         &rom, &output);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_THAT(std::string(status.message()),
                testing::HasSubstr("alias each other"));
    EXPECT_EQ(ReadFile(existing_out), "previous artifact\n");
    EXPECT_EQ(ReadFile(report_hardlink), "previous artifact\n");
  }

  const auto symlink_out = temp.path() / "symlink-out.json";
  const auto report_symlink = temp.path() / "report-symlink.json";
  WriteFile(symlink_out, "previous symlink artifact\n");
  std::error_code symlink_ec;
  std::filesystem::create_symlink(symlink_out, report_symlink, symlink_ec);
  if (!symlink_ec) {
    output.clear();
    status = handler.Run({"--out", symlink_out.string(), "--report",
                          report_symlink.string(), "--all", "--format=json"},
                         &rom, &output);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_THAT(std::string(status.message()),
                testing::HasSubstr("alias each other"));
    EXPECT_EQ(ReadFile(symlink_out), "previous symlink artifact\n");
    EXPECT_EQ(ReadFile(report_symlink), "previous symlink artifact\n");
  }
}

TEST(DungeonCollisionJsonCommandsTest,
     NewNativeEquivalentOutAndReportNamesRollBackPublication) {
  ScopedTempDirectory temp("export_new_native_aliases");
  const auto rom_path = temp.path() / "target.sfc";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);
  const auto disk_before = ReadFileBytes(rom_path);

  const std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
      candidates = {
          {temp.path() / "collision.json", temp.path() / "COLLISION.JSON"},
          {temp.path() / "caf\xC3\xA9.json", temp.path() / "cafe\xCC\x81.json"},
      };

  bool exercised = false;
  handlers::DungeonExportCustomCollisionJsonCommandHandler handler;
  for (const auto& [out_path, report_path] : candidates) {
    WriteFile(out_path, "filesystem-equivalence probe\n");
    std::error_code equivalent_ec;
    const bool native_alias =
        std::filesystem::equivalent(out_path, report_path, equivalent_ec);
    std::error_code remove_ec;
    std::filesystem::remove(out_path, remove_ec);
    ASSERT_FALSE(remove_ec) << remove_ec.message();
    if (equivalent_ec || !native_alias) {
      continue;
    }

    exercised = true;
    std::string output;
    const absl::Status status =
        handler.Run({"--out", out_path.string(), "--report",
                     report_path.string(), "--all", "--format=json"},
                    &rom, &output);
    EXPECT_FALSE(status.ok()) << status;
    EXPECT_THAT(std::string(status.message()),
                testing::HasSubstr("alias each other"));
    EXPECT_THAT(output, testing::HasSubstr("\"status\": \"error\""));
    EXPECT_FALSE(std::filesystem::exists(out_path));
    EXPECT_FALSE(std::filesystem::exists(report_path));
    EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  }

  if (!exercised) {
    GTEST_SKIP() << "Filesystem exposes neither case nor Unicode equivalence";
  }

  for (const auto& entry : std::filesystem::directory_iterator(temp.path())) {
    EXPECT_EQ(entry.path().filename().string().find(".yaze-tmp-"),
              std::string::npos)
        << entry.path();
  }
}

TEST(DungeonCollisionJsonCommandsTest,
     ExportOutRejectsCaseOrUnicodeEquivalentRomWhereSupported) {
  ScopedTempDirectory temp("export_native_equivalence");
  const auto rom_path = temp.path() / "\xC3\x89xport.sfc";  // NFC E-acute.

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);
  const auto disk_before = ReadFileBytes(rom_path);

  const std::vector<std::filesystem::path> candidates = {
      temp.path() / "\xC3\xA9xport.sfc",   // Case-equivalent where folded.
      temp.path() / "E\xCC\x81xport.sfc",  // NFD equivalent where normalized.
  };

  bool exercised = false;
  handlers::DungeonExportCustomCollisionJsonCommandHandler handler;
  for (const auto& candidate : candidates) {
    std::error_code equivalent_ec;
    if (!std::filesystem::equivalent(rom_path, candidate, equivalent_ec) ||
        equivalent_ec) {
      continue;
    }
    exercised = true;
    std::string output;
    const absl::Status status = handler.Run(
        {"--out", candidate.string(), "--all", "--format=json"}, &rom, &output);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_THAT(std::string(status.message()),
                testing::HasSubstr("aliases the active ROM"));
    EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  }

  if (!exercised) {
    GTEST_SKIP() << "Filesystem exposes neither case nor Unicode equivalence";
  }
}

TEST(DungeonCollisionJsonCommandsTest,
     SandboxExportsRejectSourceRomAliasesAndPreserveCallerState) {
  ScopedSandboxFlag sandbox_flag(false);
  ScopedTempDirectory temp("sandbox_export_source_alias");
  ScopedSandboxRoot sandbox_root(temp.path() / "sandboxes");
  const auto source_path = temp.path() / "source.sfc";
  const auto safe_out = temp.path() / "safe-out.json";

  Rom source_rom;
  ASSERT_TRUE(
      source_rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(source_rom, source_path);
  source_rom.set_filename(source_path.string());
  source_rom.set_dirty(true);

  const auto memory_before = source_rom.vector();
  const auto disk_before = ReadFileBytes(source_path);
  const std::string filename_before = source_rom.filename();

  handlers::DungeonExportCustomCollisionJsonCommandHandler handler;
  std::string output;
  absl::Status status = handler.Run(
      {"--out", source_path.string(), "--all", "--sandbox", "--format=json"},
      &source_rom, &output);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("sandbox source ROM"));
  EXPECT_THAT(output, testing::HasSubstr("INVALID_ARGUMENT"));
  auto first_sandbox = RomSandboxManager::Instance().ActiveSandbox();
  ASSERT_TRUE(first_sandbox.ok()) << first_sandbox.status();
  sandbox_root.Track(first_sandbox->id);
  EXPECT_EQ(ReadFileBytes(first_sandbox->rom_path), disk_before);

  output.clear();
  status =
      handler.Run({"--out", safe_out.string(), "--report", source_path.string(),
                   "--all", "--sandbox", "--format=json"},
                  &source_rom, &output);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("sandbox source ROM"));
  EXPECT_THAT(output, testing::HasSubstr("INVALID_ARGUMENT"));
  auto second_sandbox = RomSandboxManager::Instance().ActiveSandbox();
  ASSERT_TRUE(second_sandbox.ok()) << second_sandbox.status();
  sandbox_root.Track(second_sandbox->id);
  EXPECT_EQ(ReadFileBytes(second_sandbox->rom_path), disk_before);

  EXPECT_EQ(source_rom.vector(), memory_before);
  EXPECT_EQ(source_rom.filename(), filename_before);
  EXPECT_TRUE(source_rom.dirty());
  EXPECT_EQ(ReadFileBytes(source_path), disk_before);
  EXPECT_FALSE(std::filesystem::exists(safe_out));
  EXPECT_TRUE(FindBackupArtifacts(source_path).empty());
}

TEST(DungeonCollisionJsonCommandsTest,
     ExportAndReportPublishValidJsonWithoutMutatingRom) {
  ScopedTempDirectory temp("export_two_artifacts");
  const auto rom_path = temp.path() / "target.sfc";
  const auto out_path = temp.path() / "collision.json";
  const auto report_path = temp.path() / "collision.report.json";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(true);
  WriteFile(out_path, "previous output\n");
  WriteFile(report_path, "previous report\n");

  const auto memory_before = rom.vector();
  const auto disk_before = ReadFileBytes(rom_path);
  const std::string filename_before = rom.filename();

  handlers::DungeonExportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--out", out_path.string(), "--room=0x25", "--report",
                   report_path.string(), "--format=json"},
                  &rom, &output);
  ASSERT_TRUE(status.ok()) << status;

  auto rooms_or =
      zelda3::LoadCustomCollisionRoomsFromJsonString(ReadFile(out_path));
  ASSERT_TRUE(rooms_or.ok()) << rooms_or.status();
  const auto report = nlohmann::json::parse(ReadFile(report_path));
  EXPECT_EQ(report.value("command", ""),
            "dungeon-export-custom-collision-json");
  EXPECT_EQ(report.value("status", ""), "success");
  EXPECT_EQ(report.value("out_path", ""), out_path.string());
  EXPECT_THAT(output, testing::HasSubstr("\"status\": \"success\""));

  EXPECT_EQ(rom.vector(), memory_before);
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_TRUE(rom.dirty());
  EXPECT_EQ(ReadFileBytes(rom_path), disk_before);

  for (const auto& entry : std::filesystem::directory_iterator(temp.path())) {
    EXPECT_EQ(entry.path().filename().string().find(".yaze-tmp-"),
              std::string::npos)
        << entry.path();
  }
}

TEST(DungeonCollisionJsonCommandsTest,
     ExportPublicationFailuresPreservePreviousArtifacts) {
  ScopedTempDirectory temp("export_publish_failure");
  const auto rom_path = temp.path() / "target.sfc";
  const auto blocked_out = temp.path() / "blocked-out.json";
  const auto blocked_out_marker = blocked_out / "previous.txt";
  const auto error_report = temp.path() / "error.report.json";
  ASSERT_TRUE(std::filesystem::create_directory(blocked_out));
  WriteFile(blocked_out_marker, "previous output artifact\n");
  WriteFile(error_report, "previous report artifact\n");

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(true);
  const auto memory_before = rom.vector();
  const auto disk_before = ReadFileBytes(rom_path);
  const std::string filename_before = rom.filename();

  handlers::DungeonExportCustomCollisionJsonCommandHandler handler;
  std::string output;
  absl::Status status =
      handler.Run({"--out", blocked_out.string(), "--room=0x25", "--report",
                   error_report.string(), "--format=json"},
                  &rom, &output);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("Artifact target is a directory"));
  EXPECT_EQ(ReadFile(blocked_out_marker), "previous output artifact\n");
  EXPECT_EQ(ReadFile(error_report), "previous report artifact\n");
  EXPECT_THAT(output, testing::HasSubstr("\"status\": \"error\""));

  const auto valid_out = temp.path() / "valid-out.json";
  const auto blocked_report = temp.path() / "blocked-report.json";
  const auto blocked_report_marker = blocked_report / "previous.txt";
  ASSERT_TRUE(std::filesystem::create_directory(blocked_report));
  WriteFile(blocked_report_marker, "previous report artifact\n");
  WriteFile(valid_out, "previous output artifact\n");

  output.clear();
  status = handler.Run({"--out", valid_out.string(), "--room=0x25", "--report",
                        blocked_report.string(), "--format=json"},
                       &rom, &output);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("Artifact target is a directory"));
  EXPECT_EQ(ReadFile(blocked_report_marker), "previous report artifact\n");
  EXPECT_EQ(ReadFile(valid_out), "previous output artifact\n");
  EXPECT_THAT(output, testing::HasSubstr("\"status\": \"error\""));

  EXPECT_EQ(rom.vector(), memory_before);
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_TRUE(rom.dirty());
  EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  for (const auto& entry : std::filesystem::directory_iterator(temp.path())) {
    EXPECT_EQ(entry.path().filename().string().find(".yaze-tmp-"),
              std::string::npos)
        << entry.path();
  }
}

TEST(DungeonCollisionJsonCommandsTest,
     LaterReportPublicationFailureRestoresEarlierOutput) {
#if !defined(__APPLE__)
  GTEST_SKIP() << "Uses the macOS immutable-file flag to force late rename "
                  "failure";
#else
  ScopedTempDirectory temp("export_late_report_failure");
  const auto rom_path = temp.path() / "target.sfc";
  const auto out_path = temp.path() / "collision.json";
  const auto report_path = temp.path() / "collision.report.json";
  WriteFile(out_path, "previous output artifact\n");
  WriteFile(report_path, "previous report artifact\n");

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(true);
  const auto memory_before = rom.vector();
  const auto disk_before = ReadFileBytes(rom_path);
  const std::string filename_before = rom.filename();

  {
    ScopedImmutableFile immutable_report(report_path);
    if (!immutable_report.enabled()) {
      GTEST_SKIP() << "Could not set the immutable-file publication guard";
    }

    handlers::DungeonExportCustomCollisionJsonCommandHandler handler;
    std::string output;
    const absl::Status status =
        handler.Run({"--out", out_path.string(), "--room=0x25", "--report",
                     report_path.string(), "--format=json"},
                    &rom, &output);
    EXPECT_FALSE(status.ok()) << status;
    EXPECT_THAT(std::string(status.message()),
                testing::HasSubstr("Failed to publish artifact"));
    EXPECT_THAT(output, testing::HasSubstr("\"status\": \"error\""));
    EXPECT_EQ(ReadFile(out_path), "previous output artifact\n");
    EXPECT_EQ(ReadFile(report_path), "previous report artifact\n");
  }

  EXPECT_EQ(rom.vector(), memory_before);
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_TRUE(rom.dirty());
  EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  for (const auto& entry : std::filesystem::directory_iterator(temp.path())) {
    EXPECT_EQ(entry.path().filename().string().find(".yaze-tmp-"),
              std::string::npos)
        << entry.path();
  }
#endif
}

TEST(DungeonCollisionJsonCommandsTest,
     ConcurrentSandboxExportsKeepInvocationSourceIdentity) {
  ScopedSandboxFlag sandbox_flag(false);
  ScopedTempDirectory temp("concurrent_sandbox_exports");
  const auto sandbox_root_path = temp.path() / "sandboxes";
  ScopedSandboxRoot sandbox_root(sandbox_root_path);
  const auto source_a_path = temp.path() / "source-a.sfc";
  const auto source_b_path = temp.path() / "source-b.sfc";
  const auto out_a = temp.path() / "a.json";
  const auto out_b = temp.path() / "b.json";
  const auto report_a = temp.path() / "a.report.json";
  const auto report_b = temp.path() / "b.report.json";

  Rom source_a;
  Rom source_b;
  ASSERT_TRUE(source_a.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  ASSERT_TRUE(source_b.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(source_a, source_a_path);
  WriteRomFile(source_b, source_b_path);
  source_a.set_filename(source_a_path.string());
  source_b.set_filename(source_b_path.string());
  source_a.set_dirty(true);
  source_b.set_dirty(true);
  const auto source_a_before = ReadFileBytes(source_a_path);
  const auto source_b_before = ReadFileBytes(source_b_path);

  std::barrier rendezvous(2);
  CoordinatedCollisionExportHandler handler_a(&rendezvous);
  CoordinatedCollisionExportHandler handler_b(&rendezvous);
  absl::Status status_a = absl::UnknownError("not run");
  absl::Status status_b = absl::UnknownError("not run");
  std::string output_a;
  std::string output_b;

  std::thread thread_a([&]() {
    status_a =
        handler_a.Run({"--out", out_a.string(), "--report", report_a.string(),
                       "--all", "--sandbox", "--expected-source",
                       source_a_path.string(), "--format=json"},
                      &source_a, &output_a);
  });
  std::thread thread_b([&]() {
    status_b =
        handler_b.Run({"--out", out_b.string(), "--report", report_b.string(),
                       "--all", "--sandbox", "--expected-source",
                       source_b_path.string(), "--format=json"},
                      &source_b, &output_b);
  });
  thread_a.join();
  thread_b.join();

  EXPECT_TRUE(status_a.ok()) << status_a;
  EXPECT_TRUE(status_b.ok()) << status_b;
  EXPECT_TRUE(
      zelda3::LoadCustomCollisionRoomsFromJsonString(ReadFile(out_a)).ok());
  EXPECT_TRUE(
      zelda3::LoadCustomCollisionRoomsFromJsonString(ReadFile(out_b)).ok());
  EXPECT_EQ(nlohmann::json::parse(ReadFile(report_a)).value("status", ""),
            "success");
  EXPECT_EQ(nlohmann::json::parse(ReadFile(report_b)).value("status", ""),
            "success");
  EXPECT_TRUE(source_a.dirty());
  EXPECT_TRUE(source_b.dirty());
  EXPECT_EQ(source_a.filename(), source_a_path.string());
  EXPECT_EQ(source_b.filename(), source_b_path.string());
  EXPECT_EQ(ReadFileBytes(source_a_path), source_a_before);
  EXPECT_EQ(ReadFileBytes(source_b_path), source_b_before);

  int tracked = 0;
  for (const auto& sandbox : RomSandboxManager::Instance().ListSandboxes()) {
    if (sandbox.directory.parent_path() == sandbox_root_path) {
      sandbox_root.Track(sandbox.id);
      ++tracked;
    }
  }
  EXPECT_EQ(tracked, 2);
}

TEST(DungeonCollisionJsonCommandsTest,
     DryRunReportCannotAliasUnicodeEquivalentRom) {
  ScopedTempDirectory temp("unicode_rom_alias");
  const auto rom_path = temp.path() / "\xC3\xA9.sfc";  // NFC: e-acute.
  const auto in_path = temp.path() / "collision.json";
  const auto report_path = temp.path() / "e\xCC\x81.sfc";  // NFD.

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);
  WriteSingleCollisionImport(in_path);

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFileBytes(rom_path);

  std::error_code equivalent_ec;
  const bool unicode_equivalent =
      std::filesystem::equivalent(rom_path, report_path, equivalent_ec);
  if (equivalent_ec || !unicode_equivalent) {
    GTEST_SKIP() << "Filesystem does not equate NFC/NFD paths";
  }

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--in", in_path.string(), "--dry-run", "--report",
                   report_path.string(), "--format=json"},
                  &rom, &output);

  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("aliases the active ROM"));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  EXPECT_TRUE(FindBackupArtifacts(rom_path).empty());
}

TEST(DungeonCollisionJsonCommandsTest,
     SandboxMaterializationFailurePreservesSourceDirty) {
  ScopedTempDirectory temp("sandbox_save_failure");
  ScopedSandboxRoot sandbox_root(temp.path() / "sandboxes");

  Rom source_rom;
  ASSERT_TRUE(
      source_rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  // A trailing separator gives the source path an empty filename. The manager
  // can create its sandbox directory, but SaveToFile cannot replace that
  // directory with the ROM file, forcing the materialization save to fail.
  source_rom.set_filename(temp.path().string() + "/");
  source_rom.set_dirty(true);

  const absl::StatusOr<RomSandboxManager::SandboxMetadata> sandbox_or =
      RomSandboxManager::Instance().CreateSandbox(source_rom, "test failure");

  EXPECT_FALSE(sandbox_or.ok());
  EXPECT_TRUE(source_rom.dirty());
  EXPECT_TRUE(
      absl::IsNotFound(RomSandboxManager::Instance().ActiveSandbox().status()));
}

TEST(DungeonCollisionJsonCommandsTest,
     CustomCollisionDryRunReportFailureDoesNotMutateRom) {
  ScopedTempDirectory temp("directory_report");
  const auto rom_path = temp.path() / "target.sfc";
  const auto in_path = temp.path() / "collision.json";
  const auto report_path = temp.path() / "report-directory";
  ASSERT_TRUE(std::filesystem::create_directory(report_path));

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(true);
  WriteSingleCollisionImport(in_path);

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFileBytes(rom_path);

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output = "untouched";
  const absl::Status status =
      handler.Run({"--in", in_path.string(), "--dry-run", "--report",
                   report_path.string(), "--format=json"},
                  &rom, &output);

  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("Artifact target is a directory"));
  EXPECT_THAT(output, testing::HasSubstr("\"mode\": \"dry-run\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(rom.dirty());
  EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  EXPECT_TRUE(std::filesystem::is_directory(report_path));
  EXPECT_TRUE(FindBackupArtifacts(rom_path).empty());
}

TEST(DungeonCollisionJsonCommandsTest,
     ImportReportsRejectWriteModeAndSandboxBeforeContext) {
  ScopedSandboxFlag sandbox_flag(false);
  ScopedTempDirectory temp("write_report_rejected");
  ScopedSandboxRoot sandbox_root(temp.path() / "sandboxes");
  const auto source_path = temp.path() / "source.sfc";
  const auto collision_path = temp.path() / "collision.json";
  const auto water_path = temp.path() / "water.json";
  const auto report_path = temp.path() / "report.json";

  Rom source_rom;
  ASSERT_TRUE(
      source_rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(source_rom, source_path);
  source_rom.set_filename(source_path.string());
  source_rom.set_dirty(true);
  WriteSingleCollisionImport(collision_path);
  WriteSingleWaterFillImport(water_path);

  const std::vector<uint8_t> before = source_rom.vector();
  const std::vector<uint8_t> disk_before = ReadFileBytes(source_path);
  handlers::DungeonImportCustomCollisionJsonCommandHandler collision_handler;
  handlers::DungeonImportWaterFillJsonCommandHandler water_handler;

  for (const std::vector<std::string>& mode_args :
       {std::vector<std::string>{}, std::vector<std::string>{"--mock-rom"},
        std::vector<std::string>{"--sandbox"}}) {
    std::vector<std::string> collision_args = {"--in", collision_path.string(),
                                               "--report", report_path.string(),
                                               "--format=json"};
    collision_args.insert(collision_args.end(), mode_args.begin(),
                          mode_args.end());
    std::string collision_output = "untouched";
    const absl::Status collision_status =
        collision_handler.Run(collision_args, &source_rom, &collision_output);
    EXPECT_TRUE(absl::IsInvalidArgument(collision_status)) << collision_status;
    EXPECT_THAT(
        std::string(collision_status.message()),
        testing::HasSubstr("--report is supported only with --dry-run"));
    EXPECT_EQ(collision_output, "untouched");

    std::vector<std::string> water_args = {"--in", water_path.string(),
                                           "--report", report_path.string(),
                                           "--format=json"};
    water_args.insert(water_args.end(), mode_args.begin(), mode_args.end());
    std::string water_output = "untouched";
    const absl::Status water_status =
        water_handler.Run(water_args, &source_rom, &water_output);
    EXPECT_TRUE(absl::IsInvalidArgument(water_status)) << water_status;
    EXPECT_THAT(
        std::string(water_status.message()),
        testing::HasSubstr("--report is supported only with --dry-run"));
    EXPECT_EQ(water_output, "untouched");
  }

  for (auto* handler :
       {static_cast<resources::CommandHandler*>(&collision_handler),
        static_cast<resources::CommandHandler*>(&water_handler)}) {
    const std::filesystem::path& input =
        handler == &collision_handler ? collision_path : water_path;
    std::string output = "untouched";
    const absl::Status status =
        handler->Run({"--in", input.string(), "--dry-run", "--sandbox",
                      "--report", report_path.string(), "--format=json"},
                     &source_rom, &output);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_THAT(std::string(status.message()),
                testing::HasSubstr("--report cannot be combined with "
                                   "--sandbox"));
    EXPECT_EQ(output, "untouched");
  }

  {
    ScopedSandboxFlag global_sandbox(true);
    std::string output = "untouched";
    const absl::Status status = collision_handler.Run(
        {"--in", collision_path.string(), "--dry-run", "--report",
         report_path.string(), "--format=json"},
        &source_rom, &output);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_THAT(std::string(status.message()),
                testing::HasSubstr("--report cannot be combined with "
                                   "--sandbox"));
    EXPECT_EQ(output, "untouched");
  }

  for (auto* handler :
       {static_cast<resources::CommandHandler*>(&collision_handler),
        static_cast<resources::CommandHandler*>(&water_handler)}) {
    const std::filesystem::path& input =
        handler == &collision_handler ? collision_path : water_path;
    std::string output = "untouched";
    const absl::Status status = handler->Run(
        {"--in", input.string(), "--dry-run", "--report", "", "--format=json"},
        &source_rom, &output);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_THAT(std::string(status.message()),
                testing::HasSubstr("--report cannot be empty"));
    EXPECT_EQ(output, "untouched");
  }

  EXPECT_EQ(source_rom.vector(), before);
  EXPECT_TRUE(source_rom.dirty());
  EXPECT_EQ(ReadFileBytes(source_path), disk_before);
  EXPECT_FALSE(std::filesystem::exists(report_path));
  EXPECT_TRUE(FindBackupArtifacts(source_path).empty());
  EXPECT_TRUE(
      absl::IsNotFound(RomSandboxManager::Instance().ActiveSandbox().status()));
}

TEST(DungeonCollisionJsonCommandsTest,
     MockRomAndSandboxAreRejectedForBothImportsBeforeMutation) {
  ScopedTempDirectory temp("mock_sandbox");
  ScopedSandboxRoot sandbox_root(temp.path() / "sandboxes");
  const auto source_path = temp.path() / "source.sfc";
  const auto collision_path = temp.path() / "collision.json";
  const auto water_path = temp.path() / "water.json";

  Rom source_rom;
  ASSERT_TRUE(
      source_rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(source_rom, source_path);
  source_rom.set_filename(source_path.string());
  source_rom.set_dirty(true);
  WriteSingleCollisionImport(collision_path);
  WriteSingleWaterFillImport(water_path);

  const std::vector<uint8_t> before = source_rom.vector();
  const std::vector<uint8_t> disk_before = ReadFileBytes(source_path);

  handlers::DungeonImportCustomCollisionJsonCommandHandler collision_handler;
  std::string collision_output = "untouched";
  const absl::Status collision_status =
      collision_handler.Run({"--in", collision_path.string(), "--mock-rom",
                             "--sandbox", "--format=json"},
                            &source_rom, &collision_output);
  EXPECT_TRUE(absl::IsInvalidArgument(collision_status)) << collision_status;
  EXPECT_THAT(std::string(collision_status.message()),
              testing::HasSubstr("mutually exclusive"));
  EXPECT_EQ(collision_output, "untouched");

  handlers::DungeonImportWaterFillJsonCommandHandler water_handler;
  std::string water_output = "untouched";
  const absl::Status water_status = water_handler.Run(
      {"--in", water_path.string(), "--mock-rom", "--sandbox", "--format=json"},
      &source_rom, &water_output);
  EXPECT_TRUE(absl::IsInvalidArgument(water_status)) << water_status;
  EXPECT_THAT(std::string(water_status.message()),
              testing::HasSubstr("mutually exclusive"));
  EXPECT_EQ(water_output, "untouched");

  EXPECT_EQ(source_rom.vector(), before);
  EXPECT_TRUE(source_rom.dirty());
  EXPECT_EQ(ReadFileBytes(source_path), disk_before);
  EXPECT_TRUE(FindBackupArtifacts(source_path).empty());
  EXPECT_TRUE(
      absl::IsNotFound(RomSandboxManager::Instance().ActiveSandbox().status()));
}

TEST(DungeonCollisionJsonCommandsTest,
     CustomCollisionSerializerFailureRollsBackPartialWrites) {
  ScopedTempDirectory temp("serializer_failure");
  const auto rom_path = temp.path() / "target.sfc";
  const auto in_path = temp.path() / "collision.json";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  // Leave room for exactly one imported one-tile map. The second room then
  // fails after the first room has already changed data and its pointer.
  const int existing_pc = zelda3::kCustomCollisionDataSoftEnd - 17;
  SetLong(&rom, zelda3::kCustomCollisionRoomPointers, PcToSnes(existing_pc));
  ASSERT_TRUE(
      rom.WriteVector(existing_pc, std::vector<uint8_t>{0xF0, 0xF0, 0x00, 0x00,
                                                        0xB7, 0xFF, 0xFF})
          .ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);

  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"rooms\": [\n"
            "    { \"room_id\": \"0x20\", \"tiles\": [ [1, \"0xB7\"] ] },\n"
            "    { \"room_id\": \"0x21\", \"tiles\": [ [2, \"0xB8\"] ] }\n"
            "  ]\n"
            "}\n");

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFileBytes(rom_path);
  const std::string filename_before = rom.filename();
  const bool dirty_before = rom.dirty();

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--in", in_path.string(), "--format=json"}, &rom, &output);

  EXPECT_TRUE(absl::IsResourceExhausted(status)) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  EXPECT_TRUE(FindBackupArtifacts(rom_path).empty());
  EXPECT_FALSE(std::filesystem::exists(rom_path.string() + ".tmp"));
}

TEST(DungeonCollisionJsonCommandsTest,
     CustomCollisionRequiredBackupFailureRollsBackCallerRom) {
  ScopedTempDirectory temp("backup_failure");
  const auto rom_path = temp.path() / "target.sfc";
  const auto in_path = temp.path() / "collision.json";
  ASSERT_TRUE(std::filesystem::create_directory(rom_path));

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);
  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"rooms\": [\n"
            "    { \"room_id\": \"0x25\", \"tiles\": [ [65, \"0xB7\"] ] }\n"
            "  ]\n"
            "}\n");

  const std::vector<uint8_t> before = rom.vector();
  const std::string filename_before = rom.filename();
  const bool dirty_before = rom.dirty();

  handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--in", in_path.string(), "--format=json"}, &rom, &output);

  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("Could not create required ROM backup"));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_TRUE(std::filesystem::is_directory(rom_path));
  EXPECT_FALSE(std::filesystem::exists(rom_path.string() + ".tmp"));
  EXPECT_TRUE(FindBackupArtifacts(rom_path).empty());
}

TEST(DungeonCollisionJsonCommandsTest,
     WaterFillDiskSaveFailureRollsBackAndRetainsBackup) {
  ScopedTempDirectory temp("disk_failure");
  const auto rom_path = temp.path() / "target.sfc";
  const auto in_path = temp.path() / "water.json";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteRomFile(rom, rom_path);
  rom.set_filename(rom_path.string());
  rom.set_dirty(false);
  ASSERT_TRUE(std::filesystem::create_directory(rom_path.string() + ".tmp"));

  WriteFile(
      in_path,
      "{\n"
      "  \"version\": 1,\n"
      "  \"zones\": [\n"
      "    { \"room_id\": \"0x30\", \"mask\": \"0x01\", \"offsets\": [100] }\n"
      "  ]\n"
      "}\n");

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFileBytes(rom_path);
  const std::string filename_before = rom.filename();
  const bool dirty_before = rom.dirty();

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--in", in_path.string(), "--format=json"}, &rom, &output);

  EXPECT_TRUE(absl::IsInternal(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("Could not open temp ROM file for writing"));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(ReadFileBytes(rom_path), disk_before);
  EXPECT_TRUE(std::filesystem::is_directory(rom_path.string() + ".tmp"));

  const auto backups = FindBackupArtifacts(rom_path);
  ASSERT_EQ(backups.size(), 1u);
  EXPECT_EQ(ReadFileBytes(backups.front()), disk_before);
}

TEST(DungeonCollisionJsonCommandsTest, CustomCollisionReplaceAllRequiresForce) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  const auto in_path = std::filesystem::temp_directory_path() /
                       "yaze_custom_collision_replace_all_requires_force.json";
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
      handler.Run({"--in", in_path.string(), "--replace-all", "--format=json"},
                  &rom, &output);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(std::string(status.message()),
              testing::HasSubstr("--replace-all requires --force"));

  std::filesystem::remove(in_path);
}

TEST(DungeonCollisionJsonCommandsTest, WaterFillImportDryRunDoesNotWrite) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  SeedCustomCollisionRooms(&rom, {0x27});

  const auto in_path =
      std::filesystem::temp_directory_path() / "yaze_water_fill_dry_run.json";
  WriteFile(
      in_path,
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

  const auto in_path = std::filesystem::temp_directory_path() /
                       "yaze_water_fill_required_room.json";
  const auto report_path = std::filesystem::temp_directory_path() /
                           "yaze_water_fill_required_room.report.json";
  WriteFile(
      in_path,
      "{\n"
      "  \"version\": 1,\n"
      "  \"zones\": [\n"
      "    { \"room_id\": \"0x25\", \"mask\": \"0x01\", \"offsets\": [100] }\n"
      "  ]\n"
      "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--in", in_path.string(), "--dry-run", "--report",
                   report_path.string(), "--format=json"},
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
  const auto report_path = std::filesystem::temp_directory_path() /
                           "yaze_water_fill_strict.report.json";
  WriteFile(
      in_path,
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
      handler.Run({"--in", in_path.string(), "--dry-run", "--strict-masks",
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
  WriteFile(
      in_path,
      "{\n"
      "  \"version\": 1,\n"
      "  \"zones\": [\n"
      "    { \"room_id\": \"0x25\", \"mask\": \"0x01\", \"offsets\": [32] }\n"
      "  ]\n"
      "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--in", in_path.string(), "--dry-run", "--report",
                   report_path.string(), "--format=json"},
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

TEST(DungeonCollisionJsonCommandsTest, ZoraTempleProfileBothRoomsRoundtrip) {
  // Write both D4 zones with correct masks, then export+verify both survive.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  SeedCustomCollisionRooms(&rom, {0x25, 0x27});

  const auto in_path =
      std::filesystem::temp_directory_path() / "yaze_d4_profile_import.json";
  const auto out_path =
      std::filesystem::temp_directory_path() / "yaze_d4_profile_export.json";

  WriteFile(in_path,
            "{\n"
            "  \"version\": 1,\n"
            "  \"zones\": [\n"
            "    { \"room_id\": \"0x25\", \"mask\": \"0x02\", \"offsets\": "
            "[100, 101] },\n"
            "    { \"room_id\": \"0x27\", \"mask\": \"0x01\", \"offsets\": "
            "[200, 201] }\n"
            "  ]\n"
            "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler import_handler;
  std::string import_output;
  ASSERT_TRUE(
      import_handler
          .Run({"--in", in_path.string(), "--mock-rom", "--format=json"}, &rom,
               &import_output)
          .ok());

  handlers::DungeonExportWaterFillJsonCommandHandler export_handler;
  std::string export_output;
  ASSERT_TRUE(export_handler
                  .Run({"--rooms=0x25,0x27", "--out", out_path.string(),
                        "--format=json"},
                       &rom, &export_output)
                  .ok());

  auto zones_or = zelda3::LoadWaterFillZonesFromJsonString(ReadFile(out_path));
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

  const auto in_path =
      std::filesystem::temp_directory_path() / "yaze_d4_mask_norm.json";
  WriteFile(
      in_path,
      "{\n"
      "  \"version\": 1,\n"
      "  \"zones\": [\n"
      "    { \"room_id\": \"0x27\", \"mask\": \"0x00\", \"offsets\": [50] },\n"
      "    { \"room_id\": \"0x25\", \"mask\": \"0x00\", \"offsets\": [60] }\n"
      "  ]\n"
      "}\n");

  handlers::DungeonImportWaterFillJsonCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--in", in_path.string(), "--mock-rom", "--format=json"}, &rom, &output);
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
  zones.push_back(
      {.room_id = 0x25, .sram_bit_mask = 0x02, .fill_offsets = {1, 2}});
  zones.push_back(
      {.room_id = 0x27, .sram_bit_mask = 0x01, .fill_offsets = {3, 4}});
  ASSERT_TRUE(zelda3::WriteWaterFillTable(&rom, zones).ok());

  const auto out_path = std::filesystem::temp_directory_path() /
                        "yaze_d4_single_room_export.json";
  handlers::DungeonExportWaterFillJsonCommandHandler handler;
  std::string output;
  ASSERT_TRUE(
      handler
          .Run({"--room=0x25", "--out", out_path.string(), "--format=json"},
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
