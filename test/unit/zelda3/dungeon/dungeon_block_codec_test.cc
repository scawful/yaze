#include "zelda3/dungeon/dungeon_block_codec.h"

#include <cstdint>

#include <gtest/gtest.h>

namespace yaze::zelda3 {
namespace {

PushableBlockBytes MakeBytes(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
  return PushableBlockBytes{b1, b2, b3, b4};
}

TEST(DungeonBlockCodecTest, DecodesVanillaSampleEntryAtOriginLayerZero) {
  // Smallest representative: room_id = 0x004D (Misery Mire main room),
  // px = py = 0, layer = 0 → word = 0x0000 → b3 = b4 = 0x00.
  const PushableBlockBytes bytes = MakeBytes(0x4D, 0x00, 0x00, 0x00);
  const PushableBlockEntry entry = DecodePushableBlockEntry(bytes);
  EXPECT_EQ(entry.room_id, 0x004D);
  EXPECT_EQ(entry.px, 0);
  EXPECT_EQ(entry.py, 0);
  EXPECT_EQ(entry.layer, 0);
}

TEST(DungeonBlockCodecTest, DecodesPositionPyAboveSixtyThree) {
  // Vanilla room 0x00A8 stores a block at py=109, which sets bit 13 of the
  // encoded word. The previous LoadBlocks decoder masked b4 with 0x1F and
  // dropped that bit, silently corrupting the loaded position. The fixed
  // decoder must recover py=109 cleanly.
  //
  // word = (px=0 << 1) | (py=109 << 7) | (layer=0 << 14) = 109 * 128 = 13952
  // = 0x3680 → b3 = 0x80, b4 = 0x36.
  const PushableBlockBytes bytes = MakeBytes(0xA8, 0x00, 0x80, 0x36);
  const PushableBlockEntry entry = DecodePushableBlockEntry(bytes);
  EXPECT_EQ(entry.room_id, 0x00A8);
  EXPECT_EQ(entry.px, 0);
  EXPECT_EQ(entry.py, 109)
      << "py=109 must survive decode; bit 13 was dropped before the fix.";
  EXPECT_EQ(entry.layer, 0);
}

TEST(DungeonBlockCodecTest, DecodesLayerOne) {
  // Layer is bit 14 of the encoded word, i.e. bit 6 of b4 (0x40 mask).
  // The previous decoder used (b4 & 0x20) >> 5 — i.e. bit 13 of the word —
  // which is actually the high bit of py. A layer-1 block with px=0 and
  // py=0 would have decoded as layer=0.
  //
  // word = (0 << 1) | (0 << 7) | (1 << 14) = 0x4000 → b3 = 0x00, b4 = 0x40.
  const PushableBlockBytes bytes = MakeBytes(0x12, 0x00, 0x00, 0x40);
  const PushableBlockEntry entry = DecodePushableBlockEntry(bytes);
  EXPECT_EQ(entry.room_id, 0x0012);
  EXPECT_EQ(entry.px, 0);
  EXPECT_EQ(entry.py, 0);
  EXPECT_EQ(entry.layer, 1) << "layer must be bit 14 of the word, not bit 13.";
}

TEST(DungeonBlockCodecTest, DecodesPxAtMaxRange) {
  // px = 63, py = 0, layer = 0 → word = 63 << 1 = 126 = 0x007E.
  const PushableBlockBytes bytes = MakeBytes(0x00, 0x00, 0x7E, 0x00);
  const PushableBlockEntry entry = DecodePushableBlockEntry(bytes);
  EXPECT_EQ(entry.px, 63);
  EXPECT_EQ(entry.py, 0);
  EXPECT_EQ(entry.layer, 0);
}

TEST(DungeonBlockCodecTest, EncodeRoundTripsIdentityForOriginLayerZero) {
  PushableBlockEntry entry;
  entry.room_id = 0x0034;
  entry.px = 0;
  entry.py = 0;
  entry.layer = 0;
  const PushableBlockBytes bytes = EncodePushableBlockEntry(entry);
  EXPECT_EQ(bytes.b1, 0x34);
  EXPECT_EQ(bytes.b2, 0x00);
  EXPECT_EQ(bytes.b3, 0x00);
  EXPECT_EQ(bytes.b4, 0x00);
  const PushableBlockEntry roundtripped = DecodePushableBlockEntry(bytes);
  EXPECT_EQ(roundtripped.room_id, entry.room_id);
  EXPECT_EQ(roundtripped.px, entry.px);
  EXPECT_EQ(roundtripped.py, entry.py);
  EXPECT_EQ(roundtripped.layer, entry.layer);
}

TEST(DungeonBlockCodecTest, EncodeRoundTripsHighPyAndLowerLayer) {
  // The combined edge case: a block using the lower/BG2 selector (layer=1) at
  // the high-py end of the room. Round-trip must hold for the previously-buggy
  // regions.
  PushableBlockEntry entry;
  entry.room_id = 0x010B;
  entry.px = 5;
  entry.py = 120;
  entry.layer = 1;
  const PushableBlockBytes bytes = EncodePushableBlockEntry(entry);
  const PushableBlockEntry roundtripped = DecodePushableBlockEntry(bytes);
  EXPECT_EQ(roundtripped.room_id, entry.room_id);
  EXPECT_EQ(roundtripped.px, entry.px);
  EXPECT_EQ(roundtripped.py, entry.py);
  EXPECT_EQ(roundtripped.layer, entry.layer);
}

TEST(DungeonBlockCodecTest, EncodeRoundTripsExhaustivelyAcrossAllValidValues) {
  // Sweep the entire (px, py, layer) space (64 * 128 * 2 = 16384 cases) and
  // confirm decode(encode(x)) == x for every valid input. Cheap check that
  // pins the bit layout against silent regressions.
  for (uint8_t px = 0; px < 64; ++px) {
    for (uint8_t py = 0; py < 128; ++py) {
      for (uint8_t layer = 0; layer < 2; ++layer) {
        PushableBlockEntry entry;
        entry.room_id = 0x1234;
        entry.px = px;
        entry.py = py;
        entry.layer = layer;
        const PushableBlockBytes bytes = EncodePushableBlockEntry(entry);
        const PushableBlockEntry roundtripped = DecodePushableBlockEntry(bytes);
        ASSERT_EQ(roundtripped.px, px)
            << "px=" << +px << " py=" << +py << " layer=" << +layer;
        ASSERT_EQ(roundtripped.py, py)
            << "px=" << +px << " py=" << +py << " layer=" << +layer;
        ASSERT_EQ(roundtripped.layer, layer)
            << "px=" << +px << " py=" << +py << " layer=" << +layer;
      }
    }
  }
}

TEST(DungeonBlockCodecTest, EncodeMasksOutOfRangeInputsToValidBits) {
  // Defensive: if a caller hands in an out-of-range px/py/layer (bit set
  // above the documented range), the encoder masks the extra bits off
  // rather than smearing them into adjacent fields. Documents the bit
  // layout's safety margin.
  PushableBlockEntry entry;
  entry.room_id = 0xABCD;
  entry.px = 0xFF;     // overflow: only bits 0..5 should survive
  entry.py = 0xFF;     // overflow: only bits 0..6 should survive
  entry.layer = 0xFF;  // overflow: only bit 0 should survive
  const PushableBlockBytes bytes = EncodePushableBlockEntry(entry);
  const PushableBlockEntry roundtripped = DecodePushableBlockEntry(bytes);
  EXPECT_EQ(roundtripped.px, 63);
  EXPECT_EQ(roundtripped.py, 127);
  EXPECT_EQ(roundtripped.layer, 1);
}

}  // namespace
}  // namespace yaze::zelda3
