#ifndef YAZE_TEST_TEST_UTILS_H
#define YAZE_TEST_TEST_UTILS_H

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace yaze {
namespace test {

/**
 * @brief Utility class for handling test ROM files
 */
class TestRomManager {
 public:
  /**
   * @brief Check if ROM testing is enabled and ROM file exists
   * @return True if ROM tests can be run
   */
  static bool IsRomTestingEnabled() {
#ifdef YAZE_ENABLE_ROM_TESTS
    return std::filesystem::exists(GetTestRomPath());
#else
    return false;
#endif
  }

  /**
   * @brief Get the path to the test ROM file
   * @return Path to the test ROM
   */
  static std::string GetTestRomPath() {
#ifdef YAZE_TEST_ROM_PATH
    return YAZE_TEST_ROM_PATH;
#else
    return "zelda3.sfc";
#endif
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
    if (!IsRomTestingEnabled()) {
      GTEST_SKIP() << "ROM testing disabled or ROM file not found. "
                   << "Test: " << test_name << " requires: " << GetTestRomPath();
    }
  }
};

/**
 * @brief Test macro for ROM-dependent tests
 */
#define YAZE_ROM_TEST(test_case_name, test_name) \
  TEST(test_case_name, test_name) { \
    yaze::test::TestRomManager::SkipIfRomTestingDisabled(#test_case_name "." #test_name); \
    YAZE_ROM_TEST_BODY_##test_case_name##_##test_name(); \
  } \
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

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_TEST_UTILS_H
