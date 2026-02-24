#include "util/rom_hash.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace yaze::test {
namespace {

namespace fs = std::filesystem;

TEST(RomHashTest, ComputeSha1HexMatchesKnownValueForAbc) {
  const uint8_t kData[] = {'a', 'b', 'c'};
  const std::string sha1 = util::ComputeSha1Hex(kData, sizeof(kData));
  EXPECT_EQ(sha1, "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST(RomHashTest, ComputeSha1HexMatchesKnownValueForEmptyInput) {
  const std::string sha1 = util::ComputeSha1Hex(nullptr, 0);
  EXPECT_EQ(sha1, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST(RomHashTest, ComputeFileSha1HexMatchesBufferHash) {
  const std::vector<uint8_t> kBytes = {0x01, 0x02, 0x03, 0x04, 0x05};
  const std::string expected =
      util::ComputeSha1Hex(kBytes.data(), kBytes.size());

  const fs::path path =
      fs::temp_directory_path() / "yaze_rom_hash_test_buffer.bin";
  {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(file.is_open());
    file.write(reinterpret_cast<const char*>(kBytes.data()),
               static_cast<std::streamsize>(kBytes.size()));
  }

  const std::string actual = util::ComputeFileSha1Hex(path.string());
  std::error_code ec;
  fs::remove(path, ec);

  EXPECT_EQ(actual, expected);
}

TEST(RomHashTest, ComputeFileSha1HexHandlesEmptyFile) {
  const fs::path path =
      fs::temp_directory_path() / "yaze_rom_hash_test_empty.bin";
  {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(file.is_open());
  }

  const std::string sha1 = util::ComputeFileSha1Hex(path.string());
  std::error_code ec;
  fs::remove(path, ec);

  EXPECT_EQ(sha1, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

}  // namespace
}  // namespace yaze::test
