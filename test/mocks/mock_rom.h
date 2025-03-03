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
};

}  // namespace test
}  // namespace yaze

#endif
