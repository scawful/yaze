#include "app/rom.h"

#include <gtest/gtest.h>

#include "absl/status/statusor.h"

#define BUILD_HEADER(command, length) (command << 5) + (length - 1)

namespace yaze_test {
namespace rom_test {

namespace {

Bytes ExpectDataLoadedOk(yaze::app::ROM& rom, uchar* in, int in_size) {
  auto load_status = rom.LoadFromPointer(in, in_size);
  EXPECT_TRUE(load_status.ok());
  auto decompression_status = rom.Decompress(0, in_size);
  EXPECT_TRUE(decompression_status.ok());
  auto data = std::move(*decompression_status);
  return data;
}

}  // namespace

TEST(DecompressionTest, ValidCommandDecompress) {
  yaze::app::ROM rom;
  uchar simple_copy_input[4] = {BUILD_HEADER(0, 2), 42, 69, 0xFF};
  uchar simple_copy_output[2] = {42, 69};
  auto data = ExpectDataLoadedOk(rom, simple_copy_input, 4);
  for (int i = 0; i < 2; i++) ASSERT_EQ(simple_copy_output[i], data[i]);
}

TEST(DecompressionTest, MixingCommand) {
  yaze::app::ROM rom;
  uchar random1_i[11] = {BUILD_HEADER(1, 3),
                         42,
                         BUILD_HEADER(0, 4),
                         1,
                         2,
                         3,
                         4,
                         BUILD_HEADER(2, 2),
                         11,
                         22,
                         0xFF};
  uchar random1_o[9] = {42, 42, 42, 1, 2, 3, 4, 11, 22};
  auto data = ExpectDataLoadedOk(rom, random1_i, 11);
  for (int i = 0; i < 9; i++) {
    ASSERT_EQ(random1_o[i], data[i]) << '[' << i << ']';
  }
}

TEST(DecompressionTest, ExtendedHeaderDecompress) {
  yaze::app::ROM rom;
  uchar extendedcmd_i[4] = {0b11100100, 0x8F, 42, 0xFF};
  uchar extendedcmd_o[50];
  for (int i = 0; i < 50; ++i) {
    extendedcmd_o[i] = 42;
  }

  auto data = ExpectDataLoadedOk(rom, extendedcmd_i, 4);
  for (int i = 0; i < 50; ++i) {
    ASSERT_EQ(extendedcmd_o[i], data[i]);
  }
}

TEST(DecompressionTest, ExtendedHeaderDecompress2) {
  yaze::app::ROM rom;
  uchar extendedcmd_i[4] = {0b11100101, 0x8F, 42, 0xFF};
  uchar extendedcmd_o[50];
  for (int i = 0; i < 50; i++) {
    extendedcmd_o[i] = 42;
  }

  auto data = ExpectDataLoadedOk(rom, extendedcmd_i, 4);
  for (int i = 0; i < 50; i++) {
    ASSERT_EQ(extendedcmd_o[i], data[i]);
  }
}

TEST(DecompressionTest, CompressionSingleSet) {
  yaze::app::ROM rom;
  uchar single_set[5] = {42, 42, 42, 42, 42};
  uchar single_set_expected[3] = {BUILD_HEADER(1, 5), 42, 0xFF};

  auto data = ExpectDataLoadedOk(rom, single_set, 5);
  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ(single_set_expected[i], data[i]);
  }
}

TEST(DecompressionTest, CompressionSingleWord) {
  yaze::app::ROM rom;
  uchar single_word[6] = {42, 1, 42, 1, 42, 1};
  uchar single_word_expected[4] = {BUILD_HEADER(2, 6), 42, 1, 0xFF};

  auto data = ExpectDataLoadedOk(rom, single_word, 6);
  for (int i = 0; i < 4; i++) {
    ASSERT_EQ(single_word_expected[i], data[i]);
  }
}

TEST(DecompressionTest, CompressionSingleIncrement) {
  yaze::app::ROM rom;
  uchar single_inc[3] = {1, 2, 3};
  uchar single_inc_expected[3] = {BUILD_HEADER(3, 3), 1, 0xFF};
  auto data = ExpectDataLoadedOk(rom, single_inc, 3);
  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ(single_inc_expected[i], data[i]);
  }
}

TEST(DecompressionTest, CompressionSingleCopy) {
  uchar single_copy[4] = {3, 10, 7, 20};
  uchar single_copy_expected[6] = {BUILD_HEADER(0, 4), 3, 10, 7, 20, 0xFF};
  // CuAssertDataEquals_Msg(tc, "Single compression, direct copy",
  //                        single_copy_expected, 6,
  //                        alttp_compress_gfx(single_copy, 0, 4,
  //                        &compress_size));
}

TEST(DecompressionTest, CompressionSingleCopyRepeat) {
  uchar single_copy_repeat[8] = {3, 10, 7, 20, 3, 10, 7, 20};
  uchar single_copy_repeat_expected[9] = {BUILD_HEADER(0, 4), 3, 10, 7,   20,
                                          BUILD_HEADER(4, 4), 0, 0,  0xFF};
  // CuAssertDataEquals_Msg(
  //     tc, "Single compression, direct copy", single_copy_repeat_expected, 9,
  //     alttp_compress_gfx(single_copy_repeat, 0, 8, &compress_size));
}

