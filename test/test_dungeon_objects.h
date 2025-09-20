#ifndef YAZE_TEST_TEST_DUNGEON_OBJECTS_H
#define YAZE_TEST_TEST_DUNGEON_OBJECTS_H

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "app/rom.h"
#include "gtest/gtest.h"

namespace yaze {
namespace test {

/**
 * @brief Simplified test framework for dungeon object rendering
 * 
 * This provides a clean, focused testing environment for dungeon object
 * functionality without the complexity of full integration tests.
 */
class TestDungeonObjects : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  // Test helpers
  absl::Status CreateTestRom();
  absl::Status SetupObjectData();
  
  // Mock data generators
  std::vector<uint8_t> CreateObjectSubtypeTable(int base_addr, int count);
  std::vector<uint8_t> CreateTileData(int base_addr, int tile_count);
  std::vector<uint8_t> CreateRoomHeader(int room_id);

  std::unique_ptr<Rom> test_rom_;
  
  // Test constants
  static constexpr int kTestObjectId = 0x01;
  static constexpr int kTestRoomId = 0x00;
  static constexpr size_t kTestRomSize = 0x100000; // 1MB test ROM
};

/**
 * @brief Simple mock ROM for testing
 */
class TestRom : public Rom {
 public:
  TestRom() = default;
  ~TestRom() override = default;

  // Override key methods for testing
  absl::Status LoadFromFile(const std::string& filename) override {
    return absl::OkStatus();
  }
  
  absl::Status LoadZelda3() override {
    return absl::OkStatus();
  }
  
  // Test data management
  void SetTestData(const std::vector<uint8_t>& data) {
    test_data_ = data;
    data_ = test_data_.data();
    size_ = test_data_.size();
  }
  
  bool IsValid() const {
    return !test_data_.empty() && data_ != nullptr && size_ > 0;
  }

 private:
  std::vector<uint8_t> test_data_;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_TEST_DUNGEON_OBJECTS_H