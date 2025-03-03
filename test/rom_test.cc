#include "app/rom.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "mocks/mock_rom.h"
#include "test/testing.h"

namespace yaze {
namespace test {

using ::testing::_;
using ::testing::Return;

const static std::vector<uint8_t> kMockRomData = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
};

class RomTest : public ::testing::Test {
 protected:
  Rom rom_;
};

TEST_F(RomTest, Uninitialized) {
  EXPECT_EQ(rom_.size(), 0);
  EXPECT_EQ(rom_.data(), nullptr);
}

TEST_F(RomTest, LoadFromFile) {
#if defined(__linux__)
  GTEST_SKIP();
#endif
  EXPECT_OK(rom_.LoadFromFile("zelda3.sfc"));
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
  EXPECT_OK(rom_.LoadFromData(kMockRomData, false));

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
  EXPECT_OK(rom_.LoadFromData(kMockRomData, false));

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

TEST_F(RomTest, ReadLongOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData, false));

  for (size_t i = 0; i < kMockRomData.size(); i += 4) {
    // Little endian
    EXPECT_THAT(rom_.ReadLong(i),
                IsOkAndHolds<uint32_t>((kMockRomData[i]) | kMockRomData[i] |
                                       kMockRomData[i + 1] << 8 |
                                       kMockRomData[i + 2] << 16));
  }
}

TEST_F(RomTest, ReadBytesOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData, false));

  std::vector<uint8_t> bytes;
  ASSERT_OK_AND_ASSIGN(bytes, rom_.ReadByteVector(0, kMockRomData.size()));
  EXPECT_THAT(bytes, ::testing::ContainerEq(kMockRomData));
}

TEST_F(RomTest, ReadBytesOutOfRange) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData, false));

  std::vector<uint8_t> bytes;
  EXPECT_THAT(rom_.ReadByteVector(kMockRomData.size() + 1, 1).status(),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST_F(RomTest, WriteByteOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData, false));

  for (size_t i = 0; i < kMockRomData.size(); ++i) {
    EXPECT_OK(rom_.WriteByte(i, 0xFF));
    uint8_t byte;
    ASSERT_OK_AND_ASSIGN(byte, rom_.ReadByte(i));
    EXPECT_EQ(byte, 0xFF);
  }
}

TEST_F(RomTest, WriteWordOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData, false));

  for (size_t i = 0; i < kMockRomData.size(); i += 2) {
    EXPECT_OK(rom_.WriteWord(i, 0xFFFF));
    uint16_t word;
    ASSERT_OK_AND_ASSIGN(word, rom_.ReadWord(i));
    EXPECT_EQ(word, 0xFFFF);
  }
}

TEST_F(RomTest, WriteLongOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData, false));

  for (size_t i = 0; i < kMockRomData.size(); i += 4) {
    EXPECT_OK(rom_.WriteLong(i, 0xFFFFFF));
    uint32_t word;
    ASSERT_OK_AND_ASSIGN(word, rom_.ReadLong(i));
    EXPECT_EQ(word, 0xFFFFFF);
  }
}

TEST_F(RomTest, WriteTransactionSuccess) {
  MockRom mock_rom;
  EXPECT_OK(mock_rom.LoadFromData(kMockRomData, false));

  EXPECT_CALL(mock_rom, WriteHelper(_))
      .WillRepeatedly(Return(absl::OkStatus()));

  EXPECT_OK(mock_rom.WriteTransaction(
      Rom::WriteAction{0x1000, uint8_t{0xFF}},
      Rom::WriteAction{0x1001, uint16_t{0xABCD}},
      Rom::WriteAction{0x1002, std::vector<uint8_t>{0x12, 0x34}}));
}

TEST_F(RomTest, WriteTransactionFailure) {
  MockRom mock_rom;
  EXPECT_OK(mock_rom.LoadFromData(kMockRomData, false));

  EXPECT_CALL(mock_rom, WriteHelper(_))
      .WillOnce(Return(absl::OkStatus()))
      .WillOnce(Return(absl::InternalError("Write failed")));

  EXPECT_EQ(
      mock_rom.WriteTransaction(Rom::WriteAction{0x1000, uint8_t{0xFF}},
                                Rom::WriteAction{0x1001, uint16_t{0xABCD}}),
      absl::InternalError("Write failed"));
}

TEST_F(RomTest, ReadTransactionSuccess) {
  MockRom mock_rom;
  EXPECT_OK(mock_rom.LoadFromData(kMockRomData, false));
  uint8_t byte_val;
  uint16_t word_val;

  EXPECT_OK(mock_rom.ReadTransaction(byte_val, 0x0000, word_val, 0x0001));

  EXPECT_EQ(byte_val, 0x00);
  EXPECT_EQ(word_val, 0x0201);
}

TEST_F(RomTest, ReadTransactionFailure) {
  MockRom mock_rom;
  EXPECT_OK(mock_rom.LoadFromData(kMockRomData, false));
  uint8_t byte_val;

  EXPECT_EQ(mock_rom.ReadTransaction(byte_val, 0x1000),
            absl::FailedPreconditionError("Offset out of range"));
}

}  // namespace test
}  // namespace yaze
