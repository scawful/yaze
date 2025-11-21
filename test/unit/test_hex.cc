#include "gtest/gtest.h"
#include "util/hex.h"

using namespace yaze::util;

TEST(HexTest, HexByteDefault) {
    EXPECT_EQ(HexByte(0x42), "$42");
}

TEST(HexTest, HexByteLowercase) {
    HexStringParams params;
    params.uppercase = false;
    EXPECT_EQ(HexByte(0xAB, params), "$ab");
}

TEST(HexTest, HexByteNoPrefix) {
    HexStringParams params;
    params.prefix = HexStringParams::Prefix::kNone;
    EXPECT_EQ(HexByte(0x12, params), "12");
}

TEST(HexTest, HexByte0xPrefix) {
    HexStringParams params;
    params.prefix = HexStringParams::Prefix::k0x;
    EXPECT_EQ(HexByte(0xCD, params), "0xCD");
}

TEST(HexTest, HexByteHashPrefix) {
    HexStringParams params;
    params.prefix = HexStringParams::Prefix::kHash;
    EXPECT_EQ(HexByte(0xEF, params), "#EF");
}

TEST(HexTest, HexByteZero) {
    EXPECT_EQ(HexByte(0x00), "$00");
}

TEST(HexTest, HexByteMax) {
    EXPECT_EQ(HexByte(0xFF), "$FF");
}
