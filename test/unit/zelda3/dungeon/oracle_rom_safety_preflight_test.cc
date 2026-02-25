#include "zelda3/dungeon/oracle_rom_safety_preflight.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cli/handlers/game/dungeon_collision_commands.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::zelda3 {
namespace {

bool HasErrorCode(const OracleRomSafetyPreflightResult& result,
                  const std::string& code) {
  for (const auto& err : result.errors) {
    if (err.code == code) {
      return true;
    }
  }
  return false;
}

TEST(OracleRomSafetyPreflightTest, FailsWhenWaterFillRegionMissing) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0)).ok());

  OracleRomSafetyPreflightOptions options;
  options.require_water_fill_reserved_region = true;
  options.validate_water_fill_table = true;
  options.validate_custom_collision_maps = false;

  const auto result = RunOracleRomSafetyPreflight(&rom, options);
  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(HasErrorCode(result, "ORACLE_WATER_FILL_REGION_MISSING"));
}

TEST(OracleRomSafetyPreflightTest, FailsWhenWaterFillHeaderIsCorrupt) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  ASSERT_TRUE(rom.WriteByte(kWaterFillTableStart, 0x09).ok());  // > 8 invalid

  OracleRomSafetyPreflightOptions options;
  options.require_water_fill_reserved_region = true;
  options.validate_water_fill_table = true;
  options.validate_custom_collision_maps = false;

  const auto result = RunOracleRomSafetyPreflight(&rom, options);
  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(HasErrorCode(result, "ORACLE_WATER_FILL_HEADER_CORRUPT"));
}

TEST(OracleRomSafetyPreflightTest,
     FailsWhenCustomCollisionPointerOverlapsWaterFillRegion) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  const uint32_t snes_ptr = PcToSnes(kWaterFillTableStart);
  const int ptr_offset = kCustomCollisionRoomPointers;  // room 0 pointer
  ASSERT_TRUE(
      rom.WriteByte(ptr_offset + 0, static_cast<uint8_t>(snes_ptr & 0xFF)).ok());
  ASSERT_TRUE(
      rom.WriteByte(ptr_offset + 1, static_cast<uint8_t>((snes_ptr >> 8) & 0xFF))
          .ok());
  ASSERT_TRUE(
      rom.WriteByte(ptr_offset + 2, static_cast<uint8_t>((snes_ptr >> 16) & 0xFF))
          .ok());

  OracleRomSafetyPreflightOptions options;
  options.require_water_fill_reserved_region = true;
  options.validate_water_fill_table = false;
  options.validate_custom_collision_maps = true;

  const auto result = RunOracleRomSafetyPreflight(&rom, options);
  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(HasErrorCode(result, "ORACLE_COLLISION_POINTER_INVALID"));
}

// ---------------------------------------------------------------------------
// Prison-room entity preflight tests
//
// These tests verify the room_ids_requiring_custom_collision check, which
// unblocks D3 Kalyxo Castle prison sequence validation. A room that is listed
// as required must have non-empty authored collision data; if it is empty or
// has an invalid pointer, the preflight reports
// "ORACLE_REQUIRED_ROOM_MISSING_COLLISION".
// ---------------------------------------------------------------------------

TEST(OracleRomSafetyPreflightTest,
     SucceedsWhenRequiredRoomHasCollisionData) {
  // Import stop-tile data into room 0x32 (D3 prison entrance) then verify
  // the required-room check passes.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  // Write a valid WaterFill table header so the main preflight doesn't fail.
  // (zone_count = 0, table is empty but structurally valid.)
  ASSERT_TRUE(rom.WriteByte(kWaterFillTableStart, 0).ok());

  // Use the import handler to write custom collision for room 0x32.
  // Offset 100 = y=1, x=36 in the 64x64 collision grid.
  {
    const std::string json =
        R"({"version":1,"rooms":[{"room_id":"0x32","tiles":[[100,184]]}]})";
    const auto tmp = (std::filesystem::temp_directory_path() /
                      "yaze_prison_preflight_ok.json").string();
    std::ofstream out_file(tmp, std::ios::out | std::ios::binary | std::ios::trunc);
    out_file << json;
    out_file.close();

    yaze::cli::handlers::DungeonImportCustomCollisionJsonCommandHandler handler;
    std::string out;
    ASSERT_TRUE(handler.Run({"--in", tmp, "--format=json"}, &rom, &out).ok());
    std::filesystem::remove(tmp);
  }

  OracleRomSafetyPreflightOptions options;
  options.require_water_fill_reserved_region = false;
  options.validate_water_fill_table = false;
  options.validate_custom_collision_maps = false;
  options.room_ids_requiring_custom_collision = {0x32};

  const auto result = RunOracleRomSafetyPreflight(&rom, options);
  EXPECT_TRUE(result.ok())
      << (result.errors.empty() ? "" : result.errors[0].message);
}

