#include "core/oracle_progression_loader.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "testing.h"

namespace yaze::core {

namespace {

std::filesystem::path MakeUniqueTempFile(const std::string& prefix,
                                        const std::string& ext) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string name =
      prefix + "_" + (info ? (std::string(info->test_suite_name()) + "_" +
                              info->name())
                          : "unknown_test") +
      "_" + std::to_string(now) + ext;
  return std::filesystem::temp_directory_path() / name;
}

void WriteBinaryFile(const std::filesystem::path& path,
                     const std::vector<uint8_t>& data) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  ASSERT_TRUE(file.is_open()) << "Failed to open file for writing: "
                              << path.string();
  if (!data.empty()) {
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
  }
  file.close();
}

}  // namespace

TEST(OracleProgressionLoaderTest, LoadsFromSrmFile) {
  std::vector<uint8_t> sram(0x400, 0);
  sram[OracleProgressionState::kCrystalOffset] =
      OracleProgressionState::kCrystalD2 | OracleProgressionState::kCrystalD7;
  sram[OracleProgressionState::kGameStateOffset] = 3;
  sram[OracleProgressionState::kOosProgOffset] = 0x12;
  sram[OracleProgressionState::kOosProg2Offset] = 0x34;
  sram[OracleProgressionState::kSideQuestOffset] = 0x56;
  sram[OracleProgressionState::kPendantOffset] = 0x78;

  const auto path = MakeUniqueTempFile("yaze_oracle", ".srm");
  WriteBinaryFile(path, sram);

  OracleProgressionState state;
  ASSERT_OK_AND_ASSIGN(state, LoadOracleProgressionFromSrmFile(path.string()));
  EXPECT_EQ(state.crystal_bitfield,
            OracleProgressionState::kCrystalD2 |
                OracleProgressionState::kCrystalD7);
  EXPECT_EQ(state.game_state, 3);
  EXPECT_EQ(state.oosprog, 0x12);
  EXPECT_EQ(state.oosprog2, 0x34);
  EXPECT_EQ(state.side_quest, 0x56);
  EXPECT_EQ(state.pendants, 0x78);

  std::error_code ec;
  std::filesystem::remove(path, ec);
}

TEST(OracleProgressionLoaderTest, ReturnsNotFoundForMissingFile) {
  const auto missing = (std::filesystem::temp_directory_path() /
                        "yaze_missing_oracle_progression.srm")
                           .string();
  auto state_or = LoadOracleProgressionFromSrmFile(missing);
  EXPECT_FALSE(state_or.ok());
  EXPECT_EQ(state_or.status().code(), absl::StatusCode::kNotFound);
}

}  // namespace yaze::core
