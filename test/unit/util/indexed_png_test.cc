#include "util/indexed_png.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "miniz/miniz.h"

namespace yaze::util {
namespace {

void PutU32(std::vector<uint8_t>& out, uint32_t value) {
  for (int shift : {24, 16, 8, 0}) {
    out.push_back(static_cast<uint8_t>(value >> shift));
  }
}

void PutChunk(std::vector<uint8_t>& out, const char type[4],
              const std::vector<uint8_t>& data) {
  PutU32(out, static_cast<uint32_t>(data.size()));
  const size_t start = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data.begin(), data.end());
  PutU32(out, static_cast<uint32_t>(mz_crc32(MZ_CRC32_INIT, out.data() + start,
                                             data.size() + 4)));
}

// A PNG built by hand from already-filtered scanlines, so tests cover
// encoders other than ours (bit depths, filters, color types).
std::vector<uint8_t> BuildPng(int width, int height, int bit_depth,
                              int color_type,
                              const std::vector<uint8_t>& filtered_rows,
                              const std::vector<uint8_t>& plte = {}) {
  std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  std::vector<uint8_t> header;
  PutU32(header, width);
  PutU32(header, height);
  header.insert(header.end(), {static_cast<uint8_t>(bit_depth),
                               static_cast<uint8_t>(color_type), 0, 0, 0});
  PutChunk(out, "IHDR", header);
  if (!plte.empty()) {
    PutChunk(out, "PLTE", plte);
  }
  mz_ulong size = mz_compressBound(filtered_rows.size());
  std::vector<uint8_t> idat(size);
  EXPECT_EQ(mz_compress(idat.data(), &size, filtered_rows.data(),
                        filtered_rows.size()),
            MZ_OK);
  idat.resize(size);
  PutChunk(out, "IDAT", idat);
  PutChunk(out, "IEND", {});
  return out;
}

TEST(IndexedPngTest, EncodeDecodeRoundTripsIndicesAndPalette) {
  std::vector<uint8_t> indices(20 * 3);
  for (size_t i = 0; i < indices.size(); ++i) {
    indices[i] = static_cast<uint8_t>(i % 8);
  }
  std::vector<std::array<uint8_t, 4>> palette;
  for (int i = 0; i < 8; ++i) {
    palette.push_back({static_cast<uint8_t>(i * 30), 7, 9,
                       static_cast<uint8_t>(i == 0 ? 0 : 255)});
  }
  auto png = EncodeIndexedPng(20, 3, indices, palette);
  ASSERT_TRUE(png.ok()) << png.status();
  auto image = DecodePng(*png);
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_TRUE(image->indexed);
  EXPECT_EQ(image->width, 20);
  EXPECT_EQ(image->height, 3);
  EXPECT_EQ(image->indices, indices);
  EXPECT_EQ(image->palette, palette);  // alpha comes back through tRNS
}

TEST(IndexedPngTest, DecodesPacked4BitIndexedRows) {
  // 3x1 pixels: indices 1, 2, 3 packed as 0x12 0x30.
  const auto png = BuildPng(3, 1, 4, 3, {0, 0x12, 0x30},
                            {0, 0, 0, 10, 10, 10, 20, 20, 20, 30, 30, 30});
  auto image = DecodePng(png);
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_EQ(image->indices, (std::vector<uint8_t>{1, 2, 3}));
}

TEST(IndexedPngTest, UndoesEveryRowFilterForRgba) {
  // 2x5 RGBA, one row per filter type (0 none, 1 sub, 2 up, 3 avg, 4 paeth).
  // Every decoded pixel should be (10, 20, 30, 255).
  std::vector<uint8_t> rows = {
      0, 10, 20, 30, 255, 10, 20, 30, 255,  // none
      1, 10, 20, 30, 255, 0,  0,  0,  0,    // sub: second = first + 0
      2, 0,  0,  0,  0,   0,  0,  0,  0,    // up: equal to the row above
      3, 5,  10, 15, 128, 0,  0,  0,  0,    // avg: (0 + above) / 2 = half
      4, 0,  0,  0,  0,   0,  0,  0,  0,    // paeth: predictor is exact
  };
  // Avg row: pixel 0 = 5 + (0 + 10) / 2 = 10; pixel 1 = 0 + (10 + 10) / 2.
  auto image = DecodePng(BuildPng(2, 5, 8, 6, rows));
  ASSERT_TRUE(image.ok()) << image.status();
  ASSERT_FALSE(image->indexed);
  for (size_t i = 0; i < image->rgba.size(); i += 4) {
    SCOPED_TRACE(i / 4);
    EXPECT_EQ(image->rgba[i], 10);
    EXPECT_EQ(image->rgba[i + 1], 20);
    EXPECT_EQ(image->rgba[i + 2], 30);
    EXPECT_EQ(image->rgba[i + 3], 255);
  }
}

TEST(IndexedPngTest, RejectsBrokenFiles) {
  auto png =
      EncodeIndexedPng(2, 2, {0, 1, 1, 0}, {{0, 0, 0, 255}, {9, 9, 9, 255}});
  ASSERT_TRUE(png.ok());
  auto corrupt = *png;
  corrupt[40] ^= 0xFF;  // inside a chunk: CRC fails
  EXPECT_FALSE(DecodePng(corrupt).ok());
  EXPECT_FALSE(DecodePng({1, 2, 3}).ok());
  auto truncated = *png;
  truncated.resize(truncated.size() - 20);
  EXPECT_FALSE(DecodePng(truncated).ok());
  // 16-bit RGB is refused rather than misread.
  EXPECT_FALSE(DecodePng(BuildPng(1, 1, 16, 2, {0, 0, 0, 0, 0, 0, 0})).ok());
  // Pixel index past the palette cannot be encoded.
  EXPECT_FALSE(EncodeIndexedPng(1, 1, {2}, {{0, 0, 0, 255}}).ok());
}

}  // namespace
}  // namespace yaze::util
