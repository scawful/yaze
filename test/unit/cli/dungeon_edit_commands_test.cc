#include "cli/handlers/game/dungeon_edit_commands.h"
#include "cli/handlers/game/dungeon_commands.h"

#include <array>
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
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/oracle_rom_safety_preflight.h"
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
constexpr int kObjectAllocationPc = 0x060100;
constexpr int kObjectDataEndPc = 0x060200;
constexpr int kSyntheticPotTerminatorPc = 0x00F000;
constexpr int kRoomHeaderPointerTablePc = 0x020000;
constexpr int kRoomHeaderDataPc = 0x021000;
constexpr std::array<uint8_t, 14> kRoomHeaderSentinel = {
    0xAF, 0x11, 0x22, 0x33, 0x04, 0x05, 0x06,
    0xE4, 0xCF, 0x77, 0x88, 0x99, 0xAA, 0xBB,
};
constexpr uint16_t kRoomMessageSentinel = 0xDDCC;

void ExpectInvalidArgument(const absl::Status& status,
                           const std::string& message_fragment) {
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(status.message()), HasSubstr(message_fragment));
}

std::filesystem::path MakeUniqueTempRomPath() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("yaze_dungeon_place_sprite_relocation_" + std::to_string(now) +
          ".sfc");
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

int CountBackupArtifacts(const std::filesystem::path& rom_path) {
  return static_cast<int>(FindBackupArtifacts(rom_path).size());
}

void CleanupRomArtifacts(const std::filesystem::path& rom_path) {
  std::error_code ec;
  std::filesystem::remove(rom_path, ec);
  std::filesystem::remove_all(rom_path.string() + ".tmp", ec);

  const auto parent = rom_path.parent_path();
  const std::string prefix = rom_path.filename().string() + "_backup_";
  for (const auto& entry : std::filesystem::directory_iterator(parent, ec)) {
    if (ec || !entry.is_regular_file()) {
      continue;
    }
    const std::string filename = entry.path().filename().string();
    if (filename.rfind(prefix, 0) == 0) {
      std::filesystem::remove(entry.path(), ec);
    }
  }
}

struct ScopedRomArtifactsCleanup {
  explicit ScopedRomArtifactsCleanup(std::filesystem::path path)
      : rom_path(std::move(path)) {}
  ~ScopedRomArtifactsCleanup() { CleanupRomArtifacts(rom_path); }

  std::filesystem::path rom_path;
};

struct ScopedFileCleanup {
  explicit ScopedFileCleanup(std::filesystem::path path)
      : file_path(std::move(path)) {}
  ~ScopedFileCleanup() {
    std::error_code ec;
    std::filesystem::remove(file_path, ec);
  }

  std::filesystem::path file_path;
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

void InitializeRoomHeaderRom(Rom* rom) {
  ASSERT_TRUE(rom->LoadFromData(std::vector<uint8_t>(0x100000, 0x00)).ok());

  const uint32_t table_snes = PcToSnes(kRoomHeaderPointerTablePc);
  const uint32_t header_snes = PcToSnes(kRoomHeaderDataPc);
  WriteLong(rom, zelda3::kRoomHeaderPointer, table_snes);
  rom->mutable_data()[zelda3::kRoomHeaderPointerBank] =
      (header_snes >> 16) & 0xFF;
  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    const int pointer = kRoomHeaderPointerTablePc + room_id * 2;
    rom->mutable_data()[pointer] = header_snes & 0xFF;
    rom->mutable_data()[pointer + 1] = (header_snes >> 8) & 0xFF;
  }
  for (size_t index = 0; index < kRoomHeaderSentinel.size(); ++index) {
    rom->mutable_data()[kRoomHeaderDataPc + index] = kRoomHeaderSentinel[index];
  }
  rom->mutable_data()[zelda3::kMessagesIdDungeon] = kRoomMessageSentinel & 0xFF;
  rom->mutable_data()[zelda3::kMessagesIdDungeon + 1] =
      (kRoomMessageSentinel >> 8) & 0xFF;
  rom->set_dirty(false);
}

void SetRoomObjectPointer(Rom* rom, int room_id, int pc_addr) {
  WriteLong(rom, kObjectPointerTablePc + room_id * 3, PcToSnes(pc_addr));
}

int ReadRoomObjectPointerPc(const Rom& rom, int room_id) {
  const int pointer = kObjectPointerTablePc + room_id * 3;
  const uint32_t snes = rom.data()[pointer] |
                        (static_cast<uint32_t>(rom.data()[pointer + 1]) << 8) |
                        (static_cast<uint32_t>(rom.data()[pointer + 2]) << 16);
  return SnesToPc(snes);
}

