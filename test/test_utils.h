#ifndef YAZE_TEST_TEST_UTILS_H
#define YAZE_TEST_TEST_UTILS_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "rom/rom.h"

struct ImGuiTestContext;

namespace yaze {
namespace test {

enum class RomRole {
  kVanilla,
  kUs,
  kJp,
  kEu,
  kExpanded,
};

/**
 * @brief Utility class for handling test ROM files
 */
class TestRomManager {
 public:
  class BoundRomTest;

  /**
   * @brief Auto-discover a ROM file from common locations
   * @return Path to discovered ROM, or empty string if none found
   */
  static std::string AutoDiscoverRom(const std::vector<std::string>& rom_names) {
    // Common directories to search (relative to working directory)
    static const std::vector<std::string> kSearchPaths = {
        ".",
        "roms",
        "../roms",
        "../../roms",
    };

    for (const auto& dir : kSearchPaths) {
      for (const auto& name : rom_names) {
        std::filesystem::path path = std::filesystem::path(dir) / name;
        if (std::filesystem::exists(path)) {
          return path.string();
        }
      }
    }

    return "";
  }

  /**
   * @brief Check if ROM testing is enabled and ROM file exists
   * @return True if ROM tests can be run
   */
  static bool IsRomTestingEnabled() {
    if (std::getenv("YAZE_SKIP_ROM_TESTS")) {
      return false;
    }
    return HasRom(RomRole::kVanilla);
  }

  /**
   * @brief Get the path to the test ROM file
   * @return Path to the test ROM
   */
  static std::string GetTestRomPath() {
    return GetRomPath(RomRole::kVanilla);
  }

  /**
   * @brief Get the path to a ROM role (vanilla/expanded/region)
   * @return Path to the ROM, or empty if not found
   */
  static std::string GetRomPath(RomRole role) {
    switch (role) {
      case RomRole::kVanilla: {
        const auto env_path = GetEnvRomPath(
            {"YAZE_TEST_ROM_VANILLA", "YAZE_TEST_ROM_VANILLA_PATH",
             "YAZE_TEST_ROM_PATH"});
        if (!env_path.empty()) {
          return env_path;
        }
        const auto compile_path = GetCompileRomPath(role);
        if (!compile_path.empty()) {
          return compile_path;
        }
        static const std::vector<std::string> kVanillaNames = {
            "alttp_vanilla.sfc",
            "Legend of Zelda, The - A Link to the Past (USA).sfc",
        };
        return AutoDiscoverRom(kVanillaNames);
      }
      case RomRole::kUs: {
        const auto env_path = GetEnvRomPath(
            {"YAZE_TEST_ROM_US", "YAZE_TEST_ROM_US_PATH"});
        if (!env_path.empty()) {
          return env_path;
        }
        return GetCompileRomPath(role);
      }
      case RomRole::kJp: {
        const auto env_path = GetEnvRomPath(
            {"YAZE_TEST_ROM_JP", "YAZE_TEST_ROM_JP_PATH"});
        if (!env_path.empty()) {
          return env_path;
        }
        return GetCompileRomPath(role);
      }
      case RomRole::kEu: {
        const auto env_path = GetEnvRomPath(
            {"YAZE_TEST_ROM_EU", "YAZE_TEST_ROM_EU_PATH"});
        if (!env_path.empty()) {
          return env_path;
        }
        return GetCompileRomPath(role);
      }
      case RomRole::kExpanded: {
        const auto env_path =
            GetEnvRomPath({"YAZE_TEST_ROM_EXPANDED", "YAZE_TEST_ROM_OOS",
                           "YAZE_TEST_ROM_EXPANDED_PATH"});
        if (!env_path.empty()) {
          return env_path;
        }
        return GetCompileRomPath(role);
      }
      default:
        return "";
    }
  }

  /**
   * @brief Check if a ROM exists for the specified role
   */
  static bool HasRom(RomRole role) {
    const auto rom_path = GetRomPath(role);
    return !rom_path.empty() && std::filesystem::exists(rom_path);
  }

