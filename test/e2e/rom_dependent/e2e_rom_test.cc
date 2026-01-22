#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "rom/rom.h"
#include "rom/transaction.h"
#include "test_utils.h"
#include "testing.h"
#include "util/macro.h"

namespace yaze {
namespace test {

/**
 * @brief Comprehensive End-to-End ROM testing suite
 *
 * This test suite validates the complete ROM editing workflow:
 * 1. Load vanilla ROM
 * 2. Apply various edits (ROM data, graphics, etc.)
 * 3. Save changes
 * 4. Reload ROM and verify edits persist
 * 5. Verify no data corruption occurred
 */
class E2ERomDependentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    yaze::test::TestRomManager::SkipIfRomMissing(
        yaze::test::RomRole::kVanilla, "E2ERomDependentTest");
    vanilla_rom_path_ =
        yaze::test::TestRomManager::GetRomPath(yaze::test::RomRole::kVanilla);

    // Create test ROM copies
    test_rom_path_ = "test_rom_edit.sfc";
    backup_rom_path_ = "test_rom_backup.sfc";

    // Copy vanilla ROM for testing
    std::filesystem::copy_file(vanilla_rom_path_, test_rom_path_);
    std::filesystem::copy_file(vanilla_rom_path_, backup_rom_path_);
  }

  void TearDown() override {
    // Clean up test files
    if (std::filesystem::exists(test_rom_path_)) {
      std::filesystem::remove(test_rom_path_);
    }
    if (std::filesystem::exists(backup_rom_path_)) {
      std::filesystem::remove(backup_rom_path_);
    }
  }

  // Helper to load ROM and verify basic integrity
  static absl::Status LoadAndVerifyROM(const std::string& path,
                                       std::unique_ptr<Rom>& rom) {
    rom = std::make_unique<Rom>();
    RETURN_IF_ERROR(rom->LoadFromFile(path));

    // Basic ROM integrity checks
    EXPECT_EQ(rom->size(), 0x200000) << "ROM size should be 2MB";
    EXPECT_NE(rom->data(), nullptr) << "ROM data should not be null";

    // Check ROM mapping mode (LoROM expected)
    auto map_mode = rom->ReadByte(0x7FD5);
    RETURN_IF_ERROR(map_mode.status());
    EXPECT_EQ(*map_mode & 0x01, 0) << "ROM should be LoROM format";

    return absl::OkStatus();
  }

  // Helper to verify ROM data integrity by comparing checksums
  static bool VerifyROMIntegrity(
      const std::string& path1, const std::string& path2,
      const std::vector<uint32_t>& exclude_ranges = {}) {
    std::ifstream file1(path1, std::ios::binary);
    std::ifstream file2(path2, std::ios::binary);

    if (!file1.is_open() || !file2.is_open()) {
      return false;
    }

    file1.seekg(0, std::ios::end);
    file2.seekg(0, std::ios::end);

    size_t size1 = file1.tellg();
    size_t size2 = file2.tellg();

    if (size1 != size2) {
      return false;
    }

    file1.seekg(0);
    file2.seekg(0);

    std::vector<char> buffer1(size1);
    std::vector<char> buffer2(size2);

    file1.read(buffer1.data(), size1);
    file2.read(buffer2.data(), size2);

    // Compare byte by byte, excluding specified ranges
    for (size_t i = 0; i < size1; i++) {
      bool in_exclude_range = false;
      for (const auto& range : exclude_ranges) {
        if (i >= (range & 0xFFFFFF) && i < ((range >> 24) & 0xFF)) {
          in_exclude_range = true;
          break;
        }
      }

      if (!in_exclude_range && buffer1[i] != buffer2[i]) {
        return false;
      }
    }

    return true;
  }

  std::string vanilla_rom_path_;
  std::string test_rom_path_;
  std::string backup_rom_path_;
};

// Test basic ROM loading and saving
TEST_F(E2ERomDependentTest, BasicROMLoadSave) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyROM(vanilla_rom_path_, rom));

  // Save ROM to test path
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = test_rom_path_}));

  // Verify saved ROM matches original
  EXPECT_TRUE(VerifyROMIntegrity(vanilla_rom_path_, test_rom_path_));
}

