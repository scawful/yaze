#include "cli/handlers/game/dungeon_stream_plan_commands.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "cli/service/command_registry.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::cli::handlers {
namespace {

using ::testing::HasSubstr;
using json = nlohmann::json;

constexpr uint32_t kRomSize = 0x100000;
constexpr uint32_t kPotData = 0x00E000;
constexpr uint32_t kObjectPointerTable = 0x070000;
constexpr uint32_t kObjectData = 0x060000;
constexpr uint32_t kSpritePointerTable = 0x048000;
constexpr uint32_t kSpriteData = 0x049000;

class TempManifest {
 public:
  explicit TempManifest(const std::string& contents) {
    static std::atomic<uint64_t> sequence = 0;
    const uint64_t timestamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    path_ = std::filesystem::temp_directory_path() /
            absl::StrCat("yaze_dungeon_stream_plan_", timestamp, "_",
                         sequence.fetch_add(1), ".json");
    std::ofstream output(path_, std::ios::binary | std::ios::trunc);
    output << contents;
  }

  ~TempManifest() {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

std::string PotManifestJson() {
  return absl::StrFormat(
      R"json({
        "manifest_version": 3,
        "hack_name": "dungeon stream plan test",
        "dungeon_stream_regions": {
          "pot_items": {
            "pointer_table": "0x%06X",
            "pointer_count": 4,
            "pointer_encoding": "bank16",
            "pointer_bank": "0x01",
            "strategy": "repack_all",
            "data_regions": [
              {"start": "0x%06X", "end": "0x%06X"}
            ],
            "allocation_regions": [
              {"start": "0x%06X", "end": "0x%06X"}
            ]
          }
        }
      })json",
      PcToSnes(zelda3::kRoomItemsPointers), PcToSnes(kPotData),
      PcToSnes(kPotData + 0x40), PcToSnes(kPotData + 0x20),
      PcToSnes(kPotData + 0x40));
}

std::string ObjectManifestJson() {
  return absl::StrFormat(
      R"json({
        "manifest_version": 3,
        "hack_name": "object stream plan test",
        "dungeon_stream_regions": {
          "objects": {
            "pointer_table": "0x%06X",
            "pointer_count": 1,
            "pointer_encoding": "long24",
            "strategy": "copy_on_write",
            "data_regions": [
              {"start": "0x%06X", "end": "0x%06X"}
            ],
            "allocation_regions": [
              {"start": "0x%06X", "end": "0x%06X"}
            ]
          }
        }
      })json",
      PcToSnes(kObjectPointerTable), PcToSnes(kObjectData),
      PcToSnes(kObjectData + 0x40), PcToSnes(kObjectData + 0x20),
      PcToSnes(kObjectData + 0x40));
}

std::string SpriteManifestJson() {
  return absl::StrFormat(
      R"json({
        "manifest_version": 3,
        "hack_name": "sprite stream plan test",
        "dungeon_stream_regions": {
          "sprites": {
            "pointer_table": "0x%06X",
            "pointer_count": 1,
            "pointer_encoding": "bank16",
            "pointer_bank": "0x09",
            "strategy": "copy_on_write",
            "data_regions": [
              {"start": "0x%06X", "end": "0x%06X"}
            ],
            "allocation_regions": [
              {"start": "0x%06X", "end": "0x%06X"}
            ]
          }
        }
      })json",
      PcToSnes(kSpritePointerTable), PcToSnes(kSpriteData),
      PcToSnes(kSpriteData + 0x100), PcToSnes(kSpriteData + 0x80),
      PcToSnes(kSpriteData + 0x100));
}

