#ifndef YAZE_TEST_MOCKS_MOCK_ROM_H
#define YAZE_TEST_MOCKS_MOCK_ROM_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/rom.h"
#include "testing.h"

namespace yaze {
namespace test {

/**
 * @brief Enhanced ROM for testing that behaves like a real ROM but with test
 * data
 *
 * This class extends Rom to provide testing utilities while maintaining
 * all the real ROM functionality. Instead of mocking methods, it loads
 * real test data into the ROM.
 */
class MockRom : public Rom {
 public:
  MockRom() = default;

  // Override the only virtual method in Rom
  MOCK_METHOD(absl::Status, WriteHelper, (const WriteAction&), (override));

  /**
   * @brief Load test data into the ROM
   * @param data The test ROM data to load
   * @return Status of the operation
   */
  absl::Status SetTestData(const std::vector<uint8_t>& data) {
    auto status = LoadFromData(data, false);  // Don't load Zelda3 specific data
    if (status.ok()) {
      test_data_loaded_ = true;
    }
    return status;
  }

  /**
   * @brief Store object-specific test data for validation
   */
  void SetObjectData(int object_id, const std::vector<uint8_t>& data) {
    object_data_[object_id] = data;
  }

  /**
   * @brief Store room-specific test data for validation
   */
  void SetRoomData(int room_id, const std::vector<uint8_t>& data) {
    room_data_[room_id] = data;
  }

  /**
   * @brief Check if object data has been set for testing
   */
  bool HasObjectData(int object_id) const {
    return object_data_.find(object_id) != object_data_.end();
  }

  /**
   * @brief Check if room data has been set for testing
   */
  bool HasRoomData(int room_id) const {
    return room_data_.find(room_id) != room_data_.end();
  }

  /**
   * @brief Check if the mock ROM is valid for testing
   */
  bool IsValid() const {
    return test_data_loaded_ && is_loaded() && size() > 0;
  }

  /**
   * @brief Get the stored object data for validation
   */
  const std::vector<uint8_t>& GetObjectData(int object_id) const {
    static const std::vector<uint8_t> empty;
    auto it = object_data_.find(object_id);
    return (it != object_data_.end()) ? it->second : empty;
  }

  /**
   * @brief Get the stored room data for validation
   */
  const std::vector<uint8_t>& GetRoomData(int room_id) const {
    static const std::vector<uint8_t> empty;
    auto it = room_data_.find(room_id);
    return (it != room_data_.end()) ? it->second : empty;
  }

 private:
  bool test_data_loaded_ = false;
  std::map<int, std::vector<uint8_t>> object_data_;
  std::map<int, std::vector<uint8_t>> room_data_;
};

}  // namespace test
}  // namespace yaze

#endif
