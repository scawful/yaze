#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "cli/handlers/tools/rom_doctor_commands.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::cli {
namespace {

TEST(RomDoctorWaterFillTest, ReportsFindingWhenWaterFillTableIsInvalid) {
  Rom rom;
  std::vector<uint8_t> dummy(0x200000, 0x00);
  ASSERT_TRUE(rom.LoadFromData(dummy).ok());

  // Make room 0 custom collision pointer point into the reserved region so
  // LoadWaterFillTable() fails its overlap safety check.
  const uint32_t snes =
      PcToSnes(static_cast<uint32_t>(yaze::zelda3::kWaterFillTableStart));
  const int ptr_offset = yaze::zelda3::kCustomCollisionRoomPointers + 0;
  ASSERT_TRUE(rom.WriteByte(ptr_offset + 0, snes & 0xFF).ok());
  ASSERT_TRUE(rom.WriteByte(ptr_offset + 1, (snes >> 8) & 0xFF).ok());
  ASSERT_TRUE(rom.WriteByte(ptr_offset + 2, (snes >> 16) & 0xFF).ok());

  RomDoctorCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--format=json"}, &rom, &out).ok());

  EXPECT_NE(out.find("water_fill_table_invalid"), std::string::npos) << out;
}

}  // namespace
}  // namespace yaze::cli