void WriteWord(Rom* rom, uint32_t address, uint16_t value) {
  rom->mutable_data()[address] = static_cast<uint8_t>(value & 0xFF);
  rom->mutable_data()[address + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void WriteLong(Rom* rom, uint32_t address, uint32_t value) {
  rom->mutable_data()[address] = static_cast<uint8_t>(value & 0xFF);
  rom->mutable_data()[address + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  rom->mutable_data()[address + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
}

void SetPotPointer(Rom* rom, uint32_t room_id, uint32_t data_pc) {
  WriteWord(rom, zelda3::kRoomItemsPointers + room_id * 2,
            static_cast<uint16_t>(PcToSnes(data_pc) & 0xFFFF));
}

Rom MakePotRom() {
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());

  // Rooms 0/1 are exact aliases. Room 2 starts at their terminator, which is
  // a valid suffix stream. Room 3 has a separate empty stream.
  const std::vector<uint8_t> first_stream = {0x01, 0x00, 0x10, 0xFF, 0xFF};
  std::copy(first_stream.begin(), first_stream.end(),
            rom.mutable_data() + kPotData);
  rom.mutable_data()[kPotData + 0x10] = 0xFF;
  rom.mutable_data()[kPotData + 0x11] = 0xFF;
  SetPotPointer(&rom, 0, kPotData);
  SetPotPointer(&rom, 1, kPotData);
  SetPotPointer(&rom, 2, kPotData + 3);
  SetPotPointer(&rom, 3, kPotData + 0x10);
  rom.ClearDirty();
  return rom;
}

Rom MakeObjectRom() {
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());
  WriteLong(&rom, zelda3::kRoomObjectPointer, PcToSnes(kObjectPointerTable));
  WriteLong(&rom, kObjectPointerTable, PcToSnes(kObjectData));
  const std::vector<uint8_t> empty_stream = {
      0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF,
  };
  std::copy(empty_stream.begin(), empty_stream.end(),
            rom.mutable_data() + kObjectData);
  rom.ClearDirty();
  return rom;
}

Rom MakeSpriteRom() {
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());
  WriteWord(&rom, zelda3::kRoomsSpritePointer,
            static_cast<uint16_t>(PcToSnes(kSpritePointerTable) & 0xFFFF));
  WriteWord(&rom, kSpritePointerTable,
            static_cast<uint16_t>(PcToSnes(kSpriteData) & 0xFFFF));
  rom.mutable_data()[kSpriteData] = 0x00;
  rom.mutable_data()[kSpriteData + 1] = 0xFF;
  rom.ClearDirty();
  return rom;
}

TEST(DungeonStreamPlanCommandsTest,
     ReportsAliasesOverlapsAndManifestOwnedCapacityWithoutMutation) {
  Rom rom = MakePotRom();
  const std::vector<uint8_t> before = rom.vector();
  const bool dirty_before = rom.dirty();
  TempManifest manifest(PotManifestJson());
  DungeonStreamPlanCommandHandler handler;

  std::string output;
  const absl::Status status =
      handler.Run({"--kind=pot_items", "--manifest=" + manifest.path().string(),
                   "--format=json"},
                  &rom, &output);

  ASSERT_TRUE(status.ok()) << status;
  const json report = json::parse(output);
  EXPECT_EQ(report.at("status"), "ok");
  EXPECT_EQ(report.at("kind"), "pot_items");
  EXPECT_EQ(report.at("strategy"), "repack_all");
  EXPECT_EQ(report.at("pointer_table_pc"), "0x00DB69");
  EXPECT_EQ(report.at("pointer_count"), 4);
  EXPECT_EQ(report.at("unique_stream_count"), 3);
  EXPECT_EQ(report.at("exact_alias_group_count"), 1);
  EXPECT_EQ(report.at("exact_aliased_room_count"), 2);
  EXPECT_EQ(report.at("suffix_overlap_count"), 1);
  EXPECT_EQ(report.at("interior_overlap_count"), 0);
  EXPECT_EQ(report.at("issue_count"), 0);
  EXPECT_EQ(report.at("occupied_bytes"), 7);
  EXPECT_EQ(report.at("free_bytes"), 57);
  EXPECT_EQ(report.at("allocatable_bytes"), 32);
  ASSERT_EQ(report.at("exact_aliases").size(), 1);
  EXPECT_EQ(report.at("exact_aliases")[0].at("room_ids"),
            (json::array({"0x000", "0x001"})));
  ASSERT_EQ(report.at("overlaps").size(), 1);
  EXPECT_EQ(report.at("overlaps")[0].at("kind"), "suffix");
  EXPECT_EQ(report.at("occupied_intervals").size(), 2);
  EXPECT_EQ(report.at("allocatable_intervals").size(), 1);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.dirty(), dirty_before);
}

