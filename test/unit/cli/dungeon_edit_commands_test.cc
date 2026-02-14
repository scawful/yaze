#include "cli/handlers/game/dungeon_edit_commands.h"
#include "cli/handlers/game/dungeon_commands.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::cli {
namespace {

using ::testing::HasSubstr;

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

int CountBackupArtifacts(const std::filesystem::path& rom_path) {
  std::error_code ec;
  const auto parent = rom_path.parent_path();
  const std::string prefix = rom_path.filename().string() + "_backup_";

  int count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(parent, ec)) {
    if (ec || !entry.is_regular_file()) {
      continue;
    }
    const std::string filename = entry.path().filename().string();
    if (filename.rfind(prefix, 0) == 0) {
      ++count;
    }
  }
  return count;
}

void CleanupRomArtifacts(const std::filesystem::path& rom_path) {
  std::error_code ec;
  std::filesystem::remove(rom_path, ec);

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

TEST(DungeonEditCommandsTest, PlaceSpriteWriteRelocatesAndRoundTrips) {
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
  ASSERT_TRUE(write_status.ok()) << write_status.message();
  EXPECT_THAT(write_output, HasSubstr("\"write_status\": \"success\""));
  EXPECT_THAT(write_output, HasSubstr("\"save_status\": \"saved\""));
  EXPECT_NE(rom.vector(), before);
  EXPECT_GE(CountBackupArtifacts(cleanup.rom_path), 1);

  const int relocated_pc = ReadRoomSpritePointerPc(rom, table_pc, 0);
  EXPECT_NE(relocated_pc, zelda3::kSpritesData);
  EXPECT_EQ(rom.data()[relocated_pc], 0x00);  // Preserved sort mode.
  EXPECT_EQ(rom.data()[relocated_pc + 1], 0x15);
  EXPECT_EQ(rom.data()[relocated_pc + 2], 0x90);
  EXPECT_EQ(rom.data()[relocated_pc + 3], 0xA3);
  EXPECT_EQ(rom.data()[relocated_pc + 4], 0xFF);

  EXPECT_EQ(rom.data()[zelda3::kSpritesData], 0x00);
  EXPECT_EQ(rom.data()[zelda3::kSpritesData + 1], 0x00);

  handlers::DungeonListSpritesCommandHandler list_handler;
  std::string list_output;
  auto list_status =
      list_handler.Run({"--room=0x00", "--format=json"}, &rom, &list_output);
  ASSERT_TRUE(list_status.ok()) << list_status.message();
  EXPECT_THAT(list_output, HasSubstr("\"total_sprites\": 1"));
  EXPECT_THAT(list_output, HasSubstr("\"sprite_id\": \"0xA3\""));
  EXPECT_THAT(list_output, HasSubstr("\"x\": 16"));
  EXPECT_THAT(list_output, HasSubstr("\"y\": 21"));
  EXPECT_THAT(list_output, HasSubstr("\"subtype\": 4"));
}

}  // namespace
}  // namespace yaze::cli
