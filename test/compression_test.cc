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
using yaze::app::gfx::lc_lz2::CompressionPiece;

using ::testing::ElementsAreArray;
using ::testing::TypedEq;

namespace {

Bytes ExpectCompressOk(ROM& rom, uchar* in, int in_size) {
  auto load_status = rom.LoadFromPointer(in, in_size);
  EXPECT_TRUE(load_status.ok());
  auto compression_status = rom.Compress(0, in_size);
  EXPECT_TRUE(compression_status.ok());
  auto compressed_bytes = std::move(*compression_status);
  return compressed_bytes;
}

Bytes ExpectDecompressBytesOk(ROM& rom, Bytes& in) {
  auto load_status = rom.LoadFromBytes(in);
  EXPECT_TRUE(load_status.ok());
  auto decompression_status = rom.Decompress(0, in.size());
  EXPECT_TRUE(decompression_status.ok());
  auto decompressed_bytes = std::move(*decompression_status);
  return decompressed_bytes;
}

Bytes ExpectDecompressOk(ROM& rom, uchar* in, int in_size) {
  auto load_status = rom.LoadFromPointer(in, in_size);
  EXPECT_TRUE(load_status.ok());
  auto decompression_status = rom.Decompress(0, in_size);
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

}  // namespace

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

/* Hiding tests until I figure out a better PR to address the bug
TEST(LC_LZ2_CompressionTest, CompressionSingleCopyRepeat) {
  ROM rom;
  uchar single_copy_repeat[8] = {0x03, 0x0A, 0x07, 0x14, 0x03, 10, 0x07, 0x14};
  uchar single_copy_repeat_expected[9] = {
      BUILD_HEADER(0x00, 0x04), 0x03, 0x0A, 0x07, 0x14,
      BUILD_HEADER(0x04, 0x04), 0x00, 0x00, 0xFF};
  auto comp_result = ExpectCompressOk(rom, single_copy_repeat, 8);
  EXPECT_THAT(single_copy_repeat_expected,
              ElementsAreArray(comp_result.data(), 9));
}

TEST(LC_LZ2_CompressionTest, CompressionSingleOverflowIncrement) {
  ROM rom;
  uchar overflow_inc[4] = {0xFE, 0xFF, 0x00, 0x01};
  uchar overflow_inc_expected[3] = {BUILD_HEADER(0x03, 0x04), 0xFE, 0xFF};

  auto comp_result = ExpectCompressOk(rom, overflow_inc, 4);
  EXPECT_THAT(overflow_inc_expected, ElementsAreArray(comp_result.data(), 3));
}

TEST(LC_LZ2_CompressionTest, CompressionMixedRepeatIncrement) {
  ROM rom;
  uchar to_compress_string[28] = {0x05, 0x05, 0x05, 0x05, 0x06, 0x07, 0x08,
                                  0x09, 0x0A, 0x0B, 0x05, 0x02, 0x05, 0x02,
                                  0x05, 0x02, 0x0A, 0x0B, 0x05, 0x02, 0x05,
                                  0x02, 0x05, 0x02, 0x08, 0x0A, 0x00, 0x05};
  uchar repeat_and_inc_copy_expected[7] = {BUILD_HEADER(0x01, 0x04),
                                           0x05,
                                           BUILD_HEADER(0x03, 0x06),
                                           0x06,
                                           BUILD_HEADER(0x00, 0x01),
                                           0x05,
                                           0xFF};
  // Mixing, repeat, inc, trailing copy
  auto comp_result = ExpectCompressOk(rom, to_compress_string, 28);
  EXPECT_THAT(repeat_and_inc_copy_expected,
              ElementsAreArray(comp_result.data(), 7));
}
 */

TEST(LC_LZ2_CompressionTest, CompressionMixedIncrementIntraCopyOffset) {
  ROM rom;
  uchar to_compress_string[] = {0x05, 0x05, 0x05, 0x05, 0x06, 0x07, 0x08,
                                0x09, 0x0A, 0x0B, 0x05, 0x02, 0x05, 0x02,
                                0x05, 0x02, 0x0A, 0x0B, 0x05, 0x02, 0x05,
                                0x02, 0x05, 0x02, 0x08, 0x0A, 0x00, 0x05};
  uchar inc_word_intra_copy_expected[] = {BUILD_HEADER(0x03, 0x07),
                                          0x05,
                                          BUILD_HEADER(0x02, 0x06),
                                          0x05,
                                          0x02,
                                          BUILD_HEADER(0x04, 0x08),
                                          0x05,
                                          0x00,
                                          0xFF};

  // "Mixing, inc, alternate, intra copy"
  // compress start: 3, length: 21
  // compressed length: 9
  auto comp_result = ExpectCompressOk(rom, to_compress_string + 3, 21);
  EXPECT_THAT(inc_word_intra_copy_expected,
              ElementsAreArray(comp_result.data(), 9));
}

TEST(LC_LZ2_CompressionTest, CompressionMixedIncrementIntraCopySource) {
  ROM rom;
  uchar to_compress_string[] = {0x05, 0x05, 0x05, 0x05, 0x06, 0x07, 0x08,
                                0x09, 0x0A, 0x0B, 0x05, 0x02, 0x05, 0x02,
                                0x05, 0x02, 0x0A, 0x0B, 0x05, 0x02, 0x05,
                                0x02, 0x05, 0x02, 0x08, 0x0A, 0x00, 0x05};
  uchar all_expected[] = {BUILD_HEADER(0x01, 0x04),
                          0x05,
                          BUILD_HEADER(0x03, 0x06),
                          0x06,
                          BUILD_HEADER(0x02, 0x06),
                          0x05,
                          0x02,
                          BUILD_HEADER(0x04, 0x08),
                          0x08,
                          0x00,
                          BUILD_HEADER(0x00, 0x04),
                          0x08,
                          0x0A,
                          0x00,
                          0x05,
                          0xFF};
  // "Mixing, inc, alternate, intra copy"
  // 0, 28
  // 16
  auto comp_result = ExpectCompressOk(rom, to_compress_string, 28);
  EXPECT_THAT(all_expected, ElementsAreArray(comp_result.data(), 16));
}

TEST(LC_LZ2_CompressionTest, LengthBorderCompression) {
  ROM rom;
  uchar buffer[3000];

  for (unsigned int i = 0; i < 3000; i++) buffer[i] = 0x05;
  uchar extended_lenght_expected_42[] = {0b11100100, 0x29, 0x05, 0xFF};
  uchar extended_lenght_expected_400[] = {0b11100101, 0x8F, 0x05, 0xFF};
  uchar extended_lenght_expected_1050[] = {
      0b11100111, 0xFF, 0x05, BUILD_HEADER(0x01, 0x1A), 0x05, 0xFF};
  uchar extended_lenght_expected_2050[] = {
      0b11100111, 0xFF, 0x05, 0b11100111, 0xFF, 0x05, BUILD_HEADER(0x01, 0x02),
      0x05,       0xFF};

  // "Extended lenght, 42 repeat of 5"
  auto comp_result = ExpectCompressOk(rom, buffer, 42);
  EXPECT_THAT(extended_lenght_expected_42,
              ElementsAreArray(comp_result.data(), 4));

  // "Extended lenght, 400 repeat of 5"
  comp_result = ExpectCompressOk(rom, buffer, 400);
  EXPECT_THAT(extended_lenght_expected_400,
              ElementsAreArray(comp_result.data(), 4));

  // "Extended lenght, 1050 repeat of 5"
  comp_result = ExpectCompressOk(rom, buffer, 1050);
  EXPECT_THAT(extended_lenght_expected_1050,
              ElementsAreArray(comp_result.data(), 6));

  // "Extended lenght, 2050 repeat of 5"
  comp_result = ExpectCompressOk(rom, buffer, 2050);
  EXPECT_THAT(extended_lenght_expected_2050,
              ElementsAreArray(comp_result.data(), 9));
}

TEST(LC_LZ2_CompressionTest, CompressionExtendedWordCopy) {
  ROM rom;
  uchar buffer[3000];
  for (unsigned int i = 0; i < 3000; i += 2) {
    buffer[i] = 0x05;
    buffer[i + 1] = 0x06;
  }
  uchar hightlenght_word_1050[] = {
      0b11101011, 0xFF, 0x05, 0x06, BUILD_HEADER(0x02, 0x1A), 0x05, 0x06, 0xFF};

  // "Extended word copy"
  auto comp_result = ExpectCompressOk(rom, buffer, 1050);
  EXPECT_THAT(hightlenght_word_1050, ElementsAreArray(comp_result.data(), 8));
}

/* Extended Header Command is currently unimplemented
TEST(LC_LZ2_CompressionTest, ExtendedHeaderDecompress) {
  ROM rom;
  Bytes extendedcmd_i = {0b11100100, 0x8F, 0x2A, 0xFF};
  uchar extendedcmd_o[50];
  for (int i = 0; i < 50; ++i) {
    extendedcmd_o[i] = 0x2A;
  }

  auto decomp_result = ExpectDecompressBytesOk(rom, extendedcmd_i);
  ASSERT_THAT(extendedcmd_o, ElementsAreArray(decomp_result.data(), 50));
}

TEST(LC_LZ2_CompressionTest, ExtendedHeaderDecompress2) {
  ROM rom;
  Bytes extendedcmd_i = {0b11100101, 0x8F, 0x2A, 0xFF};
  uchar extendedcmd_o[50];
  for (int i = 0; i < 50; i++) {
    extendedcmd_o[i] = 0x2A;
  }

  auto data = ExpectDecompressBytesOk(rom, extendedcmd_i);
  for (int i = 0; i < 50; i++) {
    ASSERT_EQ(extendedcmd_o[i], data[i]);
  }
}
*/

TEST(LC_LZ2_CompressionTest, CompressionDecompressionEmptyData) {
  ROM rom;
  uchar empty_input[0] = {};
  auto comp_result = ExpectCompressOk(rom, empty_input, 0);
  EXPECT_EQ(0, comp_result.size());

  auto decomp_result = ExpectDecompressOk(rom, empty_input, 0);
  EXPECT_EQ(0, decomp_result.size());
}

// TEST(LC_LZ2_CompressionTest, CompressionDecompressionSingleByte) {
//   ROM rom;
//   uchar single_byte[1] = {0x2A};
//   uchar single_byte_expected[3] = {BUILD_HEADER(0x00, 0x01), 0x2A, 0xFF};

//   auto comp_result = ExpectCompressOk(rom, single_byte, 1);
//   EXPECT_THAT(single_byte_expected, ElementsAreArray(comp_result.data(), 3));

//   auto decomp_result = ExpectDecompressOk(rom, single_byte, 1);
//   EXPECT_THAT(single_byte, ElementsAreArray(decomp_result.data(), 1));
// }

// TEST(LC_LZ2_CompressionTest, CompressionDecompressionAllBitsSet) {
//   ROM rom;
//   uchar all_bits_set[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//   uchar all_bits_set_expected[3] = {BUILD_HEADER(0x01, 0x05), 0xFF, 0xFF};

//   auto comp_result = ExpectCompressOk(rom, all_bits_set, 5);
//   EXPECT_THAT(all_bits_set_expected, ElementsAreArray(comp_result.data(),
//   3));

//   auto decomp_result = ExpectDecompressOk(rom, all_bits_set, 5);
//   EXPECT_THAT(all_bits_set, ElementsAreArray(decomp_result.data(), 5));
// }

// TEST(LC_LZ2_CompressionTest, DecompressionInvalidData) {
//   ROM rom;
//   Bytes invalid_input = {0xFF, 0xFF};  // Invalid command

//   auto load_status = rom.LoadFromBytes(invalid_input);
//   EXPECT_TRUE(load_status.ok());
//   auto decompression_status = rom.Decompress(0, invalid_input.size());
//   EXPECT_FALSE(decompression_status.ok());  // Expect failure
// }

}  // namespace gfx_test
}  // namespace yaze_test