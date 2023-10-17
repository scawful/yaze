#include "app/gfx/compression.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>

#include "absl/status/statusor.h"
#include "app/rom.h"

#define BUILD_HEADER(command, length) (command << 5) + (length - 1)

namespace yaze_test {
namespace gfx_test {

using yaze::app::ROM;
using yaze::app::gfx::lc_lz2::CompressionContext;
using yaze::app::gfx::lc_lz2::CompressionPiece;
using yaze::app::gfx::lc_lz2::CompressV2;
using yaze::app::gfx::lc_lz2::CompressV3;
using yaze::app::gfx::lc_lz2::DecompressV2;
using yaze::app::gfx::lc_lz2::kCommandByteFill;
using yaze::app::gfx::lc_lz2::kCommandDirectCopy;
using yaze::app::gfx::lc_lz2::kCommandIncreasingFill;
using yaze::app::gfx::lc_lz2::kCommandLongLength;
using yaze::app::gfx::lc_lz2::kCommandRepeatingBytes;
using yaze::app::gfx::lc_lz2::kCommandWordFill;

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::TypedEq;

namespace {

Bytes ExpectCompressOk(ROM& rom, uchar* in, int in_size) {
  auto load_status = rom.LoadFromPointer(in, in_size);
  EXPECT_TRUE(load_status.ok());
  auto compression_status = CompressV3(rom.vector(), 0, in_size);
  EXPECT_TRUE(compression_status.ok());
  auto compressed_bytes = std::move(*compression_status);
  return compressed_bytes;
}

Bytes ExpectDecompressBytesOk(ROM& rom, Bytes& in) {
  auto load_status = rom.LoadFromBytes(in);
  EXPECT_TRUE(load_status.ok());
  auto decompression_status = DecompressV2(rom.data(), 0, in.size());
  EXPECT_TRUE(decompression_status.ok());
  auto decompressed_bytes = std::move(*decompression_status);
  return decompressed_bytes;
}

Bytes ExpectDecompressOk(ROM& rom, uchar* in, int in_size) {
  auto load_status = rom.LoadFromPointer(in, in_size);
  EXPECT_TRUE(load_status.ok());
  auto decompression_status = DecompressV2(rom.data(), 0, in_size);
  EXPECT_TRUE(decompression_status.ok());
  auto decompressed_bytes = std::move(*decompression_status);
  return decompressed_bytes;
}

std::shared_ptr<CompressionPiece> ExpectNewCompressionPieceOk(
    const char command, const int length, const std::string args,
    const int argument_length) {
  auto new_piece = std::make_shared<CompressionPiece>(command, length, args,
                                                      argument_length);
  EXPECT_TRUE(new_piece != nullptr);
  return new_piece;
}

// Helper function to assert compression quality.
void AssertCompressionQuality(
    const std::vector<uint8_t>& uncompressed_data,
    const std::vector<uint8_t>& expected_compressed_data) {
  absl::StatusOr<Bytes> result =
      CompressV3(uncompressed_data, 0, uncompressed_data.size(), 0, false);
  ASSERT_TRUE(result.ok());
  auto compressed_data = std::move(*result);
  EXPECT_THAT(compressed_data, ElementsAreArray(expected_compressed_data));
}

Bytes ExpectCompressV3Ok(const std::vector<uint8_t>& uncompressed_data,
                         const std::vector<uint8_t>& expected_compressed_data) {
  absl::StatusOr<Bytes> result =
      CompressV3(uncompressed_data, 0, uncompressed_data.size(), 0, false);
  EXPECT_TRUE(result.ok());
  auto compressed_data = std::move(*result);
  return compressed_data;
}

std::vector<uint8_t> CreateRepeatedBetweenUncompressable(
    int leftUncompressedSize, int repeatedByteSize, int rightUncompressedSize) {
  std::vector<uint8_t> result(
      leftUncompressedSize + repeatedByteSize + rightUncompressedSize, 0);
  std::fill_n(result.begin() + leftUncompressedSize, repeatedByteSize, 0x00);
  return result;
}

}  // namespace

TEST(LC_LZ2_CompressionTest, TrivialRepeatedBytes) {
  AssertCompressionQuality({0x00, 0x00, 0x00}, {0x22, 0x00, 0xFF});
}

TEST(LC_LZ2_CompressionTest, RepeatedBytesBetweenUncompressable) {
  AssertCompressionQuality({0x01, 0x00, 0x00, 0x00, 0x10},
                           {0x04, 0x01, 0x00, 0x00, 0x00, 0x10, 0xFF});
}

TEST(LC_LZ2_CompressionTest, RepeatedBytesBeforeUncompressable) {
  AssertCompressionQuality({0x00, 0x00, 0x00, 0x10},
                           {0x22, 0x00, 0x00, 0x10, 0xFF});
}

TEST(LC_LZ2_CompressionTest, RepeatedBytesAfterUncompressable) {
  AssertCompressionQuality({0x01, 0x00, 0x00, 0x00},
                           {0x00, 0x01, 0x22, 0x00, 0xFF});
}

TEST(LC_LZ2_CompressionTest, RepeatedBytesAfterUncompressableRepeated) {
  AssertCompressionQuality(
      {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02},
      {0x22, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x02, 0xFF});
}

TEST(LC_LZ2_CompressionTest, RepeatedBytesBeforeUncompressableRepeated) {
  AssertCompressionQuality(
      {0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00},
      {0x04, 0x01, 0x00, 0x00, 0x00, 0x02, 0x22, 0x00, 0xFF});
}

TEST(LC_LZ2_CompressionTest, CompressionDecompressionEmptyData) {
  ROM rom;
  uchar empty_input[0] = {};
  auto comp_result = ExpectCompressOk(rom, empty_input, 0);
  EXPECT_EQ(0, comp_result.size());

  auto decomp_result = ExpectDecompressOk(rom, empty_input, 0);
  EXPECT_EQ(0, decomp_result.size());
}

TEST(LC_LZ2_CompressionTest, NewDecompressionPieceOk) {
  char command = 1;
  int length = 1;
  char args[] = "aaa";
  int argument_length = 0x02;
  CompressionPiece old_piece;
  old_piece.command = command;
  old_piece.length = length;
  old_piece.argument = args;
  old_piece.argument_length = argument_length;
  old_piece.next = nullptr;

  auto new_piece = ExpectNewCompressionPieceOk(0x01, 0x01, "aaa", 0x02);

  EXPECT_EQ(old_piece.command, new_piece->command);
  EXPECT_EQ(old_piece.length, new_piece->length);
  ASSERT_EQ(old_piece.argument_length, new_piece->argument_length);
  for (int i = 0; i < old_piece.argument_length; ++i) {
    EXPECT_EQ(old_piece.argument[i], new_piece->argument[i]);
  }
}

// TODO: Check why header built is off by one
// 0x25 instead of 0x24
TEST(LC_LZ2_CompressionTest, CompressionSingleSet) {
  ROM rom;
  uchar single_set[5] = {0x2A, 0x2A, 0x2A, 0x2A, 0x2A};
  uchar single_set_expected[3] = {BUILD_HEADER(1, 5), 0x2A, 0xFF};

  auto comp_result = ExpectCompressOk(rom, single_set, 5);
  EXPECT_THAT(single_set_expected, ElementsAreArray(comp_result.data(), 3));
}

TEST(LC_LZ2_CompressionTest, CompressionSingleWord) {
  ROM rom;
  uchar single_word[6] = {0x2A, 0x01, 0x2A, 0x01, 0x2A, 0x01};
  uchar single_word_expected[4] = {BUILD_HEADER(0x02, 0x06), 0x2A, 0x01, 0xFF};

  auto comp_result = ExpectCompressOk(rom, single_word, 6);
  EXPECT_THAT(single_word_expected, ElementsAreArray(comp_result.data(), 4));
}

TEST(LC_LZ2_CompressionTest, CompressionSingleIncrement) {
  ROM rom;
  uchar single_inc[3] = {0x01, 0x02, 0x03};
  uchar single_inc_expected[3] = {BUILD_HEADER(0x03, 0x03), 0x01, 0xFF};
  auto comp_result = ExpectCompressOk(rom, single_inc, 3);
  EXPECT_THAT(single_inc_expected, ElementsAreArray(comp_result.data(), 3));
}

TEST(LC_LZ2_CompressionTest, CompressionSingleCopy) {
  ROM rom;
  uchar single_copy[4] = {0x03, 0x0A, 0x07, 0x14};
  uchar single_copy_expected[6] = {
      BUILD_HEADER(0x00, 0x04), 0x03, 0x0A, 0x07, 0x14, 0xFF};
  auto comp_result = ExpectCompressOk(rom, single_copy, 4);
  EXPECT_THAT(single_copy_expected, ElementsAreArray(comp_result.data(), 6));
}

TEST(LC_LZ2_CompressionTest, CompressionSingleOverflowIncrement) {
  AssertCompressionQuality({0xFE, 0xFF, 0x00, 0x01},
                           {BUILD_HEADER(0x03, 0x04), 0xFE, 0xFF});
}
/**

TEST(LC_LZ2_CompressionTest, CompressionSingleCopyRepeat) {
  std::vector<uint8_t> single_copy_expected = {0x03, 0x0A, 0x07, 0x14,
                                               0x03, 0x0A, 0x07, 0x14};

  auto comp_result = ExpectCompressV3Ok(
      single_copy_expected, {BUILD_HEADER(0x00, 0x04), 0x03, 0x0A, 0x07, 0x14,
                             BUILD_HEADER(0x04, 0x04), 0x00, 0x00, 0xFF});
  EXPECT_THAT(single_copy_expected, ElementsAreArray(comp_result.data(), 6));
}

TEST(LC_LZ2_CompressionTest, CompressionMixedRepeatIncrement) {
  AssertCompressionQuality(
      {0x05, 0x05, 0x05, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
       0x05, 0x02, 0x05, 0x02, 0x05, 0x02, 0x0A, 0x0B, 0x05, 0x02,
       0x05, 0x02, 0x05, 0x02, 0x08, 0x0A, 0x00, 0x05},
      {BUILD_HEADER(0x01, 0x04), 0x05, BUILD_HEADER(0x03, 0x06), 0x06,
       BUILD_HEADER(0x00, 0x01), 0x05, 0xFF});
}

TEST(LC_LZ2_CompressionTest, CompressionMixedIncrementIntraCopyOffset) {
  // "Mixing, inc, alternate, intra copy"
  // compress start: 3, length: 21
  // compressed length: 9
  AssertCompressionQuality(
      {0x05, 0x05, 0x05, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
       0x05, 0x02, 0x05, 0x02, 0x05, 0x02, 0x0A, 0x0B, 0x05, 0x02,
       0x05, 0x02, 0x05, 0x02, 0x08, 0x0A, 0x00, 0x05},
      {BUILD_HEADER(0x03, 0x07), 0x05, BUILD_HEADER(0x02, 0x06), 0x05, 0x02,
       BUILD_HEADER(0x04, 0x08), 0x05, 0x00, 0xFF});
}

TEST(LC_LZ2_CompressionTest, CompressionMixedIncrementIntraCopySource) {
  // "Mixing, inc, alternate, intra copy"
  // 0, 28
  // 16
  AssertCompressionQuality(
      {0x05, 0x05, 0x05, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
       0x05, 0x02, 0x05, 0x02, 0x05, 0x02, 0x0A, 0x0B, 0x05, 0x02,
       0x05, 0x02, 0x05, 0x02, 0x08, 0x0A, 0x00, 0x05},
      {BUILD_HEADER(0x01, 0x04), 0x05, BUILD_HEADER(0x03, 0x06), 0x06,
       BUILD_HEADER(0x02, 0x06), 0x05, 0x02, BUILD_HEADER(0x04, 0x08), 0x08,
       0x00, BUILD_HEADER(0x00, 0x04), 0x08, 0x0A, 0x00, 0x05, 0xFF});
}

// Extended Header
// 111CCCLL LLLLLLLL
// CCC:        Real command
// LLLLLLLLLL: Length

// Normally you have 5 bits for the length, so the maximum value you can
// represent is 31 (which outputs 32 bytes). With the long length, you get 5
// more bits for the length, so the maximum value you can represent becomes
// 1023, outputting 1024 bytes at a time.

void build_extended_header(uint8_t command, uint8_t length, uint8_t& byte1,
                           uint8_t& byte2) {
  byte1 = command << 3;
  byte1 += (length - 1);
  byte1 += 0b11100000;
  byte2 = length >> 3;
}

std::vector<uint8_t> CreateRepeatedBetweenUncompressable(
    int leftUncompressedSize, int repeatedByteSize, int rightUncompressedSize) {
  std::vector<uint8_t> result(
      leftUncompressedSize + repeatedByteSize + rightUncompressedSize, 0);
  std::fill_n(result.begin() + leftUncompressedSize, repeatedByteSize, 0x00);
  return result;
}

TEST(LC_LZ2_CompressionTest, LengthBorderCompression) {
  // "Length border compression"
  std::vector<uint8_t> result(42, 0);
  std::fill_n(result.begin(), 42, 0x05);
  AssertCompressionQuality(result, {BUILD_HEADER(0x04, 42), 0x05, 0x05, 0xFF});

  // "Extended length, 400 repeat of 5"
  std::vector<uint8_t> result2(400, 0);
  std::fill_n(result2.begin(), 400, 0x05);
  uint8_t byte1;
  uint8_t byte2;
  build_extended_header(0x01, 42, byte1, byte2);
  AssertCompressionQuality(result2, {byte1, byte2, 0x05, 0x05, 0xFF});

  // "Extended length, 1050 repeat of 5"
  std::vector<uint8_t> result3(1050, 0);
  std::fill_n(result3.begin(), 1050, 0x05);
  uint8_t byte3;
  uint8_t byte4;
  build_extended_header(0x04, 1050, byte3, byte4);
  AssertCompressionQuality(result3, {byte3, byte4, 0x05, 0x05, 0xFF});

  // // "Extended length, 2050 repeat of 5"
  std::vector<uint8_t> result4(2050, 0);
  std::fill_n(result4.begin(), 2050, 0x05);
  uint8_t byte5;
  uint8_t byte6;
  build_extended_header(0x04, 2050, byte5, byte6);
  AssertCompressionQuality(result4, {byte5, byte6, 0x05, 0x05, 0xFF});
}

TEST(LC_LZ2_CompressionTest, CompressionExtendedWordCopy) {
  // ROM rom;
  // uchar buffer[3000];
  // for (unsigned int i = 0; i < 3000; i += 2) {
  //   buffer[i] = 0x05;
  //   buffer[i + 1] = 0x06;
  // }
  // uchar hightlength_word_1050[] = {
  //     0b11101011, 0xFF, 0x05, 0x06, BUILD_HEADER(0x02, 0x1A), 0x05, 0x06,
  //     0xFF};

  // // "Extended word copy"
  // auto comp_result = ExpectCompressOk(rom, buffer, 1050);
  // EXPECT_THAT(hightlength_word_1050, ElementsAreArray(comp_result.data(),
  // 8));

  std::vector<uint8_t> buffer(3000, 0);
  std::fill_n(buffer.begin(), 3000, 0x05);
  for (unsigned int i = 0; i < 3000; i += 2) {
    buffer[i] = 0x05;
    buffer[i + 1] = 0x06;
  }

  uint8_t byte1;
  uint8_t byte2;
  build_extended_header(0x02, 0x1A, byte1, byte2);
  AssertCompressionQuality(
      buffer, {0b11101011, 0xFF, 0x05, 0x06, byte1, byte2, 0x05, 0x06, 0xFF});
}

TEST(LC_LZ2_CompressionTest, CompressionMixedPatterns) {
  AssertCompressionQuality(
      {0x05, 0x05, 0x05, 0x06, 0x07, 0x06, 0x07, 0x08, 0x09, 0x0A},
      {BUILD_HEADER(0x01, 0x03), 0x05, BUILD_HEADER(0x02, 0x04), 0x06, 0x07,
       BUILD_HEADER(0x03, 0x03), 0x08, 0xFF});
}

TEST(LC_LZ2_CompressionTest, CompressionLongIntraCopy) {
  ROM rom;
  uchar long_data[15] = {0x05, 0x06, 0x07, 0x08, 0x05, 0x06, 0x07, 0x08,
                         0x05, 0x06, 0x07, 0x08, 0x05, 0x06, 0x07};
  uchar long_expected[] = {BUILD_HEADER(0x00, 0x04), 0x05, 0x06, 0x07, 0x08,
                           BUILD_HEADER(0x04, 0x0C), 0x00, 0x00, 0xFF};

  auto comp_result = ExpectCompressOk(rom, long_data, 15);
  EXPECT_THAT(long_expected,
              ElementsAreArray(comp_result.data(), sizeof(long_expected)));
}

*/
// Tests for HandleDirectCopy

TEST(HandleDirectCopyTest, NotDirectCopyWithAccumulatedBytes) {
  CompressionContext context({0x01, 0x02, 0x03}, 0, 3);
  context.cmd_with_max = kCommandByteFill;
  context.comp_accumulator = 2;
  HandleDirectCopy(context);
  EXPECT_EQ(context.compressed_data.size(), 3);
}

TEST(HandleDirectCopyTest, NotDirectCopyWithoutAccumulatedBytes) {
  CompressionContext context({0x01, 0x02, 0x03}, 0, 3);
  context.cmd_with_max = kCommandByteFill;
  HandleDirectCopy(context);
  EXPECT_EQ(context.compressed_data.size(), 2);  // Header + 1 byte
}

TEST(HandleDirectCopyTest, AccumulateBytesWithoutMax) {
  CompressionContext context({0x01, 0x02, 0x03}, 0, 3);
  context.cmd_with_max = kCommandDirectCopy;
  HandleDirectCopy(context);
  EXPECT_EQ(context.comp_accumulator, 1);
  EXPECT_EQ(context.compressed_data.size(), 0);  // No data added yet
}

// Tests for CheckIncByteV3

TEST(CheckIncByteV3Test, IncreasingSequence) {
  CompressionContext context({0x01, 0x02, 0x03}, 0, 3);
  CheckIncByteV3(context);
  EXPECT_EQ(context.current_cmd.data_size[kCommandIncreasingFill], 3);
}

TEST(CheckIncByteV3Test, IncreasingSequenceSurroundedByIdenticalBytes) {
  CompressionContext context({0x01, 0x02, 0x03, 0x04, 0x01}, 1,
                             3);  // Start from index 1
  CheckIncByteV3(context);
  EXPECT_EQ(context.current_cmd.data_size[kCommandIncreasingFill],
            0);  // Reset to prioritize direct copy
}

TEST(CheckIncByteV3Test, NotAnIncreasingSequence) {
  CompressionContext context({0x01, 0x01, 0x03}, 0, 3);
  CheckIncByteV3(context);
  EXPECT_EQ(context.current_cmd.data_size[kCommandIncreasingFill],
            1);  // Only one byte is detected
}

TEST(LC_LZ2_CompressionTest, DecompressionValidCommand) {
  ROM rom;
  Bytes simple_copy_input = {BUILD_HEADER(0x00, 0x02), 0x2A, 0x45, 0xFF};
  uchar simple_copy_output[2] = {0x2A, 0x45};
  auto decomp_result = ExpectDecompressBytesOk(rom, simple_copy_input);
  EXPECT_THAT(simple_copy_output, ElementsAreArray(decomp_result.data(), 2));
}

TEST(LC_LZ2_CompressionTest, DecompressionMixingCommand) {
  ROM rom;
  uchar random1_i[11] = {BUILD_HEADER(0x01, 0x03),
                         0x2A,
                         BUILD_HEADER(0x00, 0x04),
                         0x01,
                         0x02,
                         0x03,
                         0x04,
                         BUILD_HEADER(0x02, 0x02),
                         0x0B,
                         0x16,
                         0xFF};
  uchar random1_o[9] = {42, 42, 42, 1, 2, 3, 4, 11, 22};
  auto decomp_result = ExpectDecompressOk(rom, random1_i, 11);
  EXPECT_THAT(random1_o, ElementsAreArray(decomp_result.data(), 9));
}

}  // namespace gfx_test
}  // namespace yaze_test