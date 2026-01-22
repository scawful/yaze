#ifndef YAZE_TEST_E2E_ROM_DEPENDENT_EDITOR_SAVE_TEST_BASE_H
#define YAZE_TEST_E2E_ROM_DEPENDENT_EDITOR_SAVE_TEST_BASE_H

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "test_utils.h"
#include "testing.h"
#include "util/macro.h"
#include "zelda.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze {
namespace test {

/**
 * @brief ROM version information for testing
 */
struct RomVersionInfo {
  std::string path;
  zelda3_version version;
  bool is_expanded_tile16;
  bool is_expanded_tile32;
  uint8_t zscustom_version;  // 0xFF = vanilla, 0x02 = v2, 0x03+ = v3+
};

/**
 * @brief Base test fixture for E2E editor save tests
 *
 * Provides common functionality for:
 * - ROM loading/saving with automatic cleanup
 * - ROM version detection
 * - Golden data comparison
 * - Backup/restore ROM state
 */
class EditorSaveTestBase : public ::testing::Test {
 protected:
  void SetUp() override {
    TestRomManager::SkipIfRomMissing(RomRole::kVanilla, "EditorSaveTestBase");
    vanilla_rom_path_ = TestRomManager::GetRomPath(RomRole::kVanilla);

    // Create test file paths with unique names per test
    test_id_ = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    test_rom_path_ = "test_" + test_id_ + ".sfc";
    backup_rom_path_ = "backup_" + test_id_ + ".sfc";

    // Copy vanilla ROM for testing
    std::error_code ec;
    std::filesystem::copy_file(
        vanilla_rom_path_, test_rom_path_,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
      GTEST_SKIP() << "Failed to copy test ROM: " << ec.message();
    }

    // Create backup
    std::filesystem::copy_file(
        vanilla_rom_path_, backup_rom_path_,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
      GTEST_SKIP() << "Failed to create backup ROM: " << ec.message();
    }
  }

  void TearDown() override {
    // Clean up test files
    CleanupTestFiles();
  }

  void CleanupTestFiles() {
    std::vector<std::string> files_to_remove = {
        test_rom_path_,
        backup_rom_path_,
    };

    for (const auto& file : files_to_remove) {
      if (std::filesystem::exists(file)) {
        std::filesystem::remove(file);
      }
    }

    // Also clean up any .bak files created by ROM saving
    if (std::filesystem::exists(test_rom_path_ + ".bak")) {
      std::filesystem::remove(test_rom_path_ + ".bak");
    }
  }

  // ===========================================================================
  // ROM Loading Helpers
  // ===========================================================================

  /**
   * @brief Load a ROM and verify basic integrity
   */
  absl::Status LoadAndVerifyRom(const std::string& path,
                                std::unique_ptr<Rom>& rom) {
    rom = std::make_unique<Rom>();
    RETURN_IF_ERROR(rom->LoadFromFile(path));

    // Basic integrity checks
    if (rom->size() < 0x100000) {  // At least 1MB
      return absl::FailedPreconditionError("ROM too small");
    }
    if (rom->data() == nullptr) {
      return absl::FailedPreconditionError("ROM data is null");
    }

    return absl::OkStatus();
  }

  /**
   * @brief Save ROM to disk
   */
  absl::Status SaveRomToFile(Rom* rom, const std::string& path) {
    if (!rom) {
      return absl::InvalidArgumentError("ROM is null");
    }

    Rom::SaveSettings settings;
    settings.filename = path;
    settings.backup = false;  // We handle backups ourselves
    settings.save_new = false;  // Overwrite the test copy for persistence checks

    return rom->SaveToFile(settings);
  }

  // ===========================================================================
  // ROM Version Detection
  // ===========================================================================

