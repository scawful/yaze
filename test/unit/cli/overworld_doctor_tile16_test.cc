#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "cli/handlers/tools/diagnostic_types.h"
#include "cli/handlers/tools/overworld_doctor_commands.h"
#include "cli/handlers/tools/rom_doctor_commands.h"
#include "rom/rom.h"

namespace yaze::cli {
namespace {

// Valid Oracle of Secrets tile16 entries 0x0F1, 0x0F2 and 0xEA8. An earlier
// doctor heuristic flagged them as corrupt and --fix zeroed them.
constexpr uint32_t kTile16F1 = 0x1E8788;
constexpr uint32_t kTile16EA8 = 0x1EF540;
const std::vector<uint8_t> kTile16F1F2 = {0x16, 0x30, 0x00, 0x30, 0x26, 0x30,
                                          0x10, 0x30, 0xC9, 0x28, 0xC9, 0x68,
                                          0xDF, 0x28, 0xDF, 0x68};
const std::vector<uint8_t> kTile16EA8Words = {0xAA, 0x08, 0xAA, 0x08,
                                              0xAA, 0x08, 0xAA, 0x08};

Rom MakeExpandedTile16Rom() {
  std::vector<uint8_t> data(0x200000, 0x00);
  data[kZSCustomVersionPos] = 3;
  data[kMap16ExpandedFlagPos] = 0x00;
  std::copy(kTile16F1F2.begin(), kTile16F1F2.end(), data.begin() + kTile16F1);
  std::copy(kTile16EA8Words.begin(), kTile16EA8Words.end(),
            data.begin() + kTile16EA8);
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(data).ok());
  return rom;
}

std::vector<uint8_t> Bytes(const Rom& rom, uint32_t addr, size_t count) {
  return {rom.data() + addr, rom.data() + addr + count};
}

TEST(OverworldDoctorTile16Test, FixLeavesValidTile16EntriesAlone) {
  Rom rom = MakeExpandedTile16Rom();

  OverworldDoctorCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--fix", "--format=json"}, &rom, &out).ok());

  EXPECT_EQ(out.find("tile16_corruption"), std::string::npos) << out;
  EXPECT_EQ(Bytes(rom, kTile16F1, kTile16F1F2.size()), kTile16F1F2);
  EXPECT_EQ(Bytes(rom, kTile16EA8, kTile16EA8Words.size()), kTile16EA8Words);
}

TEST(OverworldDoctorTile16Test, RomDoctorDoesNotFlagFixedAddresses) {
  Rom rom = MakeExpandedTile16Rom();

  RomDoctorCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--format=json"}, &rom, &out).ok());

  EXPECT_EQ(out.find("known_corruption_pattern"), std::string::npos) << out;
}

}  // namespace
}  // namespace yaze::cli