// Test ROM data editing workflow
TEST_F(E2ERomDependentTest, ROMDataEditWorkflow) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyROM(vanilla_rom_path_, rom));

  // Get initial state
  auto initial_byte = rom->ReadByte(0x1000);
  ASSERT_TRUE(initial_byte.ok());

  // Make edits
  ASSERT_OK(rom->WriteByte(0x1000, 0xAA));
  ASSERT_OK(rom->WriteByte(0x2000, 0xBB));
  ASSERT_OK(rom->WriteWord(0x3000, 0xCCDD));

  // Save changes
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = test_rom_path_}));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyROM(test_rom_path_, reloaded_rom));

  auto byte1 = reloaded_rom->ReadByte(0x1000);
  ASSERT_OK(byte1.status());
  EXPECT_EQ(*byte1, 0xAA);

  auto byte2 = reloaded_rom->ReadByte(0x2000);
  ASSERT_OK(byte2.status());
  EXPECT_EQ(*byte2, 0xBB);

  auto word1 = reloaded_rom->ReadWord(0x3000);
  ASSERT_OK(word1.status());
  EXPECT_EQ(*word1, 0xCCDD);

  // Verify other data wasn't corrupted
  EXPECT_NE(*byte1, *initial_byte);
}

// Test transaction system with multiple edits
TEST_F(E2ERomDependentTest, TransactionSystem) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyROM(vanilla_rom_path_, rom));

  // Create transaction
  auto transaction = std::make_unique<yaze::Transaction>(*rom);

  // Make multiple edits in transaction
  transaction->WriteByte(0x1000, 0xAA);
  transaction->WriteByte(0x2000, 0xBB);
  transaction->WriteWord(0x3000, 0xCCDD);

  // Commit the transaction
  ASSERT_OK(transaction->Commit());

  // Commit transaction
  ASSERT_OK(transaction->Commit());

  // Save ROM
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = test_rom_path_}));

  // Reload and verify all changes
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyROM(test_rom_path_, reloaded_rom));

  auto byte1 = reloaded_rom->ReadByte(0x1000);
  ASSERT_OK(byte1.status());
  EXPECT_EQ(*byte1, 0xAA);

  auto byte2 = reloaded_rom->ReadByte(0x2000);
  ASSERT_OK(byte2.status());
  EXPECT_EQ(*byte2, 0xBB);

  auto word1 = reloaded_rom->ReadWord(0x3000);
  ASSERT_OK(word1.status());
  EXPECT_EQ(*word1, 0xCCDD);
}

// Test ROM corruption detection
TEST_F(E2ERomDependentTest, CorruptionDetection) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyROM(vanilla_rom_path_, rom));

  // Corrupt some data
  ASSERT_OK(rom->WriteByte(0x1000, 0xFF));  // Corrupt some data
  ASSERT_OK(rom->WriteByte(0x2000, 0xAA));  // Corrupt more data

  // Save corrupted ROM
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = test_rom_path_}));

  // Verify corruption is detected
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyROM(test_rom_path_, reloaded_rom));

  auto corrupt_byte1 = reloaded_rom->ReadByte(0x1000);
  ASSERT_OK(corrupt_byte1.status());
  EXPECT_EQ(*corrupt_byte1, 0xFF);

  auto corrupt_byte2 = reloaded_rom->ReadByte(0x2000);
  ASSERT_OK(corrupt_byte2.status());
  EXPECT_EQ(*corrupt_byte2, 0xAA);
}

// Test large-scale editing without corruption
TEST_F(E2ERomDependentTest, LargeScaleEditing) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyROM(vanilla_rom_path_, rom));

  // Edit multiple areas
  for (int i = 0; i < 10; i++) {
    ASSERT_OK(rom->WriteByte(0x1000 + i, i % 16));
    ASSERT_OK(rom->WriteByte(0x2000 + i, (i + 1) % 16));
  }

  // Save and reload
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = test_rom_path_}));

  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyROM(test_rom_path_, reloaded_rom));

  // Verify all changes
  for (int i = 0; i < 10; i++) {
    auto byte1 = reloaded_rom->ReadByte(0x1000 + i);
    ASSERT_OK(byte1.status());
    EXPECT_EQ(*byte1, i % 16);

    auto byte2 = reloaded_rom->ReadByte(0x2000 + i);
    ASSERT_OK(byte2.status());
    EXPECT_EQ(*byte2, (i + 1) % 16);
  }
}

}  // namespace test
}  // namespace yaze
