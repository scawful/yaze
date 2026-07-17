#include "zelda3/dungeon/dungeon_block_codec.h"

#include <array>
#include <cstdint>
#include <iomanip>

#include <gtest/gtest.h>

namespace yaze::zelda3 {
namespace {

PushableBlockBytes MakeBytes(uint16_t room_id, uint16_t word) {
  return PushableBlockBytes{
      static_cast<uint8_t>(room_id & 0xFF),
      static_cast<uint8_t>((room_id >> 8) & 0xFF),
      static_cast<uint8_t>(word & 0xFF),
      static_cast<uint8_t>((word >> 8) & 0xFF),
  };
}

uint16_t PositionWord(const PushableBlockBytes& bytes) {
  return static_cast<uint16_t>(bytes.b3 | (bytes.b4 << 8));
}

TEST(DungeonBlockCodecTest, DecodesOriginWithIndependentSelectorsClear) {
  const PushableBlockEntry entry =
      DecodePushableBlockEntry(MakeBytes(/*room_id=*/0x004D, /*word=*/0));

  EXPECT_EQ(entry.room_id, 0x004D);
  EXPECT_EQ(entry.px, 0);
  EXPECT_EQ(entry.py, 0);
  EXPECT_EQ(entry.draw_layer, 0);
  EXPECT_EQ(entry.behavior_layer, 0);
}

TEST(DungeonBlockCodecTest, DecodesExactVanillaLowerDrawWords) {
  struct Sample {
    uint16_t room_id;
    uint16_t word;
    uint8_t px;
    uint8_t py;
  };
  constexpr std::array<Sample, 3> kSamples = {{
      {0x00A8, 0x36E0, 48, 45},
      {0x0066, 0x383C, 30, 48},
      {0x002C, 0x2814, 10, 16},
  }};

  for (const auto& sample : kSamples) {
    const PushableBlockEntry entry =
        DecodePushableBlockEntry(MakeBytes(sample.room_id, sample.word));
    EXPECT_EQ(entry.room_id, sample.room_id);
    EXPECT_EQ(entry.px, sample.px) << "word=0x" << std::hex << sample.word;
    EXPECT_EQ(entry.py, sample.py) << "word=0x" << std::hex << sample.word;
    EXPECT_EQ(entry.draw_layer, 1)
        << "bit 13 selects lower/BG2 for word=0x" << std::hex << sample.word;
    EXPECT_EQ(entry.behavior_layer, 0)
        << "bit 14 stays clear for word=0x" << std::hex << sample.word;
    EXPECT_EQ(PositionWord(EncodePushableBlockEntry(entry)), sample.word);
  }
}

TEST(DungeonBlockCodecTest, DecodesBehaviorBitIndependentlyFromDrawTarget) {
  // Vanilla room 0xCA word $56B2 has bit 14 set while bit 13 is clear.
  const PushableBlockEntry entry =
      DecodePushableBlockEntry(MakeBytes(/*room_id=*/0x00CA, /*word=*/0x56B2));

  EXPECT_EQ(entry.px, 25);
  EXPECT_EQ(entry.py, 45);
  EXPECT_EQ(entry.draw_layer, 0);
  EXPECT_EQ(entry.behavior_layer, 1);
  EXPECT_EQ(PositionWord(EncodePushableBlockEntry(entry)), 0x56B2);
}

TEST(DungeonBlockCodecTest, DecodesPxAtMaxRange) {
  const PushableBlockEntry entry =
      DecodePushableBlockEntry(MakeBytes(/*room_id=*/0x0000, /*word=*/0x007E));

  EXPECT_EQ(entry.px, 63);
  EXPECT_EQ(entry.py, 0);
  EXPECT_EQ(entry.draw_layer, 0);
  EXPECT_EQ(entry.behavior_layer, 0);
}

TEST(DungeonBlockCodecTest, EncodeRoundTripsEveryValidFieldCombination) {
  for (uint8_t px = 0; px < 64; ++px) {
    for (uint8_t py = 0; py < 64; ++py) {
      for (uint8_t draw_layer = 0; draw_layer < 2; ++draw_layer) {
        for (uint8_t behavior_layer = 0; behavior_layer < 2; ++behavior_layer) {
          PushableBlockEntry entry;
          entry.room_id = 0x1234;
          entry.px = px;
          entry.py = py;
          entry.draw_layer = draw_layer;
          entry.behavior_layer = behavior_layer;

          const PushableBlockEntry roundtripped =
              DecodePushableBlockEntry(EncodePushableBlockEntry(entry));
          ASSERT_EQ(roundtripped.room_id, entry.room_id);
          ASSERT_EQ(roundtripped.px, px);
          ASSERT_EQ(roundtripped.py, py);
          ASSERT_EQ(roundtripped.draw_layer, draw_layer);
          ASSERT_EQ(roundtripped.behavior_layer, behavior_layer);
        }
      }
    }
  }
}

TEST(DungeonBlockCodecTest, EncodeMasksInputsToTheirDedicatedBits) {
  PushableBlockEntry entry;
  entry.room_id = 0xABCD;
  entry.px = 0xFF;
  entry.py = 0xFF;
  entry.draw_layer = 0xFF;
  entry.behavior_layer = 0xFF;

  const PushableBlockEntry roundtripped =
      DecodePushableBlockEntry(EncodePushableBlockEntry(entry));

  EXPECT_EQ(roundtripped.room_id, 0xABCD);
  EXPECT_EQ(roundtripped.px, 63);
  EXPECT_EQ(roundtripped.py, 63);
  EXPECT_EQ(roundtripped.draw_layer, 1);
  EXPECT_EQ(roundtripped.behavior_layer, 1);
}

}  // namespace
}  // namespace yaze::zelda3