void WriteEmptyObjectStream(Rom* rom, int pc_addr) {
  const std::vector<uint8_t> stream = {0x00, 0x00, 0xFF, 0xFF, 0xFF,
                                       0xFF, 0xF0, 0xFF, 0xFF, 0xFF};
  ASSERT_TRUE(rom->WriteVector(pc_addr, stream).ok());
}

void InitializeTightObjectRom(Rom* rom) {
  ASSERT_TRUE(rom->LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  WriteLong(rom, zelda3::kRoomObjectPointer, PcToSnes(kObjectPointerTablePc));
  SetRoomObjectPointer(rom, 0, kObjectDataPc);
  for (int room_id = 1; room_id < zelda3::kNumberOfRooms; ++room_id) {
    SetRoomObjectPointer(rom, room_id, kObjectDataPc + 10);
  }
  WriteEmptyObjectStream(rom, kObjectDataPc);
  WriteEmptyObjectStream(rom, kObjectDataPc + 10);

  // Keep LoadRoomFromRom's unrelated pot-item scan bounded in the synthetic
  // fixture so the object-stream assertions stay focused and deterministic.
  const uint16_t pot_pointer = PcToSnes(kSyntheticPotTerminatorPc) & 0xFFFF;
  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    const int pointer = zelda3::kRoomItemsPointers + room_id * 2;
    rom->mutable_data()[pointer] = pot_pointer & 0xFF;
    rom->mutable_data()[pointer + 1] = (pot_pointer >> 8) & 0xFF;
  }
  rom->mutable_data()[kSyntheticPotTerminatorPc] = 0xFF;
  rom->mutable_data()[kSyntheticPotTerminatorPc + 1] = 0xFF;
}

void WriteObjectCowManifest(const std::filesystem::path& path,
                            bool protect_allocation = false) {
  std::string protected_section;
  if (protect_allocation) {
    protected_section = absl::StrFormat(
        R"json(,
  "protected_regions": {
    "total_hooks": 1,
    "regions": [
      {
        "start": "0x%06X",
        "end": "0x%06X",
        "size": %d,
        "hook_count": 1,
        "module": "ObjectAllocatorGuard"
      }
    ]
  })json",
        PcToSnes(kObjectAllocationPc), PcToSnes(kObjectDataEndPc),
        kObjectDataEndPc - kObjectAllocationPc);
  }
  const std::string json = absl::StrFormat(
      R"json({
  "manifest_version": 3,
  "dungeon_stream_regions": {
    "objects": {
      "pointer_table": "0x%06X",
      "pointer_count": 296,
      "pointer_encoding": "long24",
      "strategy": "copy_on_write",
      "data_regions": [
        {"start": "0x%06X", "end": "0x%06X"}
      ],
      "allocation_regions": [
        {"start": "0x%06X", "end": "0x%06X"}
      ]
    }
  }%s
})json",
      PcToSnes(kObjectPointerTablePc), PcToSnes(kObjectDataPc),
      PcToSnes(kObjectDataEndPc), PcToSnes(kObjectAllocationPc),
      PcToSnes(kObjectDataEndPc), protected_section);
  std::ofstream output(path, std::ios::trunc);
  ASSERT_TRUE(output.is_open());
  output << json;
  ASSERT_TRUE(output.good());
}

void SetRoomSpritePointer(Rom* rom, int table_pc, int room_id, int pc_addr) {
  const uint32_t snes = PcToSnes(pc_addr);
  const int ptr_off = table_pc + room_id * 2;
  rom->mutable_data()[ptr_off] = snes & 0xFF;
  rom->mutable_data()[ptr_off + 1] = (snes >> 8) & 0xFF;
}

int ReadRoomSpritePointerPc(const Rom& rom, int table_pc, int room_id) {
  const int ptr_off = table_pc + room_id * 2;
  const uint16_t low16 = (rom.data()[ptr_off + 1] << 8) | rom.data()[ptr_off];
  return SnesToPc(0x090000 | low16);
}

void WriteSpriteStream(Rom* rom, int pc_addr, uint8_t sort_mode,
                       const std::vector<uint8_t>& payload) {
  std::vector<uint8_t> bytes;
  bytes.reserve(payload.size() + 1);
  bytes.push_back(sort_mode);
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  ASSERT_TRUE(rom->WriteVector(pc_addr, std::move(bytes)).ok());
}

std::vector<uint8_t> EncodeSpritePayload(int count, uint8_t id_base = 0x10) {
  std::vector<uint8_t> payload;
  payload.reserve(count * 3 + 1);
  for (int i = 0; i < count; ++i) {
    payload.push_back(0x0A + (i & 0x07));
    payload.push_back(0x0B + (i & 0x07));
    payload.push_back(id_base + i);
  }
  payload.push_back(0xFF);
  return payload;
}

