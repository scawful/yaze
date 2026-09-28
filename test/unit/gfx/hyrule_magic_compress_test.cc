#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

#include "app/gfx/util/compression.h"
#include "rom/rom.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::test {
namespace {

using gfx::lc_lz2::DecompressExact;

std::vector<uint8_t> Compress(const std::vector<uint8_t>& input, int flag) {
  int size = 0;
  auto encoded = gfx::HyruleMagicCompress(
      input.data(), static_cast<int>(input.size()), &size, flag);
  encoded.resize(size);
  return encoded;
}

void ExpectRoundTrip(const std::vector<uint8_t>& input, int flag = 0) {
  const auto encoded = Compress(input, flag);
  auto decoded = DecompressExact(encoded.data(), encoded.size(), 0,
                                 input.size(), /*big_endian_copy=*/flag != 0);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_EQ(decoded->data, input);
  EXPECT_EQ(decoded->compressed_size, encoded.size());
}

std::vector<uint8_t> RandomBytes(std::mt19937& rng, size_t size) {
  std::vector<uint8_t> data(size);
  for (auto& byte : data) {
    byte = static_cast<uint8_t>(rng());
  }
  return data;
}

TEST(HyruleMagicCompressTest, EmptySheetRoundTrips) {
  // A 0x600-byte zero run is longer than one command can encode.
  ExpectRoundTrip(std::vector<uint8_t>(zelda3::kGfxSheet3bppBytes, 0x00));
  ExpectRoundTrip(std::vector<uint8_t>(zelda3::kGfxSheet2bppBytes, 0xFF));
}

TEST(HyruleMagicCompressTest, LongRunsSplitAtCommandLimit) {
  for (size_t size : {1024u, 1025u, 2048u, 3000u}) {
    ExpectRoundTrip(std::vector<uint8_t>(size, 0x5A));
    std::vector<uint8_t> words(size);
    for (size_t i = 0; i < size; ++i) {
      words[i] = (i & 1) ? 0x34 : 0x12;
    }
    ExpectRoundTrip(words);
  }
}

TEST(HyruleMagicCompressTest, LongLiteralBlocksSplitAtCommandLimit) {
  std::mt19937 rng(1234);
  for (size_t size : {1023u, 1024u, 1025u, 3000u}) {
    ExpectRoundTrip(RandomBytes(rng, size));
  }
}

TEST(HyruleMagicCompressTest, LongMatchesSplitAtCommandLimit) {
  std::mt19937 rng(99);
  auto block = RandomBytes(rng, 1100);
  std::vector<uint8_t> input = block;
  input.insert(input.end(), block.begin(), block.end());
  ExpectRoundTrip(input);
}

TEST(HyruleMagicCompressTest, InputEdgesStayInBounds) {
  const std::vector<std::vector<uint8_t>> inputs = {
      {0x01},
      {0x01, 0x02},
      {0x07, 0x07},
      {0x01, 0x02, 0x01},
      {0x09, 0x08, 0x09, 0x08, 0x09},
      {0x00, 0x11, 0x22, 0x11, 0x22, 0x11},
      {0x10, 0x11, 0x12, 0x13},
  };
  for (const auto& input : inputs) {
    SCOPED_TRACE(input.size());
    ExpectRoundTrip(input);
    ExpectRoundTrip(input, /*flag=*/1);
  }
}

TEST(HyruleMagicCompressTest, RandomSheetsRoundTripInBothAddressOrders) {
  std::mt19937 rng(20260925);
  for (int iteration = 0; iteration < 150; ++iteration) {
    std::vector<uint8_t> input;
    while (input.size() < zelda3::kGfxSheet3bppBytes) {
      const size_t length = 1 + rng() % 90;
      switch (rng() % 5) {
        case 0:
          input.insert(input.end(), length, static_cast<uint8_t>(rng()));
          break;
        case 1:
          for (size_t i = 0; i < length; ++i) {
            input.push_back(static_cast<uint8_t>((i & 1) ? 0xA5 : rng() & 3));
          }
          break;
        case 2: {
          const uint8_t start = static_cast<uint8_t>(rng());
          for (size_t i = 0; i < length; ++i) {
            input.push_back(static_cast<uint8_t>(start + i));
          }
          break;
        }
        case 3:
          if (input.size() > 8) {
            const size_t from = rng() % (input.size() - 4);
            for (size_t i = 0; i < length; ++i) {
              input.push_back(input[from + i]);
            }
            break;
          }
          [[fallthrough]];
        default: {
          auto noise = RandomBytes(rng, length);
          input.insert(input.end(), noise.begin(), noise.end());
          break;
        }
      }
    }
    input.resize(zelda3::kGfxSheet3bppBytes);
    SCOPED_TRACE(iteration);
    ExpectRoundTrip(input, /*flag=*/0);
    ExpectRoundTrip(input, /*flag=*/1);
  }
}

TEST(HyruleMagicCompressTest, GraphicsFlagWritesLowAddressByteFirst) {
  // Random bytes, then a repeat of bytes [0x102, 0x142): the only good match
  // has source offset 0x0102, whose two bytes differ.
  std::mt19937 rng(7);
  auto input = RandomBytes(rng, 0x200);
  input.insert(input.end(), input.begin() + 0x102, input.begin() + 0x142);

  const auto little = Compress(input, /*flag=*/0);
  auto as_little = DecompressExact(little.data(), little.size(), 0,
                                   input.size(), /*big_endian_copy=*/false);
  ASSERT_TRUE(as_little.ok()) << as_little.status();
  EXPECT_EQ(as_little->data, input);
  auto as_big = DecompressExact(little.data(), little.size(), 0, input.size(),
                                /*big_endian_copy=*/true);
  EXPECT_TRUE(!as_big.ok() || as_big->data != input);

  const auto big = Compress(input, /*flag=*/1);
  EXPECT_NE(little, big);
  auto big_decoded = DecompressExact(big.data(), big.size(), 0, input.size(),
                                     /*big_endian_copy=*/true);
  ASSERT_TRUE(big_decoded.ok()) << big_decoded.status();
  EXPECT_EQ(big_decoded->data, input);
}

TEST(DecompressExactTest, ReportsCompressedSizeAndIgnoresTrailingBytes) {
  // Literal "AB", byte fill 3 x 0x07, terminator, then unrelated bytes.
  const std::vector<uint8_t> stream = {0x01, 'A',  'B',  0x22,
                                       0x07, 0xFF, 0x12, 0x34};
  auto decoded = DecompressExact(stream.data(), stream.size(), 0, 16, false);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_EQ(decoded->data, (std::vector<uint8_t>{'A', 'B', 0x07, 0x07, 0x07}));
  EXPECT_EQ(decoded->compressed_size, 6u);
}

TEST(DecompressExactTest, OverlappingCopyRepeatsBytesLikeHardware) {
  // Literal 'A', then copy 5 bytes from output offset 0.
  const std::vector<uint8_t> stream = {0x00, 'A', 0x84, 0x00, 0x00, 0xFF};
  auto decoded = DecompressExact(stream.data(), stream.size(), 0, 16, false);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_EQ(decoded->data, std::vector<uint8_t>(6, 'A'));
}

TEST(DecompressV2Test, OverlappingCopyRepeatsBytesLikeHardware) {
  // Literal 'A', then copy 5 bytes from output offset 0 (graphics mode).
  const std::vector<uint8_t> stream = {0x00, 'A', 0x84, 0x00, 0x00, 0xFF};
  auto decoded =
      gfx::lc_lz2::DecompressV2(stream.data(), 0, 0x600, 1, stream.size());
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  ASSERT_GE(decoded->size(), 6u);
  EXPECT_EQ(std::vector<uint8_t>(decoded->begin(), decoded->begin() + 6),
            std::vector<uint8_t>(6, 'A'));
}

TEST(DecompressV2Test, ReadsRepeatedTilesWrittenByHyruleMagicCompress) {
  // Three identical non-blank tiles in a row: the compressor emits a copy
  // that overlaps its own output. The editor's loader must read it back.
  std::vector<uint8_t> sheet(0x600, 0);
  for (int tile = 0; tile < 3; ++tile) {
    for (int i = 0; i < 24; ++i) {
      sheet[tile * 24 + i] = static_cast<uint8_t>(0x11 * (i % 7) + 3);
    }
  }
  const auto stream = Compress(sheet, /*flag=*/0);
  auto exact = DecompressExact(stream.data(), stream.size(), 0, 0x600, false);
  ASSERT_TRUE(exact.ok()) << exact.status();
  EXPECT_EQ(exact->data, sheet);
  auto decoded =
      gfx::lc_lz2::DecompressV2(stream.data(), 0, 0x600, 1, stream.size());
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  ASSERT_GE(decoded->size(), sheet.size());
  EXPECT_EQ(
      std::vector<uint8_t>(decoded->begin(), decoded->begin() + sheet.size()),
      sheet);
}

TEST(DecompressV2Test, CopySourceIsCheckedAgainstTheOutputNotTheStream) {
  // 'A' x 32 by fill, then copy 32 bytes from output 0x10: the source is past
  // the stream offset (4) but already written, so the stream is valid.
  const std::vector<uint8_t> valid = {0x3F, 'A', 0x9F, 0x10, 0x00, 0xFF};
  auto decoded =
      gfx::lc_lz2::DecompressV2(valid.data(), 0, 0x600, 1, valid.size());
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_EQ(std::vector<uint8_t>(decoded->begin(), decoded->begin() + 64),
            std::vector<uint8_t>(64, 'A'));

  // A copy from output not yet written is refused, as DecompressExact does.
  const std::vector<uint8_t> forward = {0x00, 'A', 0x81, 0x05, 0x00, 0xFF};
  EXPECT_FALSE(
      gfx::lc_lz2::DecompressV2(forward.data(), 0, 0x600, 1, forward.size())
          .ok());
}

TEST(DecompressExactTest, RejectsBadStreams) {
  const std::vector<uint8_t> truncated = {0x03, 'A', 'B'};
  EXPECT_FALSE(
      DecompressExact(truncated.data(), truncated.size(), 0, 16, false).ok());

  const std::vector<uint8_t> forward_ref = {0x00, 'A', 0x81, 0x05, 0x00, 0xFF};
  EXPECT_FALSE(
      DecompressExact(forward_ref.data(), forward_ref.size(), 0, 16, false)
          .ok());

  const std::vector<uint8_t> too_long = {0x3F, 0x00, 0xFF};  // 32 x 0x00
  EXPECT_FALSE(
      DecompressExact(too_long.data(), too_long.size(), 0, 16, false).ok());
  EXPECT_TRUE(
      DecompressExact(too_long.data(), too_long.size(), 0, 32, false).ok());

  EXPECT_FALSE(
      DecompressExact(too_long.data(), too_long.size(), 3, 32, false).ok());
}

// decompress -> compress -> decompress must be the identity for every
// compressed sheet in a real ROM. Runs for each ROM the environment names.
TEST(HyruleMagicCompressRomTest, EveryCompressedSheetRoundTrips) {
  std::vector<std::string> paths;
  for (const char* env :
       {"YAZE_TEST_ROM_OOS", "YAZE_TEST_ROM_VANILLA", "YAZE_TEST_ROM_PATH"}) {
    if (const char* path = std::getenv(env);
        path != nullptr && std::filesystem::exists(path)) {
      paths.push_back(path);
    }
  }
  if (paths.empty()) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS or YAZE_TEST_ROM_VANILLA";
  }

