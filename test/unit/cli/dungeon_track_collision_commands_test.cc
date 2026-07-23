#include "cli/handlers/game/dungeon_commands.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace yaze::cli {
namespace {

using ::testing::HasSubstr;

constexpr int kObjectPointerTablePc = 0x070000;
constexpr int kObjectDataPc = 0x060000;
constexpr int kGeneratedTrackMapSize = 2 + (4 * 3) + 2;

std::filesystem::path MakeUniqueTempRomPath() {
  static std::atomic<uint64_t> counter{0};
  const uint64_t stamp = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const uint64_t sequence = counter.fetch_add(1, std::memory_order_relaxed);
  return std::filesystem::temp_directory_path() /
         absl::StrFormat("yaze_track_collision_%016llX_%llu.sfc",
                         static_cast<unsigned long long>(stamp),
                         static_cast<unsigned long long>(sequence));
}

std::vector<std::filesystem::path> FindBackupArtifacts(
    const std::filesystem::path& rom_path) {
  std::error_code ec;
  const auto parent = rom_path.parent_path();
  const std::string prefix = rom_path.filename().string() + "_backup_";
  std::vector<std::filesystem::path> backups;

  for (const auto& entry : std::filesystem::directory_iterator(parent, ec)) {
    if (ec || !entry.is_regular_file()) {
      continue;
    }
    const std::string filename = entry.path().filename().string();
    if (filename.rfind(prefix, 0) == 0) {
      backups.push_back(entry.path());
    }
  }
  return backups;
}

void CleanupRomArtifacts(const std::filesystem::path& rom_path) {
  std::error_code ec;
  std::filesystem::remove(rom_path, ec);
  std::filesystem::remove_all(rom_path.string() + ".tmp", ec);
  for (const auto& backup : FindBackupArtifacts(rom_path)) {
    std::filesystem::remove(backup, ec);
  }
}

struct ScopedRomArtifactsCleanup {
  explicit ScopedRomArtifactsCleanup(std::filesystem::path path)
      : rom_path(std::move(path)) {}
  ~ScopedRomArtifactsCleanup() { CleanupRomArtifacts(rom_path); }

  std::filesystem::path rom_path;
};

std::vector<uint8_t> ReadFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(input),
                              std::istreambuf_iterator<char>());
}

void WriteRomFile(const Rom& rom, const std::filesystem::path& path) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output.is_open());
  output.write(reinterpret_cast<const char*>(rom.data()),
               static_cast<std::streamsize>(rom.size()));
  ASSERT_TRUE(output.good());
}

void WriteLong(Rom* rom, int address, uint32_t value) {
  rom->mutable_data()[address] = value & 0xFF;
  rom->mutable_data()[address + 1] = (value >> 8) & 0xFF;
  rom->mutable_data()[address + 2] = (value >> 16) & 0xFF;
}

void SetRoomObjectPointer(Rom* rom, int room_id, int pc_address) {
  WriteLong(rom, kObjectPointerTablePc + (room_id * 3), PcToSnes(pc_address));
}

void WriteTrackObjectStream(Rom* rom, int room_id, int pc_address, int x,
                            int y) {
  SetRoomObjectPointer(rom, room_id, pc_address);
  const zelda3::RoomObject object(0x31, x, y, 0, 0);
  const auto encoded = object.EncodeObjectToBytes();
  ASSERT_TRUE(rom->WriteVector(pc_address,
                               {0x00, 0x00, encoded.b1, encoded.b2, encoded.b3,
                                0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF})
                  .ok());
}

void InitializeTrackCollisionRom(
    Rom* rom, size_t rom_size = zelda3::kCustomCollisionDataEnd) {
  ASSERT_GE(rom_size, 0x100000u);
  ASSERT_TRUE(rom->LoadFromData(std::vector<uint8_t>(rom_size, 0x00)).ok());
  WriteLong(rom, zelda3::kRoomObjectPointer, PcToSnes(kObjectPointerTablePc));
  WriteTrackObjectStream(rom, 0, kObjectDataPc, 10, 10);
  WriteTrackObjectStream(rom, 1, kObjectDataPc + 0x20, 20, 20);
  rom->set_dirty(false);
}

void SetCollisionPointer(Rom* rom, int room_id, int pc_address) {
  WriteLong(rom, zelda3::kCustomCollisionRoomPointers + (room_id * 3),
            PcToSnes(pc_address));
}

