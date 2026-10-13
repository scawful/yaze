#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "cli/handlers/rom/mock_rom.h"
#include "cli/handlers/rom/rom_commands.h"
#include "cli/handlers/tools/rom_doctor_commands.h"
#include "rom/rom.h"

namespace yaze::cli {
namespace {

constexpr uint32_t kHeaderRomSizeByte = 0x7FD7;
constexpr uint32_t kHeaderSramSizeByte = 0x7FD8;

// Returns the integer after "key": in rom-doctor JSON output, or -1.
long long JsonIntField(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\": ";
  const auto pos = json.find(needle);
  if (pos == std::string::npos) {
    return -1;
  }
  return std::stoll(json.substr(pos + needle.size()));
}

std::string RunDoctorJson(Rom* rom) {
  RomDoctorCommandHandler handler;
  std::string out;
  EXPECT_TRUE(handler.Run({"--format=json"}, rom, &out).ok());
  return out;
}

TEST(RomHeaderSizeTest, MockRomHeaderSizeMatchesMockData) {
  Rom rom;
  ASSERT_TRUE(InitializeMockRom(rom).ok());
  ASSERT_GT(rom.size(), kHeaderRomSizeByte);

  EXPECT_EQ(size_t{0x400} << rom.data()[kHeaderRomSizeByte], rom.size());
}

TEST(RomHeaderSizeTest, RomDoctorReportsHeaderSizes) {
  std::vector<uint8_t> data(0x200000, 0x00);
  data[kHeaderRomSizeByte] = 0x0A;  // 1 MB, as expanded ALTTP hacks keep it
  data[kHeaderSramSizeByte] = 0x03;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());

  const std::string out = RunDoctorJson(&rom);
  EXPECT_EQ(JsonIntField(out, "rom_size_header"), 0x100000) << out;
  EXPECT_EQ(JsonIntField(out, "sram_size"), 0x2000) << out;
  EXPECT_EQ(JsonIntField(out, "size_bytes"), 0x200000) << out;
}

TEST(RomHeaderSizeTest, RomDoctorReportsZeroForOutOfRangeSizeCodes) {
  std::vector<uint8_t> data(0x200000, 0x00);
  data[kHeaderRomSizeByte] = 0xFF;
  data[kHeaderSramSizeByte] = 0xFF;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());

  const std::string out = RunDoctorJson(&rom);
  EXPECT_EQ(JsonIntField(out, "rom_size_header"), 0) << out;
  EXPECT_EQ(JsonIntField(out, "sram_size"), 0) << out;
}

TEST(RomHeaderSizeTest, RomInfoReportsCopierHeader) {
  // 0x200-byte copier header in front of a 1 MB image.
  std::vector<uint8_t> file(0x100200, 0x00);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(file).ok());
  ASSERT_TRUE(rom.has_copier_header());

  handlers::RomInfoCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--format=json"}, &rom, &out).ok()) << out;
  EXPECT_NE(out.find("\"copier_header\": true"), std::string::npos) << out;
  EXPECT_EQ(JsonIntField(out, "copier_header_size"), 0x200) << out;
  EXPECT_EQ(JsonIntField(out, "size_bytes"), 0x100000) << out;
  EXPECT_NE(out.find("\"file_size\": \"0x100200\""), std::string::npos) << out;
  EXPECT_NE(out.find("\"copier_header_detection\": \"size_alignment\""),
            std::string::npos)
      << out;

  Rom plain;
  ASSERT_TRUE(plain.LoadFromData(std::vector<uint8_t>(0x100000, 0)).ok());
  out.clear();
  ASSERT_TRUE(handler.Run({"--format=json"}, &plain, &out).ok()) << out;
  EXPECT_NE(out.find("\"copier_header\": false"), std::string::npos) << out;
  EXPECT_EQ(JsonIntField(out, "copier_header_size"), 0) << out;
}

}  // namespace
}  // namespace yaze::cli