  /**
   * @brief Skip test if the requested ROM role is not available
   */
  static void SkipIfRomMissing(RomRole role, const std::string& test_name) {
    if (std::getenv("YAZE_SKIP_ROM_TESTS")) {
      GTEST_SKIP() << "ROM testing disabled via YAZE_SKIP_ROM_TESTS. Test: "
                   << test_name;
    }
    if (!HasRom(role)) {
      GTEST_SKIP() << "ROM not found for role " << GetRomRoleName(role)
                   << ". Test: " << test_name << ". "
                   << GetRomRoleHint(role);
    }
  }

  /**
   * @brief Human-friendly role name for logging
   */
  static const char* GetRomRoleName(RomRole role) {
    switch (role) {
      case RomRole::kVanilla:
        return "vanilla";
      case RomRole::kUs:
        return "us";
      case RomRole::kJp:
        return "jp";
      case RomRole::kEu:
        return "eu";
      case RomRole::kExpanded:
        return "expanded";
      default:
        return "unknown";
    }
  }

  /**
   * @brief Guidance for setting the correct ROM path env vars
   */
  static std::string GetRomRoleHint(RomRole role) {
    switch (role) {
      case RomRole::kVanilla:
        return "Set YAZE_TEST_ROM_VANILLA or YAZE_TEST_ROM_PATH.";
      case RomRole::kUs:
        return "Set YAZE_TEST_ROM_US.";
      case RomRole::kJp:
        return "Set YAZE_TEST_ROM_JP.";
      case RomRole::kEu:
        return "Set YAZE_TEST_ROM_EU.";
      case RomRole::kExpanded:
        return "Set YAZE_TEST_ROM_EXPANDED or YAZE_TEST_ROM_OOS.";
      default:
        return "Set the appropriate YAZE_TEST_ROM_* env var.";
    }
  }

  /**
   * @brief Load the test ROM file into memory
   * @return Vector containing ROM data, or empty if failed
   */
  static std::vector<uint8_t> LoadTestRom() {
    if (!IsRomTestingEnabled()) {
      return {};
    }

    std::string rom_path = GetTestRomPath();
    std::ifstream file(rom_path, std::ios::binary);
    if (!file) {
      std::cerr << "Failed to open test ROM: " << rom_path << std::endl;
      return {};
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read file
    std::vector<uint8_t> rom_data(file_size);
    file.read(reinterpret_cast<char*>(rom_data.data()), file_size);

    if (!file) {
      std::cerr << "Failed to read test ROM: " << rom_path << std::endl;
      return {};
    }

    return rom_data;
  }

  /**
   * @brief Create a minimal test ROM for unit testing
   * @param size Size of the ROM in bytes
   * @return Vector containing minimal ROM data
   */
  static std::vector<uint8_t> CreateMinimalTestRom(size_t size = 1024 * 1024) {
    std::vector<uint8_t> rom_data(size, 0);

    // Add minimal SNES header at 0x7FC0 (LoROM)
    const size_t header_offset = 0x7FC0;
    if (size > header_offset + 32) {
      // ROM title
      std::string title = "YAZE TEST ROM       ";
      std::copy(title.begin(), title.end(), rom_data.begin() + header_offset);

      // Map mode (LoROM)
      rom_data[header_offset + 21] = 0x20;

      // ROM size (1MB)
      rom_data[header_offset + 23] = 0x0A;

      // Calculate and set checksum
      uint16_t checksum = 0;
      for (size_t i = 0; i < size; ++i) {
        if (i < header_offset + 28 || i > header_offset + 31) {
          checksum += rom_data[i];
        }
      }

      uint16_t checksum_complement = checksum ^ 0xFFFF;
      rom_data[header_offset + 28] = checksum_complement & 0xFF;
      rom_data[header_offset + 29] = (checksum_complement >> 8) & 0xFF;
      rom_data[header_offset + 30] = checksum & 0xFF;
      rom_data[header_offset + 31] = (checksum >> 8) & 0xFF;
    }

    return rom_data;
  }

  /**
   * @brief Skip test if ROM testing is not enabled
   * @param test_name Name of the test for logging
   */
  static void SkipIfRomTestingDisabled(const std::string& test_name) {
    SkipIfRomMissing(RomRole::kVanilla, test_name);
  }

 private:
  static std::string GetEnvRomPath(
      std::initializer_list<const char*> env_vars) {
    for (const char* env_var : env_vars) {
      if (const char* env_path = std::getenv(env_var)) {
        if (std::filesystem::exists(env_path)) {
          return env_path;
        }
      }
    }
    return "";
  }

  static std::string GetCompileRomPath(RomRole role) {
    switch (role) {
      case RomRole::kVanilla:
#ifdef YAZE_TEST_ROM_VANILLA_PATH
        if (std::filesystem::exists(YAZE_TEST_ROM_VANILLA_PATH)) {
          return YAZE_TEST_ROM_VANILLA_PATH;
        }
#endif
#ifdef YAZE_TEST_ROM_PATH
        if (std::filesystem::exists(YAZE_TEST_ROM_PATH)) {
          return YAZE_TEST_ROM_PATH;
        }
#endif
        return "";
      case RomRole::kUs:
#ifdef YAZE_TEST_ROM_US_PATH
        if (std::filesystem::exists(YAZE_TEST_ROM_US_PATH)) {
          return YAZE_TEST_ROM_US_PATH;
        }
#endif
        return "";
      case RomRole::kJp:
#ifdef YAZE_TEST_ROM_JP_PATH
        if (std::filesystem::exists(YAZE_TEST_ROM_JP_PATH)) {
          return YAZE_TEST_ROM_JP_PATH;
        }
#endif
        return "";
      case RomRole::kEu:
#ifdef YAZE_TEST_ROM_EU_PATH
        if (std::filesystem::exists(YAZE_TEST_ROM_EU_PATH)) {
          return YAZE_TEST_ROM_EU_PATH;
        }
#endif
        return "";
      case RomRole::kExpanded:
#ifdef YAZE_TEST_ROM_EXPANDED_PATH
        if (std::filesystem::exists(YAZE_TEST_ROM_EXPANDED_PATH)) {
          return YAZE_TEST_ROM_EXPANDED_PATH;
        }
#endif
        return "";
      default:
        return "";
    }
  }
};

class TestRomManager::BoundRomTest : public ::testing::Test {
 protected:
  void SetUp() override { rom_instance_ = std::make_unique<Rom>(); }

