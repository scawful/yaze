#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "zelda.h"

namespace yaze::zelda3 {
namespace {

constexpr size_t kTitleOffset = 0x7FC0;
constexpr size_t kRegionOffset = 0x7FD9;

std::vector<uint8_t> MakeHeader(const std::string& title, uint8_t region) {
  std::vector<uint8_t> rom(0x8000, 0x00);
  std::memset(rom.data() + kTitleOffset, ' ', 21);
  std::memcpy(rom.data() + kTitleOffset, title.data(),
              std::min<size_t>(title.size(), 21));
  rom[kRegionOffset] = region;
  return rom;
}

// zelda3_detect_version is part of the C API in inc/zelda.h. It must link
// from yaze_zelda3; the e2e ROM tests call it.
TEST(Zelda3VersionTest, DetectsRegionFromHeader) {
  auto us = MakeHeader("THE LEGEND OF ZELDA", 0x01);
  auto eu = MakeHeader("THE LEGEND OF ZELDA", 0x02);
  auto jp_region = MakeHeader("THE LEGEND OF ZELDA", 0x00);
  auto jp_title = MakeHeader("ZELDA NO DENSETSU", 0x00);

  EXPECT_EQ(zelda3_detect_version(us.data(), us.size()), ZELDA3_VERSION_US);
  EXPECT_EQ(zelda3_detect_version(eu.data(), eu.size()), ZELDA3_VERSION_EU);
  EXPECT_EQ(zelda3_detect_version(jp_region.data(), jp_region.size()),
            ZELDA3_VERSION_JP);
  EXPECT_EQ(zelda3_detect_version(jp_title.data(), jp_title.size()),
            ZELDA3_VERSION_JP);
}

TEST(Zelda3VersionTest, RejectsMissingOrShortData) {
  std::vector<uint8_t> short_rom(0x7FFF, 0x00);

  EXPECT_EQ(zelda3_detect_version(nullptr, 0x8000), ZELDA3_VERSION_UNKNOWN);
  EXPECT_EQ(zelda3_detect_version(short_rom.data(), short_rom.size()),
            ZELDA3_VERSION_UNKNOWN);
}

TEST(Zelda3VersionTest, VersionNames) {
  EXPECT_STREQ(zelda3_version_to_string(ZELDA3_VERSION_US),
               "US/North American");
  EXPECT_STREQ(zelda3_version_to_string(ZELDA3_VERSION_UNKNOWN), "Unknown");
}

}  // namespace
}  // namespace yaze::zelda3
