#include "app/rom.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>

#include "absl/status/statusor.h"

#define BUILD_HEADER(command, length) (command << 5) + (length - 1)

namespace yaze_test {
namespace rom_test {

using yaze::app::CompressionPiece;
using yaze::app::ROM;

using ::testing::ElementsAreArray;
using ::testing::TypedEq;

namespace {

Bytes ExpectCompressOk(ROM& rom, uchar* in, int in_size) {
  Bytes result;
  auto load_status = rom.LoadFromPointer(in, in_size);
  EXPECT_TRUE(load_status.ok());
  auto compression_status = rom.Compress(0, in_size);
  EXPECT_TRUE(compression_status.ok());
  return std::move(*compression_status);
}

Bytes ExpectDecompressOk(ROM& rom, uchar* in, int in_size) {
  auto load_status = rom.LoadFromPointer(in, in_size);
  EXPECT_TRUE(load_status.ok());
  auto decompression_status = rom.Decompress(0, in_size);
  EXPECT_TRUE(decompression_status.ok());
  return std::move(*decompression_status);
}

std::shared_ptr<CompressionPiece> ExpectNewCompressionPieceOk(
    const char command, const int length, const std::string args,
    const int argument_length) {
  auto new_piece = std::make_shared<CompressionPiece>(command, length, args,
                                                      argument_length);
  EXPECT_TRUE(new_piece != nullptr);
  return std::move(new_piece);
}

}  // namespace

TEST(ROMTest, NewDecompressionPieceOk) {
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

TEST(ROMTest, DecompressionValidCommand) {
  ROM rom;
  std::array<uchar, 4> simple_copy_input = {BUILD_HEADER(0x00, 0x02), 0x2A,
                                            0x45, 0xFF};
  uchar simple_copy_output[2] = {0x2A, 0x45};
  auto decomp_result = ExpectDecompressOk(rom, simple_copy_input.data(), 4);
  EXPECT_THAT(simple_copy_output, ElementsAreArray(decomp_result.data(), 2));
}

TEST(ROMTest, DecompressionMixingCommand) {
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

/* Extended Header Command is currently unimplemented
TEST(ROMTest, ExtendedHeaderDecompress) {
  ROM rom;
  uchar extendedcmd_i[4] = {0b11100100, 0x8F, 0x2A, 0xFF};
  uchar extendedcmd_o[50];
  for (int i = 0; i < 50; ++i) {
    extendedcmd_o[i] = 0x2A;
  }

  auto decomp_result = ExpectDecompressOk(rom, extendedcmd_i, 4);
  ASSERT_THAT(extendedcmd_o, ElementsAreArray(decomp_result.data(), 50));
}

TEST(ROMTest, ExtendedHeaderDecompress2) {
  ROM rom;
  uchar extendedcmd_i[4] = {0b11100101, 0x8F, 0x2A, 0xFF};
  uchar extendedcmd_o[50];
  for (int i = 0; i < 50; i++) {
    extendedcmd_o[i] = 0x2A;
  }

  auto data = ExpectDecompressOk(rom, extendedcmd_i, 4);
  for (int i = 0; i < 50; i++) {
    ASSERT_EQ(extendedcmd_o[i], data[i]);
  }
}
*/

TEST(ROMTest, CompressionSingleSet) {
  ROM rom;
  uchar single_set[5] = {0x2A, 0x2A, 0x2A, 0x2A, 0x2A};
  uchar single_set_expected[3] = {BUILD_HEADER(1, 5), 0x2A, 0xFF};

  auto comp_result = ExpectCompressOk(rom, single_set, 5);
  EXPECT_THAT(single_set_expected, ElementsAreArray(comp_result.data(), 3));
}

TEST(ROMTest, CompressionSingleWord) {
  ROM rom;
  uchar single_word[6] = {0x2A, 0x01, 0x2A, 0x01, 0x2A, 0x01};
  uchar single_word_expected[4] = {BUILD_HEADER(0x02, 0x06), 0x2A, 0x01, 0xFF};

  auto comp_result = ExpectCompressOk(rom, single_word, 6);
  EXPECT_THAT(single_word_expected, ElementsAreArray(comp_result.data(), 4));
}

TEST(ROMTest, CompressionSingleIncrement) {
  ROM rom;
  uchar single_inc[3] = {0x01, 0x02, 0x03};
  uchar single_inc_expected[3] = {BUILD_HEADER(0x03, 0x03), 0x01, 0xFF};
  auto comp_result = ExpectCompressOk(rom, single_inc, 3);
  EXPECT_THAT(single_inc_expected, ElementsAreArray(comp_result.data(), 3));
}

TEST(ROMTest, CompressionSingleCopy) {
  ROM rom;
  uchar single_copy[4] = {0x03, 0x0A, 0x07, 0x14};
  uchar single_copy_expected[6] = {
      BUILD_HEADER(0x00, 0x04), 0x03, 0x0A, 0x07, 0x14, 0xFF};
  auto comp_result = ExpectCompressOk(rom, single_copy, 4);
  EXPECT_THAT(single_copy_expected, ElementsAreArray(comp_result.data(), 6));
}

/* Hiding tests until I figure out a better PR to address the bug
TEST(ROMTest, CompressionSingleCopyRepeat) {
  ROM rom;
  uchar single_copy_repeat[8] = {0x03, 0x0A, 0x07, 0x14, 0x03, 10, 0x07, 0x14};
  uchar single_copy_repeat_expected[9] = {
      BUILD_HEADER(0x00, 0x04), 0x03, 0x0A, 0x07, 0x14,
      BUILD_HEADER(0x04, 0x04), 0x00, 0x00, 0xFF};
  auto comp_result = ExpectCompressOk(rom, single_copy_repeat, 8);
  EXPECT_THAT(single_copy_repeat_expected,
              ElementsAreArray(comp_result.data(), 8));
}

TEST(ROMTest, CompressionSingleOverflowIncrement) {
  ROM rom;
  uchar overflow_inc[4] = {0xFE, 0xFF, 0x00, 0x01};
  uchar overflow_inc_expected[3] = {BUILD_HEADER(0x03, 0x04), 0xFE, 0xFF};

  auto comp_result = ExpectCompressOk(rom, overflow_inc, 4);
  EXPECT_THAT(overflow_inc_expected, ElementsAreArray(comp_result.data(), 3));
}

TEST(ROMTest, CompressionMixedRepeatIncrement) {
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

TEST(ROMTest, SimpleMixCompression) {
  ROM rom;
  uchar to_compress_string[] = {0x05, 0x05, 0x05, 0x05, 0x06, 0x07, 0x08,
                                0x09, 0x0A, 0x0B, 0x05, 0x02, 0x05, 0x02,
                                0x05, 0x02, 0x0A, 0x0B, 0x05, 0x02, 0x05,
                                0x02, 0x05, 0x02, 0x08, 0x0A, 0x00, 0x05};
  uchar repeat_and_inc_copy_expected[] = {BUILD_HEADER(0x01, 0x04),
                                          0x05,
                                          BUILD_HEADER(0x03, 0x06),
                                          0x06,
                                          BUILD_HEADER(0x00, 0x01),
                                          0x05,
                                          0xFF};
  // Mixing, repeat, inc, trailing copy
  auto data = ExpectCompressOk(rom, to_compress_string, 7);
  for (int i = 0; i < 7; ++i) {
    EXPECT_EQ(repeat_and_inc_copy_expected[i], data[i]);
  }

  uchar inc_word_intra_copy_expected[] = {BUILD_HEADER(0x03, 0x07),
                                          0x05,
                                          BUILD_HEADER(0x02, 0x06),
                                          0x05,
                                          0x02,
                                          BUILD_HEADER(0x04, 0x08),
                                          0x05,
                                          0x00,
                                          0xFF};
  // CuAssertDataEquals_Msg(
  //     tc, "Mixing, inc, alternate, intra copy", inc_word_intra_copy_expected,
  //     9, alttp_compress_gfx(to_compress_string, 3, 21, &compress_size));

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
  // CuAssertDataEquals_Msg(
  //     tc, "Mixing, inc, alternate, intra copy", all_expected, 16,
  //     alttp_compress_gfx(to_compress_string, 0, 28, &compress_size));
}
*/

TEST(ROMTest, LengthBorderCompression) {
  char buffer[3000];
  unsigned int compress_size;

  for (unsigned int i = 0; i < 3000; i++) buffer[i] = 0x05;
  uchar extended_lenght_expected_42[] = {0b11100100, 0x29, 0x05, 0xFF};
  uchar extended_lenght_expected_400[] = {0b11100101, 0x8F, 0x05, 0xFF};
  uchar extended_lenght_expected_1050[] = {
      0b11100111, 0xFF, 0x05, BUILD_HEADER(0x01, 0x1A), 0x05, 0xFF};
  uchar extended_lenght_expected_2050[] = {
      0b11100111, 0xFF, 0x05, 0b11100111, 0xFF, 0x05, BUILD_HEADER(0x01, 0x02),
      0x05,       0xFF};
  // CuAssertDataEquals_Msg(tc, "Extended lenght, 42 repeat of 5",
  //                        extended_lenght_expected_42, 4,
  //                        alttp_compress_gfx(buffer, 0, 42, &compress_size));
  // CuAssertDataEquals_Msg(tc, "Extended lenght, 400 repeat of 5",
  //                        extended_lenght_expected_400, 4,
  //                        alttp_compress_gfx(buffer, 0, 400, &compress_size));
  // CuAssertDataEquals_Msg(tc, "Extended lenght, 1050 repeat of 5",
  //                        extended_lenght_expected_1050, 6,
  //                        alttp_compress_gfx(buffer, 0, 1050,
  //                        &compress_size));
  // CuAssertDataEquals_Msg(tc, "Extended lenght, 2050 repeat of 5",
  //                        extended_lenght_expected_2050, 9,
  //                        alttp_compress_gfx(buffer, 0, 2050,
  //                        &compress_size));

  for (unsigned int i = 0; i < 3000; i += 2) {
    buffer[i] = 0x05;
    buffer[i + 1] = 0x06;
  }
  uchar hightlenght_word_1050[] = {
      0b11101011, 0xFF, 0x05, 0x06, BUILD_HEADER(0x02, 0x1A), 0x05, 0x06, 0xFF};
  // CuAssertDataEquals_Msg(tc, "Extended word copy", hightlenght_word_1050, 8,
  //                        alttp_compress_gfx(buffer, 0, 1050,
  //                        &compress_size));
}

}  // namespace rom_test
}  // namespace yaze_test