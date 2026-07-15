#include <gtest/gtest.h>

#include "zelda3/dungeon/dungeon_torch_codec.h"

namespace yaze::zelda3::test {

TEST(DungeonTorchCodecTest, DecodesAllPositionBitsSelectorAndLitFlag) {
  const auto entry = DecodeLightableTorchEntry(LightableTorchBytes{0xFE, 0xFF});

  EXPECT_EQ(entry.px, 63);
  EXPECT_EQ(entry.py, 127);
  EXPECT_EQ(entry.selector, 1);
  EXPECT_TRUE(entry.lit);
}

TEST(DungeonTorchCodecTest, HighYBitIsNotTheSelector) {
  const auto entry = DecodeLightableTorchEntry(LightableTorchBytes{0x14, 0x20});

  EXPECT_EQ(entry.px, 10);
  EXPECT_EQ(entry.py, 64);
  EXPECT_EQ(entry.selector, 0);
  EXPECT_FALSE(entry.lit);
}

TEST(DungeonTorchCodecTest, SelectorUsesBitFourteen) {
  const auto entry = DecodeLightableTorchEntry(LightableTorchBytes{0x14, 0x40});

  EXPECT_EQ(entry.px, 10);
  EXPECT_EQ(entry.py, 0);
  EXPECT_EQ(entry.selector, 1);
  EXPECT_FALSE(entry.lit);
}

TEST(DungeonTorchCodecTest, EncodesKnownHighYSelectorAndLitEntry) {
  const auto bytes = EncodeLightableTorchEntry(
      LightableTorchEntry{.px = 10, .py = 100, .selector = 1, .lit = true});

  EXPECT_EQ(bytes.low, 0x14);
  EXPECT_EQ(bytes.high, 0xF2);
}

TEST(DungeonTorchCodecTest, RoundTripsSupportedValues) {
  for (uint16_t px = 0; px < 64; ++px) {
    for (uint16_t py = 0; py < 128; ++py) {
      for (uint16_t selector = 0; selector < 2; ++selector) {
        for (const bool lit : {false, true}) {
          const LightableTorchEntry input{
              .px = static_cast<uint8_t>(px),
              .py = static_cast<uint8_t>(py),
              .selector = static_cast<uint8_t>(selector),
              .lit = lit,
          };
          const auto output =
              DecodeLightableTorchEntry(EncodeLightableTorchEntry(input));
          EXPECT_EQ(output.px, input.px);
          EXPECT_EQ(output.py, input.py);
          EXPECT_EQ(output.selector, input.selector);
          EXPECT_EQ(output.lit, input.lit);
        }
      }
    }
  }
}

TEST(DungeonTorchCodecTest, EncodeMasksInputsToSupportedBitRanges) {
  const auto output =
      DecodeLightableTorchEntry(EncodeLightableTorchEntry(LightableTorchEntry{
          .px = 0xFF, .py = 0xFF, .selector = 0xFF, .lit = true}));

  EXPECT_EQ(output.px, 63);
  EXPECT_EQ(output.py, 127);
  EXPECT_EQ(output.selector, 1);
  EXPECT_TRUE(output.lit);
}

}  // namespace yaze::zelda3::test