void SeedSpaceForExactlyOneGeneratedMap(Rom* rom) {
  const int existing_data =
      zelda3::kCustomCollisionDataSoftEnd - kGeneratedTrackMapSize - 2;
  SetCollisionPointer(rom, 2, existing_data);
  ASSERT_TRUE(rom->WriteVector(existing_data, {0xFF, 0xFF}).ok());
  rom->set_dirty(false);
}

void ExpectValidJson(const std::string& output) {
  const auto report = nlohmann::json::parse(output);
  EXPECT_TRUE(report.is_object());
}

TEST(DungeonTrackCollisionCommandsTest, DryRunLeavesCallerRomUnchanged) {
  Rom rom;
  InitializeTrackCollisionRom(&rom);
  const std::vector<uint8_t> before = rom.vector();

  handlers::DungeonGenerateTrackCollisionCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--room=0x00", "--format=json"}, &rom, &output);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  EXPECT_THAT(output, HasSubstr("\"mode\": \"dry-run\""));
  ExpectValidJson(output);
}

TEST(DungeonTrackCollisionCommandsTest,
     MockWriteCommitsSerializationWithoutDiskSave) {
  Rom rom;
  InitializeTrackCollisionRom(&rom);

  handlers::DungeonGenerateTrackCollisionCommandHandler handler;
  std::string output;
  const absl::Status status = handler.Run(
      {"--mock-rom", "--room=0x00", "--write", "--format=json"}, &rom, &output);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_TRUE(rom.dirty());
  EXPECT_THAT(output, HasSubstr("\"save_status\": \"mock-rom-skipped\""));
  auto collision = zelda3::LoadCustomCollisionMap(&rom, 0);
  ASSERT_TRUE(collision.ok()) << collision.status();
  EXPECT_TRUE(collision->has_data);
  EXPECT_NE(collision->tiles[(10 * 64) + 10], 0);
  ExpectValidJson(output);
}