  /**
   * @brief Detect ROM version information
   */
  RomVersionInfo DetectRomVersion(Rom& rom) {
    RomVersionInfo info;
    info.path = rom.filename();

    // Detect ZSCustomOverworld version
    auto version_byte = rom.ReadByte(0x140145);
    info.zscustom_version = version_byte.ok() ? *version_byte : 0xFF;

    // Detect expanded tile16 based on applied ASM version (v1+ uses expanded space)
    const auto overworld_version =
        zelda3::OverworldVersionHelper::GetVersion(rom);
    info.is_expanded_tile16 =
        zelda3::OverworldVersionHelper::SupportsExpandedSpace(overworld_version);

    // Detect expanded tile32
    auto tile32_check = rom.ReadByte(0x01772E);
    info.is_expanded_tile32 = tile32_check.ok() && *tile32_check != 0x04;

    // Determine zelda3 version based on header
    info.version = zelda3_detect_version(rom.data(), rom.size());

    return info;
  }

  /**
   * @brief Check if ROM has ZSCustomOverworld ASM applied
   */
  bool IsExpandedRom(Rom& rom) {
    auto version_byte = rom.ReadByte(0x140145);
    if (!version_byte.ok()) return false;
    return *version_byte != 0xFF && *version_byte != 0x00;
  }

  // ===========================================================================
  // Data Comparison Helpers
  // ===========================================================================

