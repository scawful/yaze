#include "util/hex.h"

#include "testing.h"

namespace yaze {
namespace test {

using ::testing::Eq;

TEST(HexTest, HexByte) {
  // Test basic byte conversion
  EXPECT_THAT(util::HexByte(0x00), Eq("$00"));
  EXPECT_THAT(util::HexByte(0xFF), Eq("$FF"));
  EXPECT_THAT(util::HexByte(0x1A), Eq("$1A"));

  // Test different prefixes
  util::HexStringParams params;
  params.prefix = util::HexStringParams::Prefix::kNone;
  EXPECT_THAT(util::HexByte(0x1A, params), Eq("1A"));

  params.prefix = util::HexStringParams::Prefix::kHash;
  EXPECT_THAT(util::HexByte(0x1A, params), Eq("#1A"));

  params.prefix = util::HexStringParams::Prefix::k0x;
  EXPECT_THAT(util::HexByte(0x1A, params), Eq("0x1A"));

  // Test lowercase
  params.prefix = util::HexStringParams::Prefix::kNone;
  params.uppercase = false;
  EXPECT_THAT(util::HexByte(0x1A, params), Eq("1a"));
}

TEST(HexTest, HexWord) {
  // Test basic word conversion
  EXPECT_THAT(util::HexWord(0x0000), Eq("$0000"));
  EXPECT_THAT(util::HexWord(0xFFFF), Eq("$FFFF"));
  EXPECT_THAT(util::HexWord(0x1A2B), Eq("$1A2B"));

  // Test different prefixes
  util::HexStringParams params;
  params.prefix = util::HexStringParams::Prefix::kNone;
  EXPECT_THAT(util::HexWord(0x1A2B, params), Eq("1A2B"));

  params.prefix = util::HexStringParams::Prefix::kHash;
  EXPECT_THAT(util::HexWord(0x1A2B, params), Eq("#1A2B"));

  params.prefix = util::HexStringParams::Prefix::k0x;
  EXPECT_THAT(util::HexWord(0x1A2B, params), Eq("0x1A2B"));

  // Test lowercase
  params.prefix = util::HexStringParams::Prefix::kNone;
  params.uppercase = false;
  EXPECT_THAT(util::HexWord(0x1A2B, params), Eq("1a2b"));
}

TEST(HexTest, HexLong) {
  // Test basic long conversion
  EXPECT_THAT(util::HexLong(0x000000), Eq("$000000"));
  EXPECT_THAT(util::HexLong(0xFFFFFF), Eq("$FFFFFF"));
  EXPECT_THAT(util::HexLong(0x1A2B3C), Eq("$1A2B3C"));

  // Test different prefixes
  util::HexStringParams params;
  params.prefix = util::HexStringParams::Prefix::kNone;
  EXPECT_THAT(util::HexLong(0x1A2B3C, params), Eq("1A2B3C"));

  params.prefix = util::HexStringParams::Prefix::kHash;
  EXPECT_THAT(util::HexLong(0x1A2B3C, params), Eq("#1A2B3C"));

  params.prefix = util::HexStringParams::Prefix::k0x;
  EXPECT_THAT(util::HexLong(0x1A2B3C, params), Eq("0x1A2B3C"));

  // Test lowercase
  params.prefix = util::HexStringParams::Prefix::kNone;
  params.uppercase = false;
  EXPECT_THAT(util::HexLong(0x1A2B3C, params), Eq("1a2b3c"));
}

TEST(HexTest, HexLongLong) {
  // Test basic long long conversion
  EXPECT_THAT(util::HexLongLong(0x00000000), Eq("$00000000"));
  EXPECT_THAT(util::HexLongLong(0xFFFFFFFF), Eq("$FFFFFFFF"));
  EXPECT_THAT(util::HexLongLong(0x1A2B3C4D), Eq("$1A2B3C4D"));

  // Test different prefixes
  util::HexStringParams params;
  params.prefix = util::HexStringParams::Prefix::kNone;
  EXPECT_THAT(util::HexLongLong(0x1A2B3C4D, params), Eq("1A2B3C4D"));

  params.prefix = util::HexStringParams::Prefix::kHash;
  EXPECT_THAT(util::HexLongLong(0x1A2B3C4D, params), Eq("#1A2B3C4D"));

  params.prefix = util::HexStringParams::Prefix::k0x;
  EXPECT_THAT(util::HexLongLong(0x1A2B3C4D, params), Eq("0x1A2B3C4D"));

  // Test lowercase
  params.prefix = util::HexStringParams::Prefix::kNone;
  params.uppercase = false;
  EXPECT_THAT(util::HexLongLong(0x1A2B3C4D, params), Eq("1a2b3c4d"));
}

}  // namespace test
}  // namespace yaze