TEST(DungeonEditCommandsTest, PlaceSpriteRejectsInvalidX) {
  handlers::DungeonPlaceSpriteCommandHandler handler;
  std::string output;
  const auto status = handler.Run({"--mock-rom", "--room=0x77", "--id=0xA3",
                                   "--x=nope", "--y=21", "--format=json"},
                                  nullptr, &output);

  ExpectInvalidArgument(status, "Invalid integer for '--x'");
}

TEST(DungeonEditCommandsTest, PlaceSpriteRejectsInvalidSubtype) {
  handlers::DungeonPlaceSpriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--mock-rom", "--room=0x77", "--id=0xA3", "--x=16", "--y=21",
                   "--subtype=nope", "--format=json"},
                  nullptr, &output);

  ExpectInvalidArgument(status, "Invalid integer for '--subtype'");
}

TEST(DungeonEditCommandsTest, RemoveSpriteRejectsInvalidIndex) {
  handlers::DungeonRemoveSpriteCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--room=0x77", "--index=nope", "--format=json"}, nullptr,
      &output);

  ExpectInvalidArgument(status, "Invalid integer for '--index'");
}

TEST(DungeonEditCommandsTest, RemoveSpriteRejectsMixedSelectors) {
  handlers::DungeonRemoveSpriteCommandHandler handler;
  std::string output;
  const auto status = handler.Run({"--mock-rom", "--room=0x77", "--index=1",
                                   "--x=10", "--y=5", "--format=json"},
                                  nullptr, &output);

  ExpectInvalidArgument(status, "Use either --index or --x/--y");
}

TEST(DungeonEditCommandsTest, RemoveSpriteRejectsPartialCoordinates) {
  handlers::DungeonRemoveSpriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--mock-rom", "--room=0x77", "--x=10", "--format=json"},
                  nullptr, &output);

  ExpectInvalidArgument(status, "Both --x and --y are required");
}

TEST(DungeonEditCommandsTest, RemoveSpriteRejectsOutOfRangeCoordinates) {
  handlers::DungeonRemoveSpriteCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--room=0x77", "--x=99", "--y=0", "--format=json"},
      nullptr, &output);

  ExpectInvalidArgument(status, "X must be 0-31");
}

TEST(DungeonEditCommandsTest, PlaceObjectRejectsInvalidSize) {
  handlers::DungeonPlaceObjectCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--mock-rom", "--room=0x98", "--id=0x0031", "--x=1", "--y=2",
                   "--size=nope", "--format=json"},
                  nullptr, &output);

  ExpectInvalidArgument(status, "Invalid integer for '--size'");
}

TEST(DungeonEditCommandsTest, PlaceObjectRejectsRoomIdOutOfRange) {
  handlers::DungeonPlaceObjectCommandHandler handler;
  std::string output;
  const auto status = handler.Run({"--mock-rom", "--room=0x200", "--id=0x0031",
                                   "--x=1", "--y=2", "--format=json"},
                                  nullptr, &output);

  ExpectInvalidArgument(status, "Room ID out of range");
}

TEST(DungeonEditCommandsTest, SetCollisionTileRejectsRoomIdOutOfRange) {
  handlers::DungeonSetCollisionTileCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--room=0x200", "--tiles=10,5,0xB7", "--format=json"},
      nullptr, &output);

  ExpectInvalidArgument(status, "Room ID out of range");
}

TEST(DungeonEditCommandsTest, SetCollisionTileRejectsMalformedTileTuple) {
  handlers::DungeonSetCollisionTileCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--room=0xB8", "--tiles=10,5", "--format=json"}, nullptr,
      &output);

  ExpectInvalidArgument(status, "Invalid tile spec");
}