TEST(DungeonTrackCollisionCommandsTest,
     SerializerFailureReturnsNonOkAndRestoresCaller) {
  Rom rom;
  InitializeTrackCollisionRom(&rom, 0x100000);
  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  WriteRomFile(rom, cleanup.rom_path);
  rom.set_filename(cleanup.rom_path.string());
  rom.set_dirty(false);

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
  const std::string filename_before = rom.filename();

  handlers::DungeonGenerateTrackCollisionCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--room=0x00", "--write", "--format=json"}, &rom, &output);

  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("Custom collision pointer table not present"));
  EXPECT_THAT(output, HasSubstr("\"write_error\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  EXPECT_TRUE(FindBackupArtifacts(cleanup.rom_path).empty());
  ExpectValidJson(output);
}

TEST(DungeonTrackCollisionCommandsTest,
     RequiredBackupFailureRollsBackCallerAndDisk) {
#if defined(_WIN32)
  GTEST_SKIP() << "NAME_MAX backup failure fixture is POSIX-only";
#else
  Rom rom;
  InitializeTrackCollisionRom(&rom);

  const auto parent = std::filesystem::temp_directory_path();
  const long name_max = pathconf(parent.c_str(), _PC_NAME_MAX);
  if (name_max < 128) {
    GTEST_SKIP() << "Filesystem does not expose a usable NAME_MAX";
  }
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  std::string filename = "yaze_track_backup_" + std::to_string(nonce);
  const size_t target_length = static_cast<size_t>(name_max) - 4;
  ASSERT_LT(filename.size(), target_length);
  filename.append(target_length - filename.size(), 'r');

  ScopedRomArtifactsCleanup cleanup(parent / filename);
  WriteRomFile(rom, cleanup.rom_path);
  rom.set_filename(cleanup.rom_path.string());
  rom.set_dirty(false);

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
  const std::string filename_before = rom.filename();

  handlers::DungeonGenerateTrackCollisionCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--room=0x00", "--write", "--format=json"}, &rom, &output);

  EXPECT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()), HasSubstr("required ROM backup"));
  EXPECT_THAT(output, HasSubstr("\"write_status\": \"success\""));
  EXPECT_THAT(output, HasSubstr("\"save_error\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  EXPECT_FALSE(std::filesystem::exists(cleanup.rom_path.string() + ".tmp"));
  EXPECT_TRUE(FindBackupArtifacts(cleanup.rom_path).empty());
  ExpectValidJson(output);
#endif
}

TEST(DungeonTrackCollisionCommandsTest,
     TargetWriteFailureRollsBackAndRetainsRecoveryBackup) {
  Rom rom;
  InitializeTrackCollisionRom(&rom);
  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  WriteRomFile(rom, cleanup.rom_path);
  rom.set_filename(cleanup.rom_path.string());
  rom.set_dirty(false);
  ASSERT_TRUE(
      std::filesystem::create_directory(cleanup.rom_path.string() + ".tmp"));

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
  const std::string filename_before = rom.filename();

  handlers::DungeonGenerateTrackCollisionCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--room=0x00", "--write", "--format=json"}, &rom, &output);

  EXPECT_TRUE(absl::IsInternal(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("Could not open temp ROM file for writing"));
  EXPECT_THAT(output, HasSubstr("\"write_status\": \"success\""));
  EXPECT_THAT(output, HasSubstr("\"save_error\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  const auto backups = FindBackupArtifacts(cleanup.rom_path);
  ASSERT_EQ(backups.size(), 1u);
  EXPECT_EQ(ReadFile(backups.front()), disk_before);
  ExpectValidJson(output);
}

TEST(DungeonTrackCollisionCommandsTest,
     BatchTargetWriteFailureRollsBackEveryRoomAndDisk) {
  Rom rom;
  InitializeTrackCollisionRom(&rom);
  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  WriteRomFile(rom, cleanup.rom_path);
  rom.set_filename(cleanup.rom_path.string());
  rom.set_dirty(false);
  ASSERT_TRUE(
      std::filesystem::create_directory(cleanup.rom_path.string() + ".tmp"));

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
  const std::string filename_before = rom.filename();

  handlers::DungeonGenerateTrackCollisionCommandHandler handler;
  std::string output;
  const absl::Status status = handler.Run(
      {"--rooms=0x00,0x01", "--write", "--format=json"}, &rom, &output);

  EXPECT_TRUE(absl::IsInternal(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("Could not open temp ROM file for writing"));
  EXPECT_THAT(output, HasSubstr("\"save_error\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  const auto backups = FindBackupArtifacts(cleanup.rom_path);
  ASSERT_EQ(backups.size(), 1u);
  EXPECT_EQ(ReadFile(backups.front()), disk_before);
  ExpectValidJson(output);
}

TEST(DungeonTrackCollisionCommandsTest,
     BatchLaterSerializerFailureRollsBackEveryRoom) {
  Rom rom;
  InitializeTrackCollisionRom(&rom);
  SeedSpaceForExactlyOneGeneratedMap(&rom);
  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  WriteRomFile(rom, cleanup.rom_path);
  rom.set_filename(cleanup.rom_path.string());
  rom.set_dirty(false);

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);

  handlers::DungeonGenerateTrackCollisionCommandHandler handler;
  std::string output;
  const absl::Status status = handler.Run(
      {"--rooms=0x00,0x01", "--write", "--format=json"}, &rom, &output);

  EXPECT_TRUE(absl::IsResourceExhausted(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("Write failed for room 0x001"));
  EXPECT_THAT(output, HasSubstr("\"write_status\": \"success\""));
  EXPECT_THAT(output, HasSubstr("\"write_error\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  EXPECT_TRUE(FindBackupArtifacts(cleanup.rom_path).empty());
  ExpectValidJson(output);
}

TEST(DungeonTrackCollisionCommandsTest,
     BatchSaveCreatesOneBackupAndReopensEveryMap) {
  Rom rom;
  InitializeTrackCollisionRom(&rom);
  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  WriteRomFile(rom, cleanup.rom_path);
  rom.set_filename(cleanup.rom_path.string());
  rom.set_dirty(false);
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);

  handlers::DungeonGenerateTrackCollisionCommandHandler handler;
  std::string output;
  const absl::Status status = handler.Run(
      {"--rooms=0x00,0x01", "--write", "--format=json"}, &rom, &output);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("\"save_status\": \"saved\""));
  EXPECT_FALSE(rom.dirty());
  const auto backups = FindBackupArtifacts(cleanup.rom_path);
  ASSERT_EQ(backups.size(), 1u);
  EXPECT_EQ(ReadFile(backups.front()), disk_before);

  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(cleanup.rom_path.string()).ok());
  auto room0 = zelda3::LoadCustomCollisionMap(&reopened, 0);
  auto room1 = zelda3::LoadCustomCollisionMap(&reopened, 1);
  ASSERT_TRUE(room0.ok()) << room0.status();
  ASSERT_TRUE(room1.ok()) << room1.status();
  EXPECT_TRUE(room0->has_data);
  EXPECT_TRUE(room1->has_data);
  EXPECT_NE(room0->tiles[(10 * 64) + 10], 0);
  EXPECT_NE(room1->tiles[(20 * 64) + 20], 0);
  EXPECT_EQ(reopened.vector(), rom.vector());
  ExpectValidJson(output);
}

}  // namespace
}  // namespace yaze::cli