TEST(DungeonStreamPlanCommandsTest,
     FormatsDiagnosticsThenFailsWhenInventoryHasIssues) {
  Rom rom = MakePotRom();
  SetPotPointer(&rom, 3, kPotData + 0x100);
  rom.ClearDirty();
  const std::vector<uint8_t> before = rom.vector();
  TempManifest manifest(PotManifestJson());
  DungeonStreamPlanCommandHandler handler;

  std::string output;
  const absl::Status status =
      handler.Run({"--kind", "pot_items", "--manifest",
                   manifest.path().string(), "--format", "json"},
                  &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  const json report = json::parse(output);
  EXPECT_EQ(report.at("status"), "issues");
  EXPECT_EQ(report.at("issue_count"), 1);
  ASSERT_EQ(report.at("issues").size(), 1);
  EXPECT_EQ(report.at("issues")[0].at("code"), "pointer_outside_data_ranges");
  EXPECT_EQ(report.at("issues")[0].at("room_id"), "0x003");
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST(DungeonStreamPlanCommandsTest, SupportsObjectAndSpriteInventories) {
  DungeonStreamPlanCommandHandler handler;

  Rom object_rom = MakeObjectRom();
  const auto object_before = object_rom.vector();
  TempManifest object_manifest(ObjectManifestJson());
  std::string object_output;
  const absl::Status object_status = handler.Run(
      {"--kind=objects", "--manifest=" + object_manifest.path().string(),
       "--format=json"},
      &object_rom, &object_output);
  ASSERT_TRUE(object_status.ok()) << object_status;
  const json object_report = json::parse(object_output);
  EXPECT_EQ(object_report.at("kind"), "objects");
  EXPECT_EQ(object_report.at("pointer_encoding"), "long24");
  EXPECT_EQ(object_report.at("unique_stream_count"), 1);
  EXPECT_EQ(object_report.at("issue_count"), 0);
  EXPECT_EQ(object_rom.vector(), object_before);
  EXPECT_FALSE(object_rom.dirty());

  Rom sprite_rom = MakeSpriteRom();
  const auto sprite_before = sprite_rom.vector();
  TempManifest sprite_manifest(SpriteManifestJson());
  std::string sprite_output;
  const absl::Status sprite_status = handler.Run(
      {"--kind=sprites", "--manifest=" + sprite_manifest.path().string(),
       "--format=json"},
      &sprite_rom, &sprite_output);
  ASSERT_TRUE(sprite_status.ok()) << sprite_status;
  const json sprite_report = json::parse(sprite_output);
  EXPECT_EQ(sprite_report.at("kind"), "sprites");
  EXPECT_EQ(sprite_report.at("pointer_encoding"), "bank16");
  EXPECT_EQ(sprite_report.at("pointer_bank"), "0x09");
  EXPECT_EQ(sprite_report.at("unique_stream_count"), 1);
  EXPECT_EQ(sprite_report.at("issue_count"), 0);
  EXPECT_EQ(sprite_rom.vector(), sprite_before);
  EXPECT_FALSE(sprite_rom.dirty());
}

TEST(DungeonStreamPlanCommandsTest, RejectsInvalidStreamKind) {
  Rom rom = MakePotRom();
  TempManifest manifest(PotManifestJson());
  DungeonStreamPlanCommandHandler handler;
  std::string output;

  const absl::Status status =
      handler.Run({"--kind=doors", "--manifest=" + manifest.path().string(),
                   "--format=json"},
                  &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("objects, sprites, pot_items"));
}

TEST(DungeonStreamPlanCommandsTest, RejectsWriteFlag) {
  Rom rom = MakePotRom();
  TempManifest manifest(PotManifestJson());
  DungeonStreamPlanCommandHandler handler;
  std::string output;

  const absl::Status status =
      handler.Run({"--kind=pot_items", "--manifest=" + manifest.path().string(),
                   "--write", "--format=json"},
                  &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("read-only"));
}

TEST(DungeonStreamPlanCommandsTest, RequiresSelectedManifestLayout) {
  Rom rom = MakePotRom();
  TempManifest manifest(
      R"json({"manifest_version":3,"hack_name":"no layouts"})json");
  DungeonStreamPlanCommandHandler handler;
  std::string output;

  const absl::Status status =
      handler.Run({"--kind=pot_items", "--manifest=" + manifest.path().string(),
                   "--format=json"},
                  &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(status.message(), HasSubstr("dungeon_stream_regions.pot_items"));
}

TEST(DungeonStreamPlanCommandsTest, MissingManifestFailsClosed) {
  Rom rom = MakePotRom();
  DungeonStreamPlanCommandHandler handler;
  std::string output;
  const auto missing =
      std::filesystem::temp_directory_path() /
      absl::StrCat("yaze_missing_dungeon_stream_manifest_",
                   std::chrono::steady_clock::now().time_since_epoch().count(),
                   ".json");

  const absl::Status status = handler.Run(
      {"--kind=pot_items", "--manifest=" + missing.string(), "--format=json"},
      &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
}

TEST(DungeonStreamPlanCommandsTest, SupportsTextOutput) {
  Rom rom = MakePotRom();
  TempManifest manifest(PotManifestJson());
  DungeonStreamPlanCommandHandler handler;
  std::string output;

  const absl::Status status =
      handler.Run({"--kind=pot_items", "--manifest=" + manifest.path().string(),
                   "--format=text"},
                  &rom, &output);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("pointer_table_pc"));
  EXPECT_THAT(output, HasSubstr("allocatable_bytes"));
}

TEST(DungeonStreamPlanCommandsTest, IsRegisteredWithReadOnlyHelp) {
  auto& registry = CommandRegistry::Instance();
  ASSERT_TRUE(registry.HasCommand("dungeon-stream-plan"));
  const std::string help = registry.GenerateHelp("dungeon-stream-plan");
  EXPECT_THAT(help, HasSubstr("--kind <objects|sprites|pot_items>"));
  EXPECT_THAT(help, HasSubstr("without modifying the ROM"));
}

}  // namespace
}  // namespace yaze::cli::handlers
