#include "app/rom.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "test/core/testing.h"

namespace yaze {
namespace test {

const static std::vector<uint8_t> kMockRomData = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
};

class RomTest : public ::testing::Test {
 protected:
  app::Rom rom_;
};

TEST_F(RomTest, Uninitialized) {
  EXPECT_EQ(rom_.size(), 0);
  EXPECT_EQ(rom_.data(), nullptr);
}

TEST_F(RomTest, LoadFromFile) {
  EXPECT_OK(rom_.LoadFromFile("test.sfc"));
  EXPECT_EQ(rom_.size(), 0x200000);
  EXPECT_NE(rom_.data(), nullptr);
}

TEST_F(RomTest, LoadFromFileInvalid) {
  EXPECT_THAT(rom_.LoadFromFile("invalid.sfc"),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_EQ(rom_.size(), 0);
  EXPECT_EQ(rom_.data(), nullptr);
}

TEST_F(RomTest, LoadFromFileEmpty) {
  EXPECT_THAT(rom_.LoadFromFile(""),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(RomTest, ReadByteOk) {
  EXPECT_OK(rom_.LoadFromBytes(kMockRomData));

  for (size_t i = 0; i < kMockRomData.size(); ++i) {
    uint8_t byte;
    ASSERT_OK_AND_ASSIGN(byte, rom_.ReadByte(i));
    EXPECT_EQ(byte, kMockRomData[i]);
  }
}

TEST_F(RomTest, ReadByteInvalid) {
  EXPECT_THAT(rom_.ReadByte(0).status(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(RomTest, ReadWordOk) {
  EXPECT_OK(rom_.LoadFromBytes(kMockRomData));

  for (size_t i = 0; i < kMockRomData.size(); i += 2) {
    // Little endian
    EXPECT_THAT(
        rom_.ReadWord(i),
        IsOkAndHolds<uint16_t>((kMockRomData[i]) | kMockRomData[i + 1] << 8));
  }
}

TEST_F(RomTest, ReadWordInvalid) {
  EXPECT_THAT(rom_.ReadWord(0).status(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

}  // namespace test
}  // namespace yaze