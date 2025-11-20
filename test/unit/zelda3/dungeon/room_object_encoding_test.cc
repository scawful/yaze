// test/zelda3/dungeon/room_object_encoding_test.cc
// Unit tests for Phase 1, Task 1.1: Object Encoding/Decoding
//
// These tests verify that the object encoding and decoding functions work
// correctly for all three object types (Type1, Type2, Type3) based on
// ZScream's proven implementation.

#include "zelda3/dungeon/room_object.h"

#include <gtest/gtest.h>

namespace yaze {
namespace zelda3 {
namespace {

// ============================================================================
// Object Type Detection Tests
// ============================================================================

TEST(RoomObjectEncodingTest, DetermineObjectTypeType1) {
  // Type1: b1 < 0xFC, b3 < 0xF8
  EXPECT_EQ(RoomObject::DetermineObjectType(0x28, 0x10), 1);
  EXPECT_EQ(RoomObject::DetermineObjectType(0x50, 0x42), 1);
  EXPECT_EQ(RoomObject::DetermineObjectType(0xFB, 0xF7), 1);
}

TEST(RoomObjectEncodingTest, DetermineObjectTypeType2) {
  // Type2: b1 >= 0xFC, b3 < 0xF8
  EXPECT_EQ(RoomObject::DetermineObjectType(0xFC, 0x42), 2);
  EXPECT_EQ(RoomObject::DetermineObjectType(0xFD, 0x25), 2);
  EXPECT_EQ(RoomObject::DetermineObjectType(0xFF, 0x00), 2);
}

TEST(RoomObjectEncodingTest, DetermineObjectTypeType3) {
  // Type3: b3 >= 0xF8
  EXPECT_EQ(RoomObject::DetermineObjectType(0x28, 0xF8), 3);
  EXPECT_EQ(RoomObject::DetermineObjectType(0x50, 0xF9), 3);
  EXPECT_EQ(RoomObject::DetermineObjectType(0xFC, 0xFF), 3);
}

// ============================================================================
// Type 1 Object Encoding/Decoding Tests
// ============================================================================

TEST(RoomObjectEncodingTest, Type1EncodeDecodeBasic) {
  // Type1: xxxxxxss yyyyyyss iiiiiiii
  // Example: Object ID 0x42, position (10, 20), size 3, layer 0

  RoomObject obj(0x42, 10, 20, 3, 0);

  // Encode
  auto bytes = obj.EncodeObjectToBytes();

  // Decode
  auto decoded =
      RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 0);

  // Verify
  EXPECT_EQ(decoded.id_, obj.id_);
  EXPECT_EQ(decoded.x(), obj.x());
  EXPECT_EQ(decoded.y(), obj.y());
  EXPECT_EQ(decoded.size(), obj.size());
  EXPECT_EQ(decoded.GetLayerValue(), obj.GetLayerValue());
}

TEST(RoomObjectEncodingTest, Type1MaxValues) {
  // Test maximum valid values for Type1
  // Constraints:
  // - ID < 0xF8 (b3 >= 0xF8 triggers Type3 detection)
  // - X < 63 OR Size < 12 (b1 >= 0xFC triggers Type2 detection)
  // Safe max values: ID=0xF7, X=62, Y=63, Size=15
  RoomObject obj(0xF7, 62, 63, 15, 2);

  auto bytes = obj.EncodeObjectToBytes();
  auto decoded =
      RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 2);

  EXPECT_EQ(decoded.id_, obj.id_);
  EXPECT_EQ(decoded.x(), obj.x());
  EXPECT_EQ(decoded.y(), obj.y());
  EXPECT_EQ(decoded.size(), obj.size());
}

TEST(RoomObjectEncodingTest, Type1MinValues) {
  // Test minimum values for Type1
  RoomObject obj(0x00, 0, 0, 0, 0);

  auto bytes = obj.EncodeObjectToBytes();
  auto decoded =
      RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 0);

  EXPECT_EQ(decoded.id_, obj.id_);
  EXPECT_EQ(decoded.x(), obj.x());
  EXPECT_EQ(decoded.y(), obj.y());
  EXPECT_EQ(decoded.size(), obj.size());
}

TEST(RoomObjectEncodingTest, Type1DifferentSizes) {
  // Test all valid size values (0-15)
  for (int size = 0; size <= 15; size++) {
    RoomObject obj(0x30, 15, 20, size, 1);

    auto bytes = obj.EncodeObjectToBytes();
    auto decoded =
        RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 1);

    EXPECT_EQ(decoded.size(), size) << "Failed for size " << size;
  }
}

TEST(RoomObjectEncodingTest, Type1RealWorldExample1) {
  // Example from actual ROM: Wall object
  // Bytes: 0x28 0x50 0x10
  // Expected: X=10, Y=20, Size=0, ID=0x10

  auto decoded = RoomObject::DecodeObjectFromBytes(0x28, 0x50, 0x10, 0);

  EXPECT_EQ(decoded.x(), 10);
  EXPECT_EQ(decoded.y(), 20);
  EXPECT_EQ(decoded.size(), 0);
  EXPECT_EQ(decoded.id_, 0x10);
}