TEST(OracleRomSafetyPreflightTest,
     FailsWhenRequiredRoomHasNoCollisionData) {
  // Room 0x32 with no authored data → preflight must report
  // ORACLE_REQUIRED_ROOM_MISSING_COLLISION.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  OracleRomSafetyPreflightOptions options;
  options.require_water_fill_reserved_region = false;
  options.validate_water_fill_table = false;
  options.validate_custom_collision_maps = false;
  options.room_ids_requiring_custom_collision = {0x32};

  const auto result = RunOracleRomSafetyPreflight(&rom, options);
  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(HasErrorCode(result, "ORACLE_REQUIRED_ROOM_MISSING_COLLISION"));
}

TEST(OracleRomSafetyPreflightTest,
     EmptyRequiredRoomListSkipsCheck) {
  // No required rooms → preflight must not add any MISSING_COLLISION errors.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  OracleRomSafetyPreflightOptions options;
  options.require_water_fill_reserved_region = false;
  options.validate_water_fill_table = false;
  options.validate_custom_collision_maps = false;
  // room_ids_requiring_custom_collision left empty (default).

  const auto result = RunOracleRomSafetyPreflight(&rom, options);
  EXPECT_TRUE(result.ok());
  for (const auto& err : result.errors) {
    EXPECT_NE(err.code, "ORACLE_REQUIRED_ROOM_MISSING_COLLISION");
  }
}

// ---------------------------------------------------------------------------
// SHA-256 utility tests
// ---------------------------------------------------------------------------

class Sha256Test : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto nonce = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    temp_path_ = std::filesystem::temp_directory_path() /
                 ("yaze_sha256_test_" + std::to_string(nonce));
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove(temp_path_, ec);
  }

  // Write raw bytes to the temp file.
  void WriteBytes(const std::vector<uint8_t>& data) {
    std::ofstream out(temp_path_, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    out.close();
  }

  std::filesystem::path temp_path_;
};

TEST_F(Sha256Test, ComputesKnownHash) {
  // "yaze_test_payload_1234" has a well-known SHA-256.
  const std::string payload = "yaze_test_payload_1234";
  const std::string expected_hash =
      "8ba9a29c82f1588b86b3128006ca0e3aed232fbe94a7baa1f89e3f59ba653ebc";

  WriteBytes(std::vector<uint8_t>(payload.begin(), payload.end()));

  auto hash_or = ComputeSha256(temp_path_.string());
  ASSERT_TRUE(hash_or.ok()) << hash_or.status();
  EXPECT_EQ(*hash_or, expected_hash);
}

TEST_F(Sha256Test, VerifyMatchesExpectedHash) {
  const std::string payload = "yaze_test_payload_1234";
  const std::string expected_hash =
      "8ba9a29c82f1588b86b3128006ca0e3aed232fbe94a7baa1f89e3f59ba653ebc";

  WriteBytes(std::vector<uint8_t>(payload.begin(), payload.end()));

  EXPECT_TRUE(VerifySha256(temp_path_.string(), expected_hash).ok());
}

TEST_F(Sha256Test, DetectsSingleByteMismatch) {
  const std::string payload = "yaze_test_payload_1234";
  const std::string original_hash =
      "8ba9a29c82f1588b86b3128006ca0e3aed232fbe94a7baa1f89e3f59ba653ebc";

  // Write the original content and verify it matches.
  WriteBytes(std::vector<uint8_t>(payload.begin(), payload.end()));
  ASSERT_TRUE(VerifySha256(temp_path_.string(), original_hash).ok());

  // Flip one byte and verify the hash no longer matches.
  std::vector<uint8_t> modified(payload.begin(), payload.end());
  modified[0] ^= 0x01;
  WriteBytes(modified);

  auto status = VerifySha256(temp_path_.string(), original_hash);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kDataLoss);
}

TEST_F(Sha256Test, ReturnsErrorForMissingFile) {
  auto hash_or = ComputeSha256("/nonexistent/path/to/file.bin");
  EXPECT_FALSE(hash_or.ok());
  EXPECT_EQ(hash_or.status().code(), absl::StatusCode::kNotFound);
}

}  // namespace
}  // namespace yaze::zelda3
