#ifndef YAZE_TEST_MOCKS_MOCK_ROM_H
#define YAZE_TEST_MOCKS_MOCK_ROM_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test/testing.h"

namespace yaze {
namespace test {

class MockRom : public Rom {
 public:
  MOCK_METHOD(absl::Status, WriteHelper, (const WriteAction&), (override));

  MOCK_METHOD2(ReadHelper, absl::Status(uint8_t&, int));
  MOCK_METHOD2(ReadHelper, absl::Status(uint16_t&, int));
  MOCK_METHOD2(ReadHelper, absl::Status(std::vector<uint8_t>&, int));

  MOCK_METHOD(absl::StatusOr<uint8_t>, ReadByte, (int));
  MOCK_METHOD(absl::StatusOr<uint16_t>, ReadWord, (int));
  MOCK_METHOD(absl::StatusOr<uint32_t>, ReadLong, (int));

  // Additional methods for dungeon object testing
  MOCK_METHOD(absl::Status, LoadFromFile, (const std::string&), (override));
  MOCK_METHOD(absl::Status, LoadZelda3, (), (override));
  
  // Test data management
  void SetTestData(const std::vector<uint8_t>& data) {
    test_data_ = data;
    data_ = test_data_.data();
    size_ = test_data_.size();
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

 private:
  std::vector<uint8_t> test_data_;
  std::map<int, std::vector<uint8_t>> object_data_;
  std::map<int, std::vector<uint8_t>> room_data_;
};

}  // namespace test
}  // namespace yaze

#endif
