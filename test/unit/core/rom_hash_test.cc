#include "util/rom_hash.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "util/bps.h"

#include <gtest/gtest.h>
#include "unique_temp_path.h"

namespace yaze::test {
namespace {

namespace fs = std::filesystem;

TEST(RomHashTest, StandardCrc32VectorsAndAllSingleBytes) {
  EXPECT_EQ(util::CalculateCrc32(nullptr, 0), 0u);
  const std::string check = "123456789";
  EXPECT_EQ(util::ComputeRomHash(reinterpret_cast<const uint8_t*>(check.data()),
                                 check.size()),
            "cbf43926");
  const uint8_t abc[] = {'a', 'b', 'c'};
  const auto abc_identity = util::ComputeRomIdentityHashes(abc, sizeof(abc));
  EXPECT_EQ(abc_identity.crc32, "352441c2");
  EXPECT_EQ(abc_identity.legacy_crc32, "354537c2");
  size_t legacy_differences = 0;
  for (int value = 0; value < 256; ++value) {
    const uint8_t byte = value;
    uint32_t reference = 0xffffffff ^ byte;
    for (int bit = 0; bit < 8; ++bit) {
      reference = (reference >> 1) ^ ((reference & 1) ? 0xedb88320 : 0);
    }
    EXPECT_EQ(util::CalculateCrc32(&byte, 1), reference ^ 0xffffffff) << value;
    const auto identity = util::ComputeRomIdentityHashes(&byte, 1);
    legacy_differences += identity.crc32 != identity.legacy_crc32;
  }
  EXPECT_EQ(legacy_differences, 15u);
}

TEST(RomHashTest, LegacyCompatibilityRequiresExactRomAndKeepsSha1Distinct) {
  const std::vector<uint8_t> bytes(0x8000, 0x2a);
  const auto identity =
      util::ComputeRomIdentityHashes(bytes.data(), bytes.size());
  ASSERT_NE(identity.crc32, identity.legacy_crc32);
  EXPECT_EQ(identity.Match(identity.crc32), util::RomHashMatch::kCrc32);
  EXPECT_EQ(identity.Match(identity.legacy_crc32),
            util::RomHashMatch::kLegacyCrc32);
  EXPECT_EQ(identity.Match(identity.sha1), util::RomHashMatch::kSha1);
  EXPECT_EQ(identity.Match(" 0x" + identity.legacy_crc32 + " "),
            util::RomHashMatch::kLegacyCrc32);
  EXPECT_EQ(identity.Match(""), util::RomHashMatch::kMismatch);
  EXPECT_EQ(identity.Match("deadbeef"), util::RomHashMatch::kMismatch);
  EXPECT_EQ(identity.Match("0000000g"), util::RomHashMatch::kMismatch);
  EXPECT_EQ(identity.Match(identity.crc32.substr(0, 4) + " " +
                           identity.crc32.substr(4)),
            util::RomHashMatch::kMismatch);
  auto changed = bytes;
  changed[0] ^= 1;
  const auto foreign =
      util::ComputeRomIdentityHashes(changed.data(), changed.size());
  for (const auto& expected :
       {identity.crc32, identity.legacy_crc32, identity.sha1}) {
    EXPECT_EQ(foreign.Match(expected), util::RomHashMatch::kMismatch);
  }
}

TEST(RomHashTest, DisposableRealFixtureMatchesIndependentBpsCrc) {
  const char* fixture = std::getenv("YAZE_TEST_ROM_VANILLA");
  if (!fixture)
    GTEST_SKIP() << "Set YAZE_TEST_ROM_VANILLA";
  const auto copy = UniqueTempPath("crc-real-fixture", ".sfc");
  ASSERT_TRUE(fs::copy_file(fixture, copy));
  std::ifstream file(copy, std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
  fs::remove(copy);
  ASSERT_FALSE(bytes.empty());
  const auto identity =
      util::ComputeRomIdentityHashes(bytes.data(), bytes.size());
  std::vector<uint8_t> patch;
  ASSERT_TRUE(util::CreateBpsPatch(bytes, bytes, patch).ok());
  ASSERT_GE(patch.size(), 12u);
  uint32_t bps_source_crc = 0;
  for (int i = 0; i < 4; ++i)
    bps_source_crc |= uint32_t(patch[patch.size() - 12 + i]) << (8 * i);
  EXPECT_EQ(util::CalculateCrc32(bytes.data(), bytes.size()), bps_source_crc);
  EXPECT_EQ(identity.Match(identity.legacy_crc32),
            util::RomHashMatch::kLegacyCrc32);
}

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
      ::yaze::test::UniqueTempPath("yaze_rom_hash_test_buffer", ".bin");
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
      ::yaze::test::UniqueTempPath("yaze_rom_hash_test_empty", ".bin");
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