TEST(RoomObjectEncodingTest, Type1RealWorldExample2) {
  // Example: Ceiling object with size
  // Correct bytes for X=10, Y=20, Size=3, ID=0x00: 0x28 0x53 0x00

  auto decoded = RoomObject::DecodeObjectFromBytes(0x28, 0x53, 0x00, 0);

  EXPECT_EQ(decoded.x(), 10);
  EXPECT_EQ(decoded.y(), 20);
  EXPECT_EQ(decoded.size(), 3);
  EXPECT_EQ(decoded.id_, 0x00);
}

// ============================================================================
// Type 2 Object Encoding/Decoding Tests
// ============================================================================

TEST(RoomObjectEncodingTest, Type2EncodeDecodeBasic) {
  // Type2: 111111xx xxxxyyyy yyiiiiii
  // Example: Object ID 0x125, position (15, 30), size ignored, layer 1

  RoomObject obj(0x125, 15, 30, 0, 1);

  // Encode
  auto bytes = obj.EncodeObjectToBytes();

  // Verify b1 starts with 0xFC
  EXPECT_GE(bytes.b1, 0xFC);

  // Decode
  auto decoded =
      RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 1);

  // Verify
  EXPECT_EQ(decoded.id_, obj.id_);
  EXPECT_EQ(decoded.x(), obj.x());
  EXPECT_EQ(decoded.y(), obj.y());
  EXPECT_EQ(decoded.GetLayerValue(), obj.GetLayerValue());
}

TEST(RoomObjectEncodingTest, Type2MaxValues) {
  // Type2 allows larger position range, but has constraints:
  // When Y=63 and ID=0x13F, b3 becomes 0xFF >= 0xF8, triggering Type3 detection
  // Safe max: X=63, Y=59, ID=0x13F (b3 = ((59&0x03)<<6)|(0x3F) = 0xFF still!)
  // Even safer: X=63, Y=63, ID=0x11F (b3 = (0xC0|0x1F) = 0xDF < 0xF8)
  RoomObject obj(0x11F, 63, 63, 0, 2);

  auto bytes = obj.EncodeObjectToBytes();
  auto decoded =
      RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 2);

  EXPECT_EQ(decoded.id_, obj.id_);
  EXPECT_EQ(decoded.x(), obj.x());
  EXPECT_EQ(decoded.y(), obj.y());
}

TEST(RoomObjectEncodingTest, Type2RealWorldExample) {
  // Example: Large brazier (object 0x11C)
  // Position (8, 12)

  RoomObject obj(0x11C, 8, 12, 0, 0);

  auto bytes = obj.EncodeObjectToBytes();
  auto decoded =
      RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 0);

  EXPECT_EQ(decoded.id_, 0x11C);
  EXPECT_EQ(decoded.x(), 8);
  EXPECT_EQ(decoded.y(), 12);
}

// ============================================================================
// Type 3 Object Encoding/Decoding Tests
// ============================================================================

TEST(RoomObjectEncodingTest, Type3EncodeDecodeChest) {
  // Type3: xxxxxxii yyyyyyii 11111iii
  // Example: Small chest (0xF99), position (5, 10)

  RoomObject obj(0xF99, 5, 10, 0, 0);

  // Encode
  auto bytes = obj.EncodeObjectToBytes();

  // Verify b3 >= 0xF8
  EXPECT_GE(bytes.b3, 0xF8);

  // Decode
  auto decoded =
      RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 0);

  // Verify
  EXPECT_EQ(decoded.id_, obj.id_);
  EXPECT_EQ(decoded.x(), obj.x());
  EXPECT_EQ(decoded.y(), obj.y());
}

TEST(RoomObjectEncodingTest, Type3EncodeDcodeBigChest) {
  // Example: Big chest (0xFB1), position (15, 20)

  RoomObject obj(0xFB1, 15, 20, 0, 1);

  auto bytes = obj.EncodeObjectToBytes();
  auto decoded =
      RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 1);

  EXPECT_EQ(decoded.id_, 0xFB1);
  EXPECT_EQ(decoded.x(), 15);
  EXPECT_EQ(decoded.y(), 20);
}

TEST(RoomObjectEncodingTest, Type3RealWorldExample) {
  // Example from ROM: Chest at position (10, 15)
  // Correct bytes for ID 0xF99: 0x29 0x3E 0xF9

  auto decoded = RoomObject::DecodeObjectFromBytes(0x29, 0x3E, 0xF9, 0);

  // Expected: X=10, Y=15, ID=0xF99 (small chest)
  EXPECT_EQ(decoded.x(), 10);
  EXPECT_EQ(decoded.y(), 15);
  EXPECT_EQ(decoded.id_, 0xF99);
}