TEST(DungeonEditCommandsTest,
     SetRoomPropertyRejectsMalformedValueWithStructuredError) {
  Rom rom;
  InitializeRoomHeaderRom(&rom);
  const std::vector<uint8_t> before = rom.vector();

  handlers::DungeonSetRoomPropertyCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--mock-rom", "--room=0x00", "--property=palette",
                   "--value=nope", "--format=json"},
                  &rom, &output);

  ExpectInvalidArgument(status, "Invalid value format");
  const auto report = nlohmann::json::parse(output);
  const auto& result = report.at("Dungeon Room Property Set");
  EXPECT_EQ(result.at("status"), "error");
  EXPECT_THAT(result.at("error").get<std::string>(),
              HasSubstr("Invalid value format"));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST(DungeonEditCommandsTest,
     SetRoomPropertyRejectsUnsupportedPropertyWithStructuredError) {
  Rom rom;
  InitializeRoomHeaderRom(&rom);
  const std::vector<uint8_t> before = rom.vector();

  handlers::DungeonSetRoomPropertyCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--mock-rom", "--room=0x00", "--property=unknown",
                   "--value=0x01", "--format=json"},
                  &rom, &output);

  ExpectInvalidArgument(status, "Unsupported property");
  const auto report = nlohmann::json::parse(output);
  const auto& result = report.at("Dungeon Room Property Set");
  EXPECT_EQ(result.at("status"), "error");
  EXPECT_THAT(result.at("error").get<std::string>(),
              HasSubstr("Unsupported property"));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST(DungeonEditCommandsTest,
     SetRoomPropertyRejectsOutOfRangeRoomIdWithoutMutation) {
  Rom rom;
  InitializeRoomHeaderRom(&rom);
  const std::vector<uint8_t> before = rom.vector();

  handlers::DungeonSetRoomPropertyCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--mock-rom", "--room=0x128", "--property=palette",
                   "--value=0x01", "--format=json"},
                  &rom, &output);

  ExpectInvalidArgument(status, "Invalid room ID");
  const auto report = nlohmann::json::parse(output);
  const auto& result = report.at("Dungeon Room Property Set");
  EXPECT_EQ(result.at("status"), "error");
  EXPECT_THAT(result.at("error").get<std::string>(),
              HasSubstr("Invalid room ID"));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST(DungeonEditCommandsTest, SetRoomPropertyMockRomCommitsSerializedHeader) {
  Rom rom;
  InitializeRoomHeaderRom(&rom);

  handlers::DungeonSetRoomPropertyCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--mock-rom", "--room=0x00", "--property=palette",
                   "--value=0x02", "--format=json"},
                  &rom, &output);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(rom.data()[kRoomHeaderDataPc + 1], 0x02);
  EXPECT_TRUE(rom.dirty());
  EXPECT_THAT(output, HasSubstr("\"save_status\": \"mock-rom-skipped\""));
}

TEST(DungeonEditCommandsTest,
     SetRoomPropertyRejectsUnimplementedObjectStreamPropertiesWithoutMutation) {
  for (const std::string property :
       {"layout", "layout_id", "floor1", "floor2"}) {
    SCOPED_TRACE(property);
    Rom rom;
    InitializeRoomHeaderRom(&rom);
    ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
    WriteRomFile(rom, cleanup.rom_path);
    rom.set_filename(cleanup.rom_path.string());
    rom.set_dirty(false);

    const std::vector<uint8_t> before = rom.vector();
    const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
    const std::string filename_before = rom.filename();

    handlers::DungeonSetRoomPropertyCommandHandler handler;
    std::string output;
    const absl::Status status =
        handler.Run({"--room=0x00", "--property=" + property, "--value=0x02",
                     "--format=json"},
                    &rom, &output);

    EXPECT_TRUE(absl::IsUnimplemented(status)) << status;
    EXPECT_THAT(std::string(status.message()),
                HasSubstr("object-stream header"));
    EXPECT_THAT(std::string(status.message()),
                HasSubstr("persistence is not implemented"));
    EXPECT_THAT(output, HasSubstr("\"status\": \"error\""));
    EXPECT_THAT(output, HasSubstr("\"error\""));
    EXPECT_EQ(rom.vector(), before);
    EXPECT_FALSE(rom.dirty());
    EXPECT_EQ(rom.filename(), filename_before);
    EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
    EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 0);
  }
}

TEST(DungeonEditCommandsTest,
     SetRoomPropertyRequiredBackupFailureRollsBackCallerRom) {
#if defined(_WIN32)
  GTEST_SKIP() << "NAME_MAX backup failure fixture is POSIX-only";
#else
  Rom rom;
  InitializeRoomHeaderRom(&rom);

  const auto parent = std::filesystem::temp_directory_path();
  const long name_max = pathconf(parent.c_str(), _PC_NAME_MAX);
  if (name_max < 128) {
    GTEST_SKIP() << "Filesystem does not expose a usable NAME_MAX";
  }
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  std::string filename = "yaze_room_property_backup_" + std::to_string(nonce);
  const size_t target_length = static_cast<size_t>(name_max) - 4;
  ASSERT_LT(filename.size(), target_length);
  filename.append(target_length - filename.size(), 'r');

  ScopedRomArtifactsCleanup cleanup(parent / filename);
  WriteRomFile(rom, cleanup.rom_path);
  rom.set_filename(cleanup.rom_path.string());
  rom.set_dirty(false);

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
  const size_t size_before = rom.size();
  const bool dirty_before = rom.dirty();
  const std::string filename_before = rom.filename();

  handlers::DungeonSetRoomPropertyCommandHandler handler;
  std::string output;
  const absl::Status status = handler.Run(
      {"--room=0x00", "--property=palette", "--value=0x02", "--format=json"},
      &rom, &output);

  EXPECT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()), HasSubstr("required ROM backup"));
  EXPECT_THAT(output, HasSubstr("\"status\": \"error\""));
  EXPECT_THAT(output, HasSubstr("\"save_error\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.size(), size_before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  EXPECT_FALSE(std::filesystem::exists(cleanup.rom_path.string() + ".tmp"));
  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 0);
#endif
}

