#include <gtest/gtest.h>

#include "zelda3/dungeon/dungeon_torch_codec.h"

namespace yaze::zelda3::test {

TEST(DungeonTorchCodecTest, DecodesAllFieldsFromKnownWord) {
  const auto entry = DecodeLightableTorchEntry(LightableTorchBytes{0xFE, 0xFF});

  EXPECT_EQ(entry.px, 63);
  EXPECT_EQ(entry.py, 63);
  EXPECT_EQ(entry.draw_layer, 1);
  EXPECT_EQ(entry.reserved, 1);
  EXPECT_TRUE(entry.lit);
}

TEST(DungeonTorchCodecTest, BitThirteenSelectsLowerBackground) {
  const auto entry = DecodeLightableTorchEntry(LightableTorchBytes{0x14, 0x20});

  EXPECT_EQ(entry.px, 10);
  EXPECT_EQ(entry.py, 0);
  EXPECT_EQ(entry.draw_layer, 1);
  EXPECT_EQ(entry.reserved, 0);
  EXPECT_FALSE(entry.lit);
}

TEST(DungeonTorchCodecTest, BitFourteenIsPreservedIndependently) {
  const auto entry = DecodeLightableTorchEntry(LightableTorchBytes{0x14, 0x40});

  EXPECT_EQ(entry.px, 10);
  EXPECT_EQ(entry.py, 0);
  EXPECT_EQ(entry.draw_layer, 0);
  EXPECT_EQ(entry.reserved, 1);
  EXPECT_FALSE(entry.lit);
}

TEST(DungeonTorchCodecTest, EncodesKnownLowerLayerLitEntry) {
  const auto bytes = EncodeLightableTorchEntry(LightableTorchEntry{
      .px = 10, .py = 20, .draw_layer = 1, .reserved = 0, .lit = true});

  EXPECT_EQ(bytes.low, 0x14);
  EXPECT_EQ(bytes.high, 0xAA);
}

TEST(DungeonTorchCodecTest, EncodesKnownReservedLowerLayerLitEntry) {
  const auto bytes = EncodeLightableTorchEntry(LightableTorchEntry{
      .px = 10, .py = 20, .draw_layer = 1, .reserved = 1, .lit = true});

  EXPECT_EQ(bytes.low, 0x14);
  EXPECT_EQ(bytes.high, 0xEA);
}

TEST(DungeonTorchCodecTest, RoundTripsAllRepresentableCoordinatesAndFlags) {
  for (uint16_t px = 0; px < 64; ++px) {
    for (uint16_t py = 0; py < 64; ++py) {
      for (uint16_t draw_layer = 0; draw_layer < 2; ++draw_layer) {
        for (uint16_t reserved = 0; reserved < 2; ++reserved) {
          for (const bool lit : {false, true}) {
            const LightableTorchEntry input{
                .px = static_cast<uint8_t>(px),
                .py = static_cast<uint8_t>(py),
                .draw_layer = static_cast<uint8_t>(draw_layer),
                .reserved = static_cast<uint8_t>(reserved),
                .lit = lit,
            };
            const auto output =
                DecodeLightableTorchEntry(EncodeLightableTorchEntry(input));
            EXPECT_EQ(output.px, input.px);
            EXPECT_EQ(output.py, input.py);
            EXPECT_EQ(output.draw_layer, input.draw_layer);
            EXPECT_EQ(output.reserved, input.reserved);
            EXPECT_EQ(output.lit, input.lit);
          }
        }
      }
    }
  }
}

}  // namespace yaze::zelda3::test