// ============================================================================
// Edge Cases and Special Values
// ============================================================================

TEST(RoomObjectEncodingTest, LayerPreservation) {
  // Test that layer information is preserved through encode/decode
  for (uint8_t layer = 0; layer <= 2; layer++) {
    RoomObject obj(0x42, 10, 20, 3, layer);

    auto bytes = obj.EncodeObjectToBytes();
    auto decoded =
        RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, layer);

    EXPECT_EQ(decoded.GetLayerValue(), layer)
        << "Failed for layer " << (int)layer;
  }
}

TEST(RoomObjectEncodingTest, BoundaryBetweenTypes) {
  // Test boundary values between object types
  // NOTE: Type1 can only go up to ID 0xF7 (b3 >= 0xF8 triggers Type3)

  // Last safe Type1 object
  RoomObject type1(0xF7, 10, 20, 3, 0);
  auto bytes1 = type1.EncodeObjectToBytes();
  auto decoded1 =
      RoomObject::DecodeObjectFromBytes(bytes1.b1, bytes1.b2, bytes1.b3, 0);
  EXPECT_EQ(decoded1.id_, 0xF7);

  // First Type2 object
  RoomObject type2(0x100, 10, 20, 0, 0);
  auto bytes2 = type2.EncodeObjectToBytes();
  auto decoded2 =
      RoomObject::DecodeObjectFromBytes(bytes2.b1, bytes2.b2, bytes2.b3, 0);
  EXPECT_EQ(decoded2.id_, 0x100);

  // Last Type2 object
  RoomObject type2_last(0x13F, 10, 20, 0, 0);
  auto bytes2_last = type2_last.EncodeObjectToBytes();
  auto decoded2_last = RoomObject::DecodeObjectFromBytes(
      bytes2_last.b1, bytes2_last.b2, bytes2_last.b3, 0);
  EXPECT_EQ(decoded2_last.id_, 0x13F);

  // Type3 objects (start at 0xF80)
  RoomObject type3(0xF99, 10, 20, 0, 0);
  auto bytes3 = type3.EncodeObjectToBytes();
  auto decoded3 =
      RoomObject::DecodeObjectFromBytes(bytes3.b1, bytes3.b2, bytes3.b3, 0);
  EXPECT_EQ(decoded3.id_, 0xF99);
}

TEST(RoomObjectEncodingTest, ZeroPosition) {
  // Test objects at position (0, 0)
  RoomObject type1(0x10, 0, 0, 0, 0);
  auto bytes1 = type1.EncodeObjectToBytes();
  auto decoded1 =
      RoomObject::DecodeObjectFromBytes(bytes1.b1, bytes1.b2, bytes1.b3, 0);
  EXPECT_EQ(decoded1.x(), 0);
  EXPECT_EQ(decoded1.y(), 0);

  RoomObject type2(0x110, 0, 0, 0, 0);
  auto bytes2 = type2.EncodeObjectToBytes();
  auto decoded2 =
      RoomObject::DecodeObjectFromBytes(bytes2.b1, bytes2.b2, bytes2.b3, 0);
  EXPECT_EQ(decoded2.x(), 0);
  EXPECT_EQ(decoded2.y(), 0);
}

// ============================================================================
// Batch Tests with Multiple Objects
// ============================================================================

TEST(RoomObjectEncodingTest, MultipleObjectsRoundTrip) {
  // Test encoding/decoding a batch of different objects
  std::vector<RoomObject> objects;

  // Add various objects
  objects.emplace_back(0x10, 5, 10, 2, 0);    // Type1
  objects.emplace_back(0x42, 15, 20, 5, 1);   // Type1
  objects.emplace_back(0x110, 8, 12, 0, 0);   // Type2
  objects.emplace_back(0x125, 25, 30, 0, 1);  // Type2
  objects.emplace_back(0xF99, 10, 15, 0, 0);  // Type3 (chest)
  objects.emplace_back(0xFB1, 20, 25, 0, 2);  // Type3 (big chest)

  for (size_t i = 0; i < objects.size(); i++) {
    auto& obj = objects[i];
    auto bytes = obj.EncodeObjectToBytes();
    auto decoded = RoomObject::DecodeObjectFromBytes(
        bytes.b1, bytes.b2, bytes.b3, obj.GetLayerValue());

    EXPECT_EQ(decoded.id_, obj.id_) << "Failed at index " << i;
    EXPECT_EQ(decoded.x(), obj.x()) << "Failed at index " << i;
    EXPECT_EQ(decoded.y(), obj.y()) << "Failed at index " << i;
    if (obj.id_ < 0x100) {  // Type1 objects have size
      EXPECT_EQ(decoded.size(), obj.size()) << "Failed at index " << i;
    }
  }
}

}  // namespace
}  // namespace zelda3
}  // namespace yaze