TEST(DungeonEditCommandsTest,
     SetRoomPropertyDiskSaveFailureRollsBackCallerRom) {
  Rom rom;
  InitializeRoomHeaderRom(&rom);
  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  WriteRomFile(rom, cleanup.rom_path);
  rom.set_filename(cleanup.rom_path.string());
  rom.set_dirty(false);
  ASSERT_TRUE(
      std::filesystem::create_directory(cleanup.rom_path.string() + ".tmp"));

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
  const size_t size_before = rom.size();
  const bool dirty_before = rom.dirty();
  const std::string filename_before = rom.filename();

  handlers::DungeonSetRoomPropertyCommandHandler handler;
  std::string output;
  const absl::Status status = handler.Run(
      {"--room=0x00", "--property=palette", "--value=0x02", "--format=json"},
      &rom, &output);

  EXPECT_TRUE(absl::IsInternal(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("Could not open temp ROM file for writing"));
  EXPECT_THAT(output, HasSubstr("\"status\": \"error\""));
  EXPECT_THAT(output, HasSubstr("\"save_error\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.size(), size_before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  const auto backups = FindBackupArtifacts(cleanup.rom_path);
  ASSERT_EQ(backups.size(), 1u);
  EXPECT_EQ(ReadFile(backups.front()), disk_before);
}

TEST(DungeonEditCommandsTest, SetRoomPropertySavesBackupAndReopens) {
  Rom rom;
  InitializeRoomHeaderRom(&rom);
  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  WriteRomFile(rom, cleanup.rom_path);
  rom.set_filename(cleanup.rom_path.string());
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);

  handlers::DungeonSetRoomPropertyCommandHandler handler;
  std::string output;
  const absl::Status status = handler.Run(
      {"--room=0x00", "--property=palette", "--value=0x02", "--format=json"},
      &rom, &output);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("\"save_status\": \"saved\""));
  std::vector<uint8_t> expected = disk_before;
  expected[kRoomHeaderDataPc + 1] = 0x02;
  EXPECT_EQ(rom.vector(), expected);
  EXPECT_EQ(ReadFile(cleanup.rom_path), expected);
  EXPECT_FALSE(rom.dirty());
  const auto backups = FindBackupArtifacts(cleanup.rom_path);
  ASSERT_EQ(backups.size(), 1u);
  EXPECT_EQ(ReadFile(backups.front()), disk_before);

  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(cleanup.rom_path.string()).ok());
  const zelda3::Room room = zelda3::LoadRoomHeaderFromRom(&reopened, 0);
  EXPECT_EQ(room.bg2(), background2::DarkRoom);
  EXPECT_EQ(room.collision(), static_cast<zelda3::CollisionKey>(3));
  EXPECT_EQ(room.palette(), 0x02);
  EXPECT_EQ(room.blockset(), 0x22);
  EXPECT_EQ(room.spriteset(), 0x33);
  EXPECT_EQ(room.effect(), static_cast<zelda3::EffectKey>(0x04));
  EXPECT_EQ(room.tag1(), static_cast<zelda3::TagKey>(0x05));
  EXPECT_EQ(room.tag2(), static_cast<zelda3::TagKey>(0x06));
  EXPECT_EQ(room.staircase_plane(0), 0x01);
  EXPECT_EQ(room.staircase_plane(1), 0x02);
  EXPECT_EQ(room.staircase_plane(2), 0x03);
  EXPECT_EQ(room.staircase_plane(3), 0x03);
  EXPECT_EQ(room.holewarp(), 0x77);
  EXPECT_EQ(room.staircase_room(0), 0x88);
  EXPECT_EQ(room.staircase_room(1), 0x99);
  EXPECT_EQ(room.staircase_room(2), 0xAA);
  EXPECT_EQ(room.staircase_room(3), 0xBB);
  EXPECT_EQ(room.message_id(), kRoomMessageSentinel);
}

