#include "zelda3/dungeon/oracle_rom_safety_preflight.h"

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

}  // namespace
}  // namespace yaze::zelda3
