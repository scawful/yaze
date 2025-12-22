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
#include "testing.h"
#include "util/macro.h"
#include "zelda.h"

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
 * @brief Environment variable names for ROM paths
 */
struct RomEnvVars {
  static constexpr const char* kDefaultRomPath = "YAZE_TEST_ROM_PATH";
  static constexpr const char* kSkipRomTests = "YAZE_SKIP_ROM_TESTS";
  static constexpr const char* kJpRomPath = "YAZE_TEST_ROM_JP_PATH";
  static constexpr const char* kUsRomPath = "YAZE_TEST_ROM_US_PATH";
  static constexpr const char* kEuRomPath = "YAZE_TEST_ROM_EU_PATH";
  static constexpr const char* kExpandedRomPath = "YAZE_TEST_ROM_EXPANDED_PATH";
};

/**
 * @brief Default ROM paths relative to workspace (roms/ directory)
 */
struct DefaultRomPaths {
  static constexpr const char* kVanilla = "roms/alttp_vanilla.sfc";
  static constexpr const char* kUsRom = "roms/Legend of Zelda, The - A Link to the Past (USA).sfc";
  static constexpr const char* kExpanded = "roms/oos168.sfc";
  static constexpr const char* kFallback = "zelda3.sfc";
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
    // Skip tests if ROM tests are disabled
    if (getenv(RomEnvVars::kSkipRomTests)) {
      GTEST_SKIP() << "ROM tests disabled via YAZE_SKIP_ROM_TESTS";
    }

    // Determine ROM path
    const char* rom_path_env = getenv(RomEnvVars::kDefaultRomPath);
    if (rom_path_env && std::filesystem::exists(rom_path_env)) {
      vanilla_rom_path_ = rom_path_env;
    } else if (std::filesystem::exists(DefaultRomPaths::kVanilla)) {
      vanilla_rom_path_ = DefaultRomPaths::kVanilla;
    } else if (std::filesystem::exists(DefaultRomPaths::kUsRom)) {
      vanilla_rom_path_ = DefaultRomPaths::kUsRom;
    } else if (std::filesystem::exists(DefaultRomPaths::kFallback)) {
      vanilla_rom_path_ = DefaultRomPaths::kFallback;
    } else {
      GTEST_SKIP() << "No test ROM found. Set YAZE_TEST_ROM_PATH or place ROM in roms/";
    }

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
    settings.save_new = true;

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

    // Detect expanded tile16
    auto tile16_check = rom.ReadByte(0x02FD28);
    info.is_expanded_tile16 = tile16_check.ok() && *tile16_check != 0x0F;

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
    const char* expanded_path = getenv(RomEnvVars::kExpandedRomPath);
    if (expanded_path && std::filesystem::exists(expanded_path)) {
      return expanded_path;
    }
    if (std::filesystem::exists(DefaultRomPaths::kExpanded)) {
      return DefaultRomPaths::kExpanded;
    }
    return "";  // Not available
  }

  /**
   * @brief Skip test if expanded ROM is required but not available
   */
  void RequireExpandedRom() {
    std::string path = GetExpandedRomPath();
    if (path.empty()) {
      GTEST_SKIP() << "Expanded ROM not available for v3 feature tests";
    }
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
    const char* jp_path = getenv(RomEnvVars::kJpRomPath);
    const char* us_path = getenv(RomEnvVars::kUsRomPath);
    const char* eu_path = getenv(RomEnvVars::kEuRomPath);

    if (jp_path && std::filesystem::exists(jp_path)) {
      jp_rom_path_ = jp_path;
    }
    if (us_path && std::filesystem::exists(us_path)) {
      us_rom_path_ = us_path;
    } else if (std::filesystem::exists(DefaultRomPaths::kUsRom)) {
      us_rom_path_ = DefaultRomPaths::kUsRom;
    }
    if (eu_path && std::filesystem::exists(eu_path)) {
      eu_rom_path_ = eu_path;
    }
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
    // Skip if ROM tests disabled
    if (getenv(RomEnvVars::kSkipRomTests)) {
      GTEST_SKIP() << "ROM tests disabled via YAZE_SKIP_ROM_TESTS";
    }

    // Get expanded ROM path
    const char* expanded_path = getenv(RomEnvVars::kExpandedRomPath);
    if (expanded_path && std::filesystem::exists(expanded_path)) {
      expanded_rom_path_ = expanded_path;
    } else if (std::filesystem::exists(DefaultRomPaths::kExpanded)) {
      expanded_rom_path_ = DefaultRomPaths::kExpanded;
    } else {
      GTEST_SKIP() << "Expanded ROM not available. Set YAZE_TEST_ROM_EXPANDED_PATH";
    }

    // Use vanilla for baseline comparison
    if (std::filesystem::exists(DefaultRomPaths::kVanilla)) {
      vanilla_rom_path_ = DefaultRomPaths::kVanilla;
    } else {
      vanilla_rom_path_ = "";
    }

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