TEST(DungeonEditCommandsTest,
     PlaceSpriteWriteFailsClosedWhenRepackingRequired) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  {
    std::ofstream seed(cleanup.rom_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(seed.is_open());
    seed.write(reinterpret_cast<const char*>(rom.data()),
               static_cast<std::streamsize>(rom.size()));
    ASSERT_TRUE(seed.good());
  }
  rom.set_filename(cleanup.rom_path.string());

  rom.mutable_data()[zelda3::kRoomsSpritePointer] = 0x00;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 1] = 0x80;
  const int table_pc = SnesToPc(0x098000);

  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    SetRoomSpritePointer(&rom, table_pc, room_id, zelda3::kSpritesData + 0x20);
  }
  SetRoomSpritePointer(&rom, table_pc, 0, zelda3::kSpritesData);
  SetRoomSpritePointer(&rom, table_pc, 1, zelda3::kSpritesData + 0x02);

  const std::vector<uint8_t> empty_payload = {0xFF};
  WriteSpriteStream(&rom, zelda3::kSpritesData, 0x00, empty_payload);
  WriteSpriteStream(&rom, zelda3::kSpritesData + 0x02, 0x00, empty_payload);
  WriteSpriteStream(&rom, zelda3::kSpritesData + 0x20, 0x00, empty_payload);

  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 0);

  const auto before = rom.vector();
  handlers::DungeonPlaceSpriteCommandHandler place_handler;

  std::string dry_output;
  auto dry_status =
      place_handler.Run({"--room=0x00", "--id=0xA3", "--x=16", "--y=21",
                         "--subtype=4", "--format=json"},
                        &rom, &dry_output);
  ASSERT_TRUE(dry_status.ok()) << dry_status.message();
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 0);
  EXPECT_THAT(dry_output, HasSubstr("\"mode\": \"dry-run\""));

  std::string write_output;
  auto write_status =
      place_handler.Run({"--room=0x00", "--id=0xA3", "--x=16", "--y=21",
                         "--subtype=4", "--write", "--format=json"},
                        &rom, &write_output);
  EXPECT_TRUE(absl::IsResourceExhausted(write_status))
      << write_status.message();
  EXPECT_THAT(write_output, HasSubstr("repacking is required"));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(ReadRoomSpritePointerPc(rom, table_pc, 0), zelda3::kSpritesData);
  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 0);
}

TEST(DungeonEditCommandsTest,
     PlaceSpriteRequiredBackupFailureRollsBackCallerRom) {
#if defined(_WIN32)
  GTEST_SKIP() << "NAME_MAX backup failure fixture is POSIX-only";
#else
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());

  rom.mutable_data()[zelda3::kRoomsSpritePointer] = 0x00;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 1] = 0x80;
  const int table_pc = SnesToPc(0x098000);
  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    SetRoomSpritePointer(&rom, table_pc, room_id, zelda3::kSpritesData + 0x20);
  }
  SetRoomSpritePointer(&rom, table_pc, 0, zelda3::kSpritesData);
  WriteSpriteStream(&rom, zelda3::kSpritesData, 0x00, EncodeSpritePayload(0));
  WriteSpriteStream(&rom, zelda3::kSpritesData + 0x20, 0x00,
                    EncodeSpritePayload(0));

  const auto parent = std::filesystem::temp_directory_path();
  const long name_max = pathconf(parent.c_str(), _PC_NAME_MAX);
  if (name_max < 128) {
    GTEST_SKIP() << "Filesystem does not expose a usable NAME_MAX";
  }
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  std::string filename = "yaze_required_backup_" + std::to_string(nonce);
  const size_t target_length = static_cast<size_t>(name_max) - 4;
  ASSERT_LT(filename.size(), target_length);
  filename.append(target_length - filename.size(), 'r');

  ScopedRomArtifactsCleanup cleanup(parent / filename);
  WriteRomFile(rom, cleanup.rom_path);
  rom.set_filename(cleanup.rom_path.string());
  rom.set_dirty(false);

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
  const int pointer_before = ReadRoomSpritePointerPc(rom, table_pc, 0);
  const bool dirty_before = rom.dirty();
  const std::string filename_before = rom.filename();

  handlers::DungeonPlaceSpriteCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--room=0x00", "--id=0xA3", "--x=16", "--y=21",
                   "--subtype=4", "--write", "--format=json"},
                  &rom, &output);

  EXPECT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()), HasSubstr("required ROM backup"));
  EXPECT_THAT(output, HasSubstr("\"write_status\": \"success\""));
  EXPECT_THAT(output, HasSubstr("\"save_error\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_EQ(ReadRoomSpritePointerPc(rom, table_pc, 0), pointer_before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  EXPECT_FALSE(std::filesystem::exists(cleanup.rom_path.string() + ".tmp"));
  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 0);
#endif
}

