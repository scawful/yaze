#include "core/patch/patch_manager.h"

#include <gtest/gtest.h>

#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "rom/rom.h"
#include "test_utils.h"

namespace yaze::test {
namespace {

std::string SanitizeForPath(const std::string& value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (unsigned char ch : value) {
    if (std::isalnum(ch) || ch == '-' || ch == '_') {
      sanitized.push_back(static_cast<char>(ch));
    } else {
      sanitized.push_back('_');
    }
  }
  return sanitized;
}

std::string CurrentTestName() {
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  if (!info) {
    return "unknown_test";
  }
  return std::string(info->test_suite_name()) + "_" + info->name();
}

std::filesystem::path MakeUniqueTempDir(const std::string& prefix) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto name = prefix + "_" + SanitizeForPath(CurrentTestName()) + "_" +
                    std::to_string(now);
  return std::filesystem::temp_directory_path() / name;
}

class Z3dkPatchManagerIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = MakeUniqueTempDir("yaze_z3dk_patch_manager");
    std::filesystem::create_directories(test_dir_ / "Misc");
    std::filesystem::create_directories(test_dir_ / "Sprites");

    CreatePatchFile("Misc", "BytePatch.asm", R"(;#PATCH_NAME=Byte Patch
;#ENABLED=true
org $008100
db $42, $43, $44
)");

    CreatePatchFile("Sprites", "SpritePatch.asm", R"(;#PATCH_NAME=Sprite Patch
;#ENABLED=true
org $008120
db $99, $88
)");

    CreatePatchFile("Misc", "DisabledPatch.asm", R"(;#PATCH_NAME=Disabled Patch
;#ENABLED=false
org $008140
db $DE, $AD
)");

    ASSERT_TRUE(rom_.LoadFromData(TestRomManager::CreateMinimalTestRom()).ok());
    baseline_rom_ = rom_.vector();
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(test_dir_, ec);
  }

  void CreatePatchFile(const std::string& folder, const std::string& name,
                       const std::string& content) {
    std::ofstream file(test_dir_ / folder / name);
    ASSERT_TRUE(file.is_open());
    file << content;
    file.close();
  }

  std::filesystem::path test_dir_;
  Rom rom_;
  std::vector<uint8_t> baseline_rom_;
};

TEST_F(Z3dkPatchManagerIntegrationTest,
       ApplyEnabledPatchesWritesCombinedPatchThroughZ3dkBackend) {
  core::PatchManager manager;
  auto load_status = manager.LoadPatches(test_dir_.string());
  ASSERT_TRUE(load_status.ok()) << load_status.message();
  ASSERT_EQ(manager.GetEnabledPatchCount(), 2);

  auto apply_status = manager.ApplyEnabledPatches(&rom_);
  ASSERT_TRUE(apply_status.ok()) << apply_status.message();

  ASSERT_GT(rom_.vector().size(), 0x141u);
  EXPECT_EQ(rom_.vector()[0x0100], 0x42);
  EXPECT_EQ(rom_.vector()[0x0101], 0x43);
  EXPECT_EQ(rom_.vector()[0x0102], 0x44);
  EXPECT_EQ(rom_.vector()[0x0120], 0x99);
  EXPECT_EQ(rom_.vector()[0x0121], 0x88);

  EXPECT_EQ(rom_.vector()[0x0140], baseline_rom_[0x0140]);
  EXPECT_EQ(rom_.vector()[0x0141], baseline_rom_[0x0141]);
}

}  // namespace
}  // namespace yaze::test
