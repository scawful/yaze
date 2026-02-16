#include "zelda3/dungeon/oracle_rom_safety_preflight.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

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