TEST(DecompressionTest, CompressionSingleOverflowIncrement) {
  uchar overflow_inc[4] = {0xFE, 0xFF, 0, 1};
  uchar overflow_inc_expected[3] = {BUILD_HEADER(3, 4), 0xFE, 0xFF};
  // CuAssertDataEquals_Msg(
  //     tc, "Inc overflowying", overflow_inc_expected, 3,
  //     alttp_compress_gfx(overflow_inc, 0, 4, &compress_size));
}

TEST(DecompressionTest, SimpleMixCompression) {
  // unsigned int compress_size;
  // char to_compress_string[] = {5, 5, 5,  5,  6, 7, 8, 9, 10, 11, 5, 2,  5, 2,
  //                              5, 2, 10, 11, 5, 2, 5, 2, 5,  2,  8, 10, 0,
  //                              5};

  // char repeat_and_inc_copy_expected[] = {BUILD_HEADER(1, 4),
  //                                        5,
  //                                        BUILD_HEADER(3, 6),
  //                                        6,
  //                                        BUILD_HEADER(0, 1),
  //                                        5,
  //                                        0xFF};
  // CuAssertDataEquals_Msg(
  //     tc, "Mixing, repeat, inc, trailing copy", repeat_and_inc_copy_expected,
  //     7, alttp_compress_gfx(to_compress_string, 0, 11, &compress_size));
  // char inc_word_intra_copy_expected[] = {BUILD_HEADER(3, 7),
  //                                        5,
  //                                        BUILD_HEADER(2, 6),
  //                                        5,
  //                                        2,
  //                                        BUILD_HEADER(4, 8),
  //                                        5,
  //                                        0,
  //                                        0xFF};
  // CuAssertDataEquals_Msg(
  //     tc, "Mixing, inc, alternate, intra copy", inc_word_intra_copy_expected,
  //     9, alttp_compress_gfx(to_compress_string, 3, 21, &compress_size));
  // char all_expected[] = {BUILD_HEADER(1, 4),
  //                        5,
  //                        BUILD_HEADER(3, 6),
  //                        6,
  //                        BUILD_HEADER(2, 6),
  //                        5,
  //                        2,
  //                        BUILD_HEADER(4, 8),
  //                        8,
  //                        0,
  //                        BUILD_HEADER(0, 4),
  //                        8,
  //                        10,
  //                        0,
  //                        5,
  //                        0xFF};
  // CuAssertDataEquals_Msg(
  //     tc, "Mixing, inc, alternate, intra copy", all_expected, 16,
  //     alttp_compress_gfx(to_compress_string, 0, 28, &compress_size));
}

TEST(DecompressionTest, LengthBorderCompression) {
  // char buffer[3000];
  // unsigned int compress_size;

  // for (unsigned int i = 0; i < 3000; i++) buffer[i] = 5;
  // char extended_lenght_expected_42[] = {0b11100100, 41, 5, 0xFF};
  // char extended_lenght_expected_400[] = {0b11100101, 0x8F, 5, 0xFF};
  // char extended_lenght_expected_1050[] = {0b11100111,          0xFF, 5,
  //                                         BUILD_HEADER(1, 26), 5,    0xFF};
  // char extended_lenght_expected_2050[] = {
  //     0b11100111, 0xFF, 5, 0b11100111, 0xFF, 5, BUILD_HEADER(1, 2), 5, 0xFF};
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

  // for (unsigned int i = 0; i < 3000; i += 2) {
  //   buffer[i] = 5;
  //   buffer[i + 1] = 6;
  // }
  // char hightlenght_word_1050[] = {0b11101011,          0xFF, 5, 6,
  //                                 BUILD_HEADER(2, 26), 5,    6, 0xFF};
  // CuAssertDataEquals_Msg(tc, "Extended word copy", hightlenght_word_1050, 8,
  //                        alttp_compress_gfx(buffer, 0, 1050,
  //                        &compress_size));
}

TEST(DecompressionTest, CompressDecompress) {
  // char buffer[32];
  // unsigned int compress_size;
  // int fd = open("testsnestilebpp4.tl", O_RDONLY);

  // if (fd == -1) {
  //   fprintf(stderr, "Can't open testsnestilebpp4.tl : %s\n",
  //   strerror(errno)); return;
  // }
  // read(fd, buffer, 32);
  // char* comdata = alttp_compress_gfx(buffer, 0, 32, &compress_size);
  // CuAssertDataEquals_Msg(
  //     tc, "Compressing/Uncompress testtilebpp4.tl", buffer, 32,
  //     alttp_decompress_gfx(comdata, 0, 0, &compress_size, &c_size));
}

}  // namespace rom_test
}  // namespace yaze_test