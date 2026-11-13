#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "cli/handlers/graphics/hex_commands.h"
#include "cli/handlers/rom/rom_commands.h"
#include "cli/handlers/tools/rom_compare_commands.h"
#include "rom/rom.h"
#include "unique_temp_path.h"

namespace yaze::cli {
namespace {

constexpr size_t kRomSize = 0x100000;
// Outside every region rom-compare checks by default.
constexpr uint32_t kUncheckedOffset = 0x04CA37;

std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void WriteBytes(const std::filesystem::path& path,
                const std::vector<uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

class RomWriteCommandTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = test::UniqueTempPath("rom_write");
    std::filesystem::create_directories(dir_);
    image_.assign(kRomSize, 0x00);
    image_[0x0100] = 0x9C;
    image_[kUncheckedOffset] = 0xF0;
    work_ = dir_ / "work.sfc";
    WriteBytes(work_, image_);
    ASSERT_TRUE(rom_.LoadFromFile(work_.string()).ok());
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }

  nlohmann::json RunJson(resources::CommandHandler& handler,
                         std::vector<std::string> args,
                         absl::Status* status_out = nullptr) {
    std::string output;
    args.push_back("--format=json");
    const auto status = handler.Run(args, &rom_, &output);
    if (status_out != nullptr) {
      *status_out = status;
    } else {
      EXPECT_TRUE(status.ok()) << status << "\n" << output;
    }
    return nlohmann::json::parse(output, nullptr, /*allow_exceptions=*/false);
  }

  std::filesystem::path dir_;
  std::filesystem::path work_;
  std::vector<uint8_t> image_;
  Rom rom_;
};

TEST_F(RomWriteCommandTest, RomWriteDefaultsToDryRun) {
  handlers::RomWriteCommandHandler handler;
  auto json = RunJson(handler, {"--address=0x100", "--data=AA"});
  EXPECT_EQ(json["mode"], "dry-run");
  EXPECT_EQ(json["write_status"], "dry-run");
  EXPECT_EQ(json["before"], "9C");
  EXPECT_EQ(json["after"], "AA");
  EXPECT_EQ(rom_.vector()[0x100], 0x9C);
  EXPECT_EQ(ReadBytes(work_), image_);
}

TEST_F(RomWriteCommandTest, RomWriteSavesAndVerifiesTheFile) {
  handlers::RomWriteCommandHandler handler;
  auto json = RunJson(handler, {"--address=0x100", "--data=AA BB", "--write"});
  ASSERT_EQ(json["save_status"], "saved") << json.dump();
  EXPECT_EQ(json["verified_reload"], true);

  auto expected = image_;
  expected[0x100] = 0xAA;
  expected[0x101] = 0xBB;
  EXPECT_EQ(ReadBytes(work_), expected);
}

TEST_F(RomWriteCommandTest, HexWriteSharesTheGuardedWriter) {
  handlers::HexWriteCommandHandler handler;
  auto dry = RunJson(handler, {"--address=0x100", "--data=10"});
  EXPECT_EQ(dry["Hex Write"]["mode"], "dry-run");
  EXPECT_EQ(ReadBytes(work_), image_);

  auto json = RunJson(handler, {"--address=0x100", "--data=10", "--write"});
  ASSERT_EQ(json["Hex Write"]["save_status"], "saved") << json.dump();
  EXPECT_EQ(ReadBytes(work_)[0x100], 0x10);
}

TEST_F(RomWriteCommandTest, RomWriteRefusesOracleCheckoutRom) {
  const auto root = dir_ / "checkout";
  std::filesystem::create_directories(root / "Roms");
  std::ofstream(root / "Oracle_main.asm") << "; main\n";
  const auto project_rom = root / "Roms" / "work.sfc";
  WriteBytes(project_rom, image_);
  ASSERT_TRUE(rom_.LoadFromFile(project_rom.string()).ok());

  handlers::RomWriteCommandHandler handler;
  absl::Status status;
  RunJson(handler, {"--address=0x100", "--data=AA", "--write"}, &status);
  EXPECT_TRUE(absl::IsPermissionDenied(status)) << status;
  EXPECT_EQ(ReadBytes(project_rom), image_);
}

TEST_F(RomWriteCommandTest, RomCompareReportsBytesOutsideCheckedRegions) {
  const auto baseline = dir_ / "baseline.sfc";
  WriteBytes(baseline, image_);
  RomCompareCommandHandler handler;

  auto same = RunJson(handler, {"--baseline=" + baseline.string()});
  EXPECT_EQ(same["assessment"], "identical");
  EXPECT_EQ(same["unchecked_diff_bytes"], 0);

  ASSERT_TRUE(rom_.WriteByte(kUncheckedOffset, 0x0C).ok());
  auto edited = RunJson(handler, {"--baseline=" + baseline.string()});
  EXPECT_EQ(edited["assessment"], "differences_outside_checked_regions");
  EXPECT_EQ(edited["total_diff_bytes"], 0);
  EXPECT_EQ(edited["unchecked_diff_bytes"], 1);

  auto all = RunJson(handler, {"--baseline=" + baseline.string(), "--all"});
  EXPECT_EQ(all["total_diff_bytes"], 1);
  EXPECT_EQ(all["unchecked_diff_bytes"], 0);
}

}  // namespace
}  // namespace yaze::cli
