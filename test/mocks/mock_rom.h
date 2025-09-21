#ifndef YAZE_TEST_MOCKS_MOCK_ROM_H
#define YAZE_TEST_MOCKS_MOCK_ROM_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test/testing.h"

namespace yaze {
namespace test {

class MockRom : public Rom {
 public:
  // Override the only virtual method in Rom
  MOCK_METHOD(absl::Status, WriteHelper, (const WriteAction&), (override));

  // Test data management methods
  void SetTestData(const std::vector<uint8_t>& data) {
    // Access protected/private members through public interface
    LoadFromData(data, false);
  }
  
  void SetObjectData(int object_id, const std::vector<uint8_t>& data) {
    object_data_[object_id] = data;
  }
  
  void SetRoomData(int room_id, const std::vector<uint8_t>& data) {
    room_data_[room_id] = data;
  }
  
  bool HasObjectData(int object_id) const {
    return object_data_.find(object_id) != object_data_.end();
  }
  
  bool HasRoomData(int room_id) const {
    return room_data_.find(room_id) != room_data_.end();
  }

  // Helper method to check if ROM is valid for testing
  bool IsValid() const {
    return is_loaded() && size() > 0;
  }

 private:
  std::map<int, std::vector<uint8_t>> object_data_;
  std::map<int, std::vector<uint8_t>> room_data_;
};

}  // namespace test
}  // namespace yaze

#endif