TEST(DungeonEditCommandsTest,
     PlaceObjectDryRunMatchesManifestlessWriteFailure) {
  Rom rom;
  InitializeTightObjectRom(&rom);
  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  WriteRomFile(rom, cleanup.rom_path);
  rom.set_filename(cleanup.rom_path.string());

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
  handlers::DungeonPlaceObjectCommandHandler handler;
  const std::vector<std::string> args = {"--room=0x00", "--id=0x0031",
                                         "--x=1",       "--y=2",
                                         "--size=6",    "--format=json"};

  std::string dry_output;
  const absl::Status dry_status = handler.Run(args, &rom, &dry_output);
  EXPECT_TRUE(absl::IsResourceExhausted(dry_status)) << dry_status;
  EXPECT_THAT(dry_output, HasSubstr("\"mode\": \"dry-run\""));
  EXPECT_THAT(dry_output, HasSubstr("\"preflight_status\": \"failed\""));
  EXPECT_THAT(dry_output, HasSubstr("object data too large"));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  EXPECT_EQ(ReadRoomObjectPointerPc(rom, 0), kObjectDataPc);
  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 0);

  std::vector<std::string> write_args = args;
  write_args.push_back("--write");
  std::string write_output;
  const absl::Status write_status =
      handler.Run(write_args, &rom, &write_output);
  EXPECT_EQ(write_status.code(), dry_status.code());
  EXPECT_EQ(write_status.message(), dry_status.message());
  EXPECT_THAT(write_output, HasSubstr("\"mode\": \"write\""));
  EXPECT_THAT(write_output, HasSubstr("\"preflight_status\": \"failed\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  EXPECT_EQ(ReadRoomObjectPointerPc(rom, 0), kObjectDataPc);
  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 0);
}

TEST(DungeonEditCommandsTest,
     PlaceObjectManifestDryRunAndWriteRelocatesAndReopens) {
  Rom rom;
  InitializeTightObjectRom(&rom);
  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  ScopedFileCleanup manifest_cleanup(cleanup.rom_path.string() +
                                     ".manifest.json");
  WriteRomFile(rom, cleanup.rom_path);
  WriteObjectCowManifest(manifest_cleanup.file_path);
  rom.set_filename(cleanup.rom_path.string());

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
  handlers::DungeonPlaceObjectCommandHandler handler;
  const std::vector<std::string> args = {
      "--room=0x00",
      "--id=0x0031",
      "--x=1",
      "--y=2",
      "--size=6",
      absl::StrFormat("--manifest=%s", manifest_cleanup.file_path.string()),
      "--format=json"};

  std::string dry_output;
  const absl::Status dry_status = handler.Run(args, &rom, &dry_output);
  ASSERT_TRUE(dry_status.ok()) << dry_status;
  EXPECT_THAT(dry_output,
              HasSubstr("\"allocator_capability\": \"copy_on_write\""));
  EXPECT_THAT(dry_output, HasSubstr("\"preflight_status\": \"success\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  EXPECT_EQ(ReadRoomObjectPointerPc(rom, 0), kObjectDataPc);
  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 0);

  std::vector<std::string> write_args = args;
  write_args.push_back("--write");
  std::string write_output;
  const absl::Status write_status =
      handler.Run(write_args, &rom, &write_output);
  ASSERT_TRUE(write_status.ok()) << write_status;
  EXPECT_THAT(write_output, HasSubstr("\"write_status\": \"success\""));
  EXPECT_THAT(write_output, HasSubstr("\"save_status\": \"saved\""));
  EXPECT_EQ(ReadRoomObjectPointerPc(rom, 0), kObjectAllocationPc);
  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 1);

  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(cleanup.rom_path.string()).ok());
  EXPECT_EQ(ReadRoomObjectPointerPc(reopened, 0), kObjectAllocationPc);
  zelda3::Room room = zelda3::LoadRoomFromRom(&reopened, 0);
  ASSERT_EQ(room.GetTileObjects().size(), 1u);
  const zelda3::RoomObject& object = room.GetTileObjects().front();
  EXPECT_EQ(object.id_, 0x0031);
  EXPECT_EQ(object.x(), 1);
  EXPECT_EQ(object.y(), 2);
  EXPECT_EQ(object.size(), 6);
  EXPECT_EQ(object.GetLayerValue(), 0);

  zelda3::Room untouched = zelda3::LoadRoomFromRom(&reopened, 1);
  EXPECT_TRUE(untouched.GetTileObjects().empty());
  EXPECT_EQ(ReadRoomObjectPointerPc(reopened, 1), kObjectDataPc + 10);
}