  for (const auto& path : paths) {
    SCOPED_TRACE(path);
    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(bytes).ok());

    int compressed_sheets = 0;
    for (uint16_t sheet = 0; sheet < zelda3::kGfxSheetCount; ++sheet) {
      if (zelda3::GetGfxSheetStorageKind(sheet) ==
          zelda3::GfxSheetStorageKind::kRaw3bpp) {
        continue;
      }
      SCOPED_TRACE(sheet);
      auto original = zelda3::ReadGfxSheetData(rom, sheet);
      ASSERT_TRUE(original.ok()) << original.status();
      ExpectRoundTrip(*original);
      // The editor's loader (DecompressV2) must agree with the game's
      // decoder on the ROM's own stream and on the re-encoded one.
      auto pc = zelda3::ReadGfxSheetPc(rom, sheet);
      ASSERT_TRUE(pc.ok()) << pc.status();
      auto loaded = gfx::lc_lz2::DecompressV2(rom.data(), static_cast<int>(*pc),
                                              0x800, 1, rom.size());
      ASSERT_TRUE(loaded.ok()) << loaded.status();
      ASSERT_GE(loaded->size(), original->size());
      EXPECT_TRUE(
          std::equal(original->begin(), original->end(), loaded->begin()));
      const auto encoded = Compress(*original, /*flag=*/0);
      auto reloaded = gfx::lc_lz2::DecompressV2(encoded.data(), 0, 0x800, 1,
                                                encoded.size());
      ASSERT_TRUE(reloaded.ok()) << reloaded.status();
      ASSERT_GE(reloaded->size(), original->size());
      EXPECT_TRUE(
          std::equal(original->begin(), original->end(), reloaded->begin()));
      ++compressed_sheets;
    }
    EXPECT_EQ(compressed_sheets, 211);
  }
}

}  // namespace
}  // namespace yaze::test
