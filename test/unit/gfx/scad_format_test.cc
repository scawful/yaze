#include "app/gfx/util/scad_format.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
namespace test {
namespace {

std::string WriteTempFile(const std::string& name,
                          const std::vector<uint8_t>& bytes) {
  const std::string path =
      (std::filesystem::path(::testing::TempDir()) / name).string();
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  return path;
}

// Bpp8SnesToIndexed always converts 4096 tiles, so 2bpp data must cover them.
constexpr size_t kCgx2BppDataSize = 0x10040;

std::vector<uint8_t> ReadFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                              std::istreambuf_iterator<char>());
}

TEST(ScadFormatTest, LoadCgxSplitsDataFromTrailer) {
  std::vector<uint8_t> bytes(kCgx2BppDataSize + 0x500, 0);
  bytes[0] = 0x11;
  bytes[kCgx2BppDataSize + 0x100] = 0xAB;
  bytes.back() = 0xCD;
  const std::string path = WriteTempFile("scad_valid.cgx", bytes);

  std::vector<uint8_t> data, loaded, header;
  ASSERT_TRUE(gfx::LoadCgx(2, path, data, loaded, header).ok());
  ASSERT_EQ(data.size(), kCgx2BppDataSize);
  EXPECT_EQ(data[0], 0x11);
  ASSERT_EQ(header.size(), 0x400u);
  EXPECT_EQ(header.front(), 0xAB);
  EXPECT_EQ(header.back(), 0xCD);
}

TEST(ScadFormatTest, LoadCgxRejectsFileShorterThanTrailer) {
  const std::string path =
      WriteTempFile("scad_short.cgx", std::vector<uint8_t>(0x4FF, 0));
  std::vector<uint8_t> data, loaded, header;
  EXPECT_EQ(gfx::LoadCgx(4, path, data, loaded, header).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ScadFormatTest, LoadCgxUsesOnlyTheViewedPath) {
  const std::string path = WriteTempFile(
      "scad_view.cgx", std::vector<uint8_t>(kCgx2BppDataSize + 0x500, 0));
  const std::string padded = path + ".unrelated-suffix";
  std::vector<uint8_t> data, loaded, header;
  EXPECT_TRUE(gfx::LoadCgx(2, std::string_view(padded).substr(0, path.size()),
                           data, loaded, header)
                  .ok());
}

TEST(ScadFormatTest, LoadScrRejectsTruncatedFiles) {
  std::vector<uint8_t> map_data;
  const std::string scr =
      WriteTempFile("scad_short.SCR", std::vector<uint8_t>(0x10, 0));
  EXPECT_EQ(gfx::LoadScr(scr, 0, map_data).code(),
            absl::StatusCode::kInvalidArgument);
  const std::string pnl =
      WriteTempFile("scad_short.PNL", std::vector<uint8_t>(0x10, 0));
  EXPECT_EQ(gfx::LoadScr(pnl, 1, map_data).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ScadFormatTest, DecodeColFileDropsTrailerAndRejectsShortFiles) {
  std::vector<uint8_t> bytes = {0x1F, 0x00, 0xE0, 0x03};  // red, green
  bytes.resize(bytes.size() + 0x200, 0xFF);
  const auto colors =
      gfx::DecodeColFile(WriteTempFile("scad_valid.col", bytes));
  ASSERT_EQ(colors.size(), 2u);
  EXPECT_EQ(colors[0].r, 0xF8);
  EXPECT_EQ(colors[0].a, 0);
  EXPECT_EQ(colors[1].g, 0xF8);
  EXPECT_EQ(colors[1].a, 255);

  EXPECT_TRUE(gfx::DecodeColFile(WriteTempFile("scad_short.col",
                                               std::vector<uint8_t>(0x10, 0)))
                  .empty());
}

TEST(ScadFormatTest, SaveCgxWritesZeroedReservedHeaderBytes) {
  const std::string path =
      (std::filesystem::path(::testing::TempDir()) / "scad_saved.cgx").string();
  ASSERT_TRUE(gfx::SaveCgx(4, path, std::vector<uint8_t>(0x20, 0x77), {}).ok());
  const std::vector<uint8_t> bytes = ReadFile(path);
  ASSERT_EQ(bytes.size(), sizeof(gfx::CgxHeader) + 0x20);
  for (size_t i = offsetof(gfx::CgxHeader, reserved);
       i < sizeof(gfx::CgxHeader); ++i) {
    ASSERT_EQ(bytes[i], 0) << "header byte " << i;
  }
}

}  // namespace
}  // namespace test
}  // namespace yaze