  /**
   * @brief Compare two ROM regions for equality
   */
  bool CompareRomRegions(Rom& rom1, Rom& rom2, uint32_t offset, uint32_t size) {
    if (offset + size > rom1.size() || offset + size > rom2.size()) {
      return false;
    }

    for (uint32_t i = 0; i < size; ++i) {
      auto byte1 = rom1.ReadByte(offset + i);
      auto byte2 = rom2.ReadByte(offset + i);
      if (!byte1.ok() || !byte2.ok() || *byte1 != *byte2) {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief Read a byte from ROM with default value on error
   */
  uint8_t ReadByteOrDefault(Rom& rom, uint32_t offset, uint8_t default_value = 0) {
    auto result = rom.ReadByte(offset);
    return result.ok() ? *result : default_value;
  }

  /**
   * @brief Read a word from ROM with default value on error
   */
  uint16_t ReadWordOrDefault(Rom& rom, uint32_t offset, uint16_t default_value = 0) {
    auto result = rom.ReadWord(offset);
    return result.ok() ? *result : default_value;
  }

  /**
   * @brief Verify ROM byte matches expected value
   */
  ::testing::AssertionResult VerifyRomByte(Rom& rom, uint32_t offset,
                                           uint8_t expected,
                                           const std::string& description = "") {
    auto result = rom.ReadByte(offset);
    if (!result.ok()) {
      return ::testing::AssertionFailure()
             << "Failed to read byte at 0x" << std::hex << offset
             << (description.empty() ? "" : " (" + description + ")");
    }
    if (*result != expected) {
      return ::testing::AssertionFailure()
             << "Byte mismatch at 0x" << std::hex << offset
             << ": expected 0x" << static_cast<int>(expected)
             << ", got 0x" << static_cast<int>(*result)
             << (description.empty() ? "" : " (" + description + ")");
    }
    return ::testing::AssertionSuccess();
  }

  /**
   * @brief Verify ROM word matches expected value
   */
  ::testing::AssertionResult VerifyRomWord(Rom& rom, uint32_t offset,
                                           uint16_t expected,
                                           const std::string& description = "") {
    auto result = rom.ReadWord(offset);
    if (!result.ok()) {
      return ::testing::AssertionFailure()
             << "Failed to read word at 0x" << std::hex << offset
             << (description.empty() ? "" : " (" + description + ")");
    }
    if (*result != expected) {
      return ::testing::AssertionFailure()
             << "Word mismatch at 0x" << std::hex << offset
             << ": expected 0x" << expected
             << ", got 0x" << *result
             << (description.empty() ? "" : " (" + description + ")");
    }
    return ::testing::AssertionSuccess();
  }

  // ===========================================================================
  // Corruption Detection Helpers
  // ===========================================================================

  /**
   * @brief Record ROM state for later comparison (records specific regions)
   */
  struct RomSnapshot {
    std::vector<uint8_t> data;
    uint32_t offset;
    uint32_t size;
  };

  RomSnapshot TakeSnapshot(Rom& rom, uint32_t offset, uint32_t size) {
    RomSnapshot snapshot;
    snapshot.offset = offset;
    snapshot.size = std::min(size, static_cast<uint32_t>(rom.size() - offset));
    snapshot.data.resize(snapshot.size);

    for (uint32_t i = 0; i < snapshot.size; ++i) {
      auto byte = rom.ReadByte(offset + i);
      snapshot.data[i] = byte.ok() ? *byte : 0;
    }

    return snapshot;
  }

  /**
   * @brief Verify ROM region matches snapshot (no corruption)
   */
  ::testing::AssertionResult VerifyNoCorruption(Rom& rom,
                                                 const RomSnapshot& snapshot,
                                                 const std::string& region_name = "") {
    for (uint32_t i = 0; i < snapshot.size; ++i) {
      auto byte = rom.ReadByte(snapshot.offset + i);
      if (!byte.ok()) {
        return ::testing::AssertionFailure()
               << "Failed to read byte at offset 0x" << std::hex
               << (snapshot.offset + i);
      }
      if (*byte != snapshot.data[i]) {
        return ::testing::AssertionFailure()
               << "Corruption detected in " << (region_name.empty() ? "ROM region" : region_name)
               << " at offset 0x" << std::hex << (snapshot.offset + i)
               << ": expected 0x" << static_cast<int>(snapshot.data[i])
               << ", got 0x" << static_cast<int>(*byte);
      }
    }
    return ::testing::AssertionSuccess();
  }

  // ===========================================================================
  // Test Utility Methods
  // ===========================================================================

  /**
   * @brief Get path to expanded ROM for v3 feature tests
   */
  std::string GetExpandedRomPath() {
    return TestRomManager::GetRomPath(RomRole::kExpanded);
  }

  /**
   * @brief Skip test if expanded ROM is required but not available
   */
  void RequireExpandedRom() {
    TestRomManager::SkipIfRomMissing(RomRole::kExpanded, "ExpandedRomSaveTest");
  }

  // ===========================================================================
  // Member Variables
  // ===========================================================================

  std::string vanilla_rom_path_;
  std::string test_rom_path_;
  std::string backup_rom_path_;
  std::string test_id_;
};

/**
 * @brief Extended test fixture for multi-ROM version testing
 */
class MultiVersionEditorSaveTest : public EditorSaveTestBase {
 protected:
  void SetUp() override {
    EditorSaveTestBase::SetUp();

    // Check for additional ROM versions
    jp_rom_path_ = TestRomManager::GetRomPath(RomRole::kJp);
    us_rom_path_ = TestRomManager::GetRomPath(RomRole::kUs);
    eu_rom_path_ = TestRomManager::GetRomPath(RomRole::kEu);
  }

  bool HasJpRom() const { return !jp_rom_path_.empty(); }
  bool HasUsRom() const { return !us_rom_path_.empty(); }
  bool HasEuRom() const { return !eu_rom_path_.empty(); }

  std::string jp_rom_path_;
  std::string us_rom_path_;
  std::string eu_rom_path_;
};

/**
 * @brief Test fixture specifically for ZSCustomOverworld v3 expanded ROMs
 */
class ExpandedRomSaveTest : public EditorSaveTestBase {
 protected:
  void SetUp() override {
    TestRomManager::SkipIfRomMissing(RomRole::kExpanded, "ExpandedRomSaveTest");
    expanded_rom_path_ = TestRomManager::GetRomPath(RomRole::kExpanded);

    // Use vanilla for baseline comparison
    vanilla_rom_path_ = TestRomManager::GetRomPath(RomRole::kVanilla);

    // Create test file paths
    test_id_ = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    test_rom_path_ = "test_expanded_" + test_id_ + ".sfc";
    backup_rom_path_ = "backup_expanded_" + test_id_ + ".sfc";

    // Copy expanded ROM for testing
    std::error_code ec;
    std::filesystem::copy_file(
        expanded_rom_path_, test_rom_path_,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
      GTEST_SKIP() << "Failed to copy expanded ROM: " << ec.message();
    }

    std::filesystem::copy_file(
        expanded_rom_path_, backup_rom_path_,
        std::filesystem::copy_options::overwrite_existing, ec);
  }

  std::string expanded_rom_path_;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_ROM_DEPENDENT_EDITOR_SAVE_TEST_BASE_H
