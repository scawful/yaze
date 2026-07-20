#include "oracle_rom_fixture.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

namespace yaze::test {
namespace {

class OracleRomFixtureDiscoveryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto nonce =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    temp_dir_ = std::filesystem::temp_directory_path() /
                ("yaze_oracle_fixture_" + std::to_string(nonce));
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(temp_dir_, error);
  }

  static void Touch(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(file.is_open()) << path;
  }

  std::filesystem::path temp_dir_;
};

TEST_F(OracleRomFixtureDiscoveryTest,
       NestedPatchedRomBeatsBaseInCurrentDirectory) {
  const auto yaze_dir = temp_dir_ / "nested_case" / "yaze";
  const auto base_rom = yaze_dir / "oos168.sfc";
  const auto patched_rom = yaze_dir / "roms" / "oos168x.sfc";
  Touch(base_rom);
  Touch(patched_rom);

  EXPECT_EQ(AutoDiscoverOracleRom(yaze_dir),
            patched_rom.lexically_normal().string());
}

TEST_F(OracleRomFixtureDiscoveryTest,
       SiblingPatchedRomBeatsBaseInCurrentDirectory) {
  const auto case_dir = temp_dir_ / "sibling_case";
  const auto yaze_dir = case_dir / "yaze";
  const auto base_rom = yaze_dir / "oos168.sfc";
  const auto patched_rom =
      case_dir / "oracle-of-secrets" / "Roms" / "oos168x.sfc";
  Touch(base_rom);
  Touch(patched_rom);

  EXPECT_EQ(AutoDiscoverOracleRom(yaze_dir),
            patched_rom.lexically_normal().string());
}

}  // namespace
}  // namespace yaze::test
