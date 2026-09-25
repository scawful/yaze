#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "cli/handlers/tools/graphics_doctor_commands.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::cli {
namespace {

using test::BuildGfxSheetTestRom;
using test::GfxSheetTestRom;

nlohmann::json RunDoctor(Rom& rom) {
  GraphicsDoctorCommandHandler handler;
  std::string out;
  const absl::Status status = handler.Run({"--format=json"}, &rom, &out);
  EXPECT_TRUE(status.ok()) << status;
  const auto begin = out.find('{');
  EXPECT_NE(begin, std::string::npos) << out;
  return nlohmann::json::parse(out.substr(begin), nullptr,
                               /*allow_exceptions=*/false);
}

bool HasFinding(const nlohmann::json& report, const std::string& id) {
  if (!report.contains("findings")) {
    return false;
  }
  // Each finding is emitted as a JSON-encoded string.
  for (const auto& finding : report["findings"]) {
    const std::string text =
        finding.is_string() ? finding.get<std::string>() : finding.dump();
    if (text.find("\"" + id + "\"") != std::string::npos) {
      return true;
    }
  }
  return false;
}

Rom LoadRom(const std::vector<uint8_t>& bytes) {
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(bytes).ok());
  return rom;
}

std::vector<uint8_t> Encode(const std::vector<uint8_t>& data) {
  int size = 0;
  auto encoded = gfx::HyruleMagicCompress(
      data.data(), static_cast<int>(data.size()), &size, /*flag=*/0);
  encoded.resize(size);
  return encoded;
}

TEST(GraphicsDoctorTest, CleanRomHasNoErrorsAndReportsAliasesAsInfo) {
  const auto fixture = BuildGfxSheetTestRom(/*alias_2bpp=*/true);
  Rom rom = LoadRom(fixture.bytes);
  const auto report = RunDoctor(rom);
  ASSERT_FALSE(report.is_discarded());

  EXPECT_EQ(report["successful_decompressions"], 223) << report.dump(2);
  EXPECT_EQ(report["failed_decompressions"], 0);
  EXPECT_EQ(report["error_count"], 0) << report.dump(2);
  EXPECT_EQ(report["critical_count"], 0);
  EXPECT_EQ(report["warning_count"], 0) << report.dump(2);
  EXPECT_EQ(report["aliased_sheets"], 2);
  EXPECT_EQ(report["overlapping_sheets"], 0);
  EXPECT_TRUE(HasFinding(report, "sheet_alias"));
}

TEST(GraphicsDoctorTest, FlagsA3bppSheetThatOverrunsItsWramBuffer) {
  auto fixture = BuildGfxSheetTestRom();
  // Sheet 0x10 re-encoded as 0x800 bytes, as older yaze saves did.
  auto oversized = fixture.sheets.at(0x10);
  oversized.resize(0x800, 0x00);
  const auto stream = Encode(oversized);
  std::copy(stream.begin(), stream.end(),
            fixture.bytes.begin() + GfxSheetTestRom::kFreeStart);
  test::SetTestSheetPointer(fixture.bytes, 0x10, GfxSheetTestRom::kFreeStart,
                            /*fast_rom=*/false);

  Rom rom = LoadRom(fixture.bytes);
  const auto report = RunDoctor(rom);
  EXPECT_EQ(report["oversized_sheets"], 1) << report.dump(2);
  EXPECT_GE(report["error_count"], 1);
  EXPECT_TRUE(HasFinding(report, "sheet_decoded_size"));
}

TEST(GraphicsDoctorTest, FlagsPartiallyOverlappingSheets) {
  auto fixture = BuildGfxSheetTestRom();
  // Sheet 0x12's stream begins two bytes into sheet 0x11's stream.
  const auto tail = Encode(fixture.sheets.at(0x12));
  std::vector<uint8_t> head = {0x00, 0x5A};  // one literal byte
  head.insert(head.end(), tail.begin(), tail.end());
  std::copy(head.begin(), head.end(),
            fixture.bytes.begin() + GfxSheetTestRom::kFreeStart);
  test::SetTestSheetPointer(fixture.bytes, 0x11, GfxSheetTestRom::kFreeStart,
                            false);
  test::SetTestSheetPointer(fixture.bytes, 0x12,
                            GfxSheetTestRom::kFreeStart + 2, false);

  Rom rom = LoadRom(fixture.bytes);
  const auto report = RunDoctor(rom);
  EXPECT_EQ(report["overlapping_sheets"], 1) << report.dump(2);
  EXPECT_TRUE(HasFinding(report, "sheet_overlap"));
}

TEST(GraphicsDoctorTest, WarnsWhenASpritesetNamesASheetPastTheTables) {
  auto fixture = BuildGfxSheetTestRom();
  // 0x6E + 0x73 = sheet 225; the tables hold 223 sheets.
  fixture.bytes[zelda3::kSpriteBlocksetPointer + 61 * 4] = 0x6E;

  Rom rom = LoadRom(fixture.bytes);
  const auto report = RunDoctor(rom);
  EXPECT_EQ(report["invalid_group_refs"], 1) << report.dump(2);
  EXPECT_EQ(report["error_count"], 0);
  EXPECT_TRUE(HasFinding(report, "invalid_group_sheet_ref"));
}

TEST(GraphicsDoctorTest, SingleSheetModeReportsItsExtent) {
  const auto fixture = BuildGfxSheetTestRom();
  Rom rom = LoadRom(fixture.bytes);
  GraphicsDoctorCommandHandler handler;
  std::string out;
  ASSERT_TRUE(handler.Run({"--sheet=116", "--format=json"}, &rom, &out).ok());
  EXPECT_NE(out.find("\"stored_size\": 1536"), std::string::npos) << out;
  EXPECT_NE(out.find("\"successful_decompressions\": 1"), std::string::npos)
      << out;
}

// The Oracle RC ROM must have no graphics errors. Warnings (for example an
// Oracle spriteset naming a sheet past the tables) are allowed and printed.
TEST(GraphicsDoctorRomTest, OracleRomHasNoGraphicsErrors) {
  const char* env = std::getenv("YAZE_TEST_ROM_OOS");
  if (env == nullptr || !std::filesystem::exists(env)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to an Oracle ROM";
  }
  std::ifstream file(env, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
  Rom rom = LoadRom(bytes);
  const auto report = RunDoctor(rom);
  ASSERT_FALSE(report.is_discarded());
  EXPECT_EQ(report["critical_count"], 0) << report.dump(2);
  EXPECT_EQ(report["error_count"], 0) << report.dump(2);
  EXPECT_EQ(report["successful_decompressions"], 223);
  EXPECT_EQ(report["oversized_sheets"], 0);
  EXPECT_EQ(report["overlapping_sheets"], 0);
  std::cout << "graphics-doctor warnings: " << report["warning_count"]
            << ", invalid group refs: " << report["invalid_group_refs"] << "\n";
}

}  // namespace
}  // namespace yaze::cli
