#include "util/hex.h"

#include <cstdint>
#include <limits>

#include "gtest/gtest.h"

namespace yaze {
namespace util {
namespace {

TEST(ParseHexStringTest, AcceptsPrefixedAndBareDigits) {
  uint32_t value = 0;
  EXPECT_TRUE(ParseHexString("0x1E8000", &value));
  EXPECT_EQ(value, 0x1E8000u);
  EXPECT_TRUE(ParseHexString("0X1E8000", &value));
  EXPECT_EQ(value, 0x1E8000u);
  EXPECT_TRUE(ParseHexString("$1E8000", &value));
  EXPECT_EQ(value, 0x1E8000u);
  EXPECT_TRUE(ParseHexString("1e8000", &value));
  EXPECT_EQ(value, 0x1E8000u);
}

// The strtoul-based parsers skipped leading whitespace; keep that.
TEST(ParseHexStringTest, IgnoresSurroundingWhitespace) {
  uint32_t value = 0;
  EXPECT_TRUE(ParseHexString("  0x1E8000", &value));
  EXPECT_EQ(value, 0x1E8000u);
  EXPECT_TRUE(ParseHexString("0x1E8000\t", &value));
  EXPECT_EQ(value, 0x1E8000u);
  EXPECT_TRUE(ParseHexString(" 0x1E8000 ", &value));
  EXPECT_EQ(value, 0x1E8000u);
}

// std::stoul stopped at the first invalid character, so "0x1E80zz" used to
// parse as 0x1E80 and a manifest loaded with a wrong address.
TEST(ParseHexStringTest, RejectsTrailingJunk) {
  uint32_t value = 0xABCDu;
  EXPECT_FALSE(ParseHexString("0x1E80zz", &value));
  EXPECT_FALSE(ParseHexString("0x1E80 ; comment", &value));
  EXPECT_FALSE(ParseHexString("1E 80", &value));
  EXPECT_FALSE(ParseHexString("0x1E80.5", &value));
  EXPECT_EQ(value, 0xABCDu) << "out must be untouched on failure";
}

// The old uint32 cast truncated silently: 0x1'0000'0000 became 0.
TEST(ParseHexStringTest, RejectsValuesTooLargeForTheTarget) {
  uint32_t value = 0;
  EXPECT_FALSE(ParseHexString("0x100000000", &value));
  EXPECT_FALSE(ParseHexString("1FFFFFFFF", &value));
  EXPECT_TRUE(ParseHexString("0xFFFFFFFF", &value));
  EXPECT_EQ(value, 0xFFFFFFFFu);

  int signed_value = 0;
  EXPECT_FALSE(ParseHexString("0x80000000", &signed_value));
  EXPECT_TRUE(ParseHexString("0x7FFFFFFF", &signed_value));
  EXPECT_EQ(signed_value, std::numeric_limits<int>::max());

  uint64_t wide = 0;
  EXPECT_TRUE(ParseHexString("0xFFFFFFFFFFFFFFFF", &wide));
  EXPECT_EQ(wide, std::numeric_limits<uint64_t>::max());
  EXPECT_FALSE(ParseHexString("0x10000000000000000", &wide));
}

// strtoul negated instead of failing, so "-1" became 0xFFFFFFFF.
TEST(ParseHexStringTest, RejectsSignedInput) {
  uint32_t value = 0;
  EXPECT_FALSE(ParseHexString("-1", &value));
  EXPECT_FALSE(ParseHexString("0x-1", &value));
  EXPECT_FALSE(ParseHexString("+1E8000", &value));
}

TEST(ParseHexStringTest, RejectsEmptyAndPrefixOnlyInput) {
  uint32_t value = 0;
  EXPECT_FALSE(ParseHexString("", &value));
  EXPECT_FALSE(ParseHexString("   ", &value));
  EXPECT_FALSE(ParseHexString("0x", &value));
  EXPECT_FALSE(ParseHexString("$", &value));
}

}  // namespace
}  // namespace util
}  // namespace yaze