  void TearDown() override {
    rom_instance_.reset();
    rom_loaded_ = false;
  }

  Rom* rom() {
    EnsureRomLoaded();
    return rom_instance_.get();
  }
  const Rom* rom() const { return rom_instance_.get(); }

  std::string GetBoundRomPath() const {
    return TestRomManager::GetRomPath(RomRole::kVanilla);
  }

 private:
  std::unique_ptr<Rom> rom_instance_;
  bool rom_loaded_ = false;

  void EnsureRomLoaded() {
    if (rom_loaded_) {
      return;
    }
    const std::string rom_path = TestRomManager::GetRomPath(RomRole::kVanilla);
    ASSERT_TRUE(rom_instance_->LoadFromFile(rom_path).ok())
        << "Failed to load test ROM from " << rom_path;
    rom_loaded_ = true;
  }
};

/**
 * @brief Test macro for ROM-dependent tests
 */
#define YAZE_ROM_TEST(test_case_name, test_name)                          \
  TEST(test_case_name, test_name) {                                       \
    yaze::test::TestRomManager::SkipIfRomTestingDisabled(#test_case_name  \
                                                         "." #test_name); \
    YAZE_ROM_TEST_BODY_##test_case_name##_##test_name();                  \
  }                                                                       \
  void YAZE_ROM_TEST_BODY_##test_case_name##_##test_name()

/**
 * @brief Test fixture for ROM-dependent tests
 */
class RomDependentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    TestRomManager::SkipIfRomTestingDisabled("RomDependentTest");
    test_rom_ = TestRomManager::LoadTestRom();
    ASSERT_FALSE(test_rom_.empty()) << "Failed to load test ROM";
  }

  std::vector<uint8_t> test_rom_;
};

namespace gui {

void LoadRomInTest(ImGuiTestContext* ctx, const std::string& rom_path);
void OpenEditorInTest(ImGuiTestContext* ctx, const std::string& editor_name);

}  // namespace gui

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_TEST_UTILS_H