TEST(DungeonEditCommandsTest,
     PlaceObjectDiskSaveFailureRollsBackCowPlanAndCallerRom) {
  Rom rom;
  InitializeTightObjectRom(&rom);
  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  ScopedFileCleanup manifest_cleanup(cleanup.rom_path.string() +
                                     ".manifest.json");
  WriteRomFile(rom, cleanup.rom_path);
  WriteObjectCowManifest(manifest_cleanup.file_path);
  rom.set_filename(cleanup.rom_path.string());
  rom.set_dirty(false);
  ASSERT_TRUE(
      std::filesystem::create_directory(cleanup.rom_path.string() + ".tmp"));

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
  const int pointer_before = ReadRoomObjectPointerPc(rom, 0);
  const bool dirty_before = rom.dirty();
  const std::string filename_before = rom.filename();

  handlers::DungeonPlaceObjectCommandHandler handler;
  std::string output;
  const absl::Status status = handler.Run(
      {"--room=0x00", "--id=0x0031", "--x=1", "--y=2", "--size=6",
       absl::StrFormat("--manifest=%s", manifest_cleanup.file_path.string()),
       "--write", "--format=json"},
      &rom, &output);

  EXPECT_TRUE(absl::IsInternal(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("Could not open temp ROM file for writing"));
  EXPECT_THAT(output, HasSubstr("\"preflight_status\": \"success\""));
  EXPECT_THAT(output, HasSubstr("\"write_status\": \"success\""));
  EXPECT_THAT(output, HasSubstr("\"save_error\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(rom.filename(), filename_before);
  EXPECT_EQ(ReadRoomObjectPointerPc(rom, 0), pointer_before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 1);
}

TEST(DungeonEditCommandsTest,
     PlaceObjectProtectedCowRangeRejectsDryRunAndWriteBeforeMutation) {
  Rom rom;
  InitializeTightObjectRom(&rom);
  ScopedRomArtifactsCleanup cleanup(MakeUniqueTempRomPath());
  ScopedFileCleanup manifest_cleanup(cleanup.rom_path.string() +
                                     ".manifest.json");
  WriteRomFile(rom, cleanup.rom_path);
  WriteObjectCowManifest(manifest_cleanup.file_path,
                         /*protect_allocation=*/true);
  rom.set_filename(cleanup.rom_path.string());

  const std::vector<uint8_t> before = rom.vector();
  const std::vector<uint8_t> disk_before = ReadFile(cleanup.rom_path);
  auto disk_hash_before = zelda3::ComputeSha256(cleanup.rom_path.string());
  ASSERT_TRUE(disk_hash_before.ok()) << disk_hash_before.status();
  const int pointer_before = ReadRoomObjectPointerPc(rom, 0);
  const bool dirty_before = rom.dirty();

  handlers::DungeonPlaceObjectCommandHandler handler;
  const std::vector<std::string> args = {
      "--room=0x00",
      "--id=0x0031",
      "--x=1",
      "--y=2",
      "--size=6",
      absl::StrFormat("--manifest=%s", manifest_cleanup.file_path.string()),
      "--format=json"};

  std::string dry_output;
  const absl::Status dry_status = handler.Run(args, &rom, &dry_output);
  EXPECT_EQ(dry_status.code(), absl::StatusCode::kPermissionDenied)
      << dry_status;
  EXPECT_EQ(dry_status.message(), "Write conflict with Hack Manifest");
  EXPECT_THAT(dry_output, HasSubstr("\"mode\": \"dry-run\""));
  EXPECT_THAT(dry_output, HasSubstr("\"manifest_write_policy\": \"block\""));
  EXPECT_THAT(dry_output, HasSubstr("\"preflight_status\": \"failed\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  auto disk_hash_after_dry = zelda3::ComputeSha256(cleanup.rom_path.string());
  ASSERT_TRUE(disk_hash_after_dry.ok()) << disk_hash_after_dry.status();
  EXPECT_EQ(*disk_hash_after_dry, *disk_hash_before);
  EXPECT_EQ(ReadRoomObjectPointerPc(rom, 0), pointer_before);
  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 0);

  std::vector<std::string> write_args = args;
  write_args.push_back("--write");
  std::string write_output;
  const absl::Status write_status =
      handler.Run(write_args, &rom, &write_output);
  EXPECT_EQ(write_status.code(), dry_status.code()) << write_status;
  EXPECT_EQ(write_status.message(), dry_status.message());
  EXPECT_THAT(write_output, HasSubstr("\"mode\": \"write\""));
  EXPECT_THAT(write_output, HasSubstr("\"preflight_status\": \"failed\""));
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(ReadFile(cleanup.rom_path), disk_before);
  auto disk_hash_after_write = zelda3::ComputeSha256(cleanup.rom_path.string());
  ASSERT_TRUE(disk_hash_after_write.ok()) << disk_hash_after_write.status();
  EXPECT_EQ(*disk_hash_after_write, *disk_hash_before);
  EXPECT_EQ(ReadRoomObjectPointerPc(rom, 0), pointer_before);
  EXPECT_EQ(CountBackupArtifacts(cleanup.rom_path), 0);
}

}  // namespace
}  // namespace yaze::cli
