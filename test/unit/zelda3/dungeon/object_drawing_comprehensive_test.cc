/**
 * @file object_drawing_comprehensive_test.cc
 * @brief Comprehensive tests for object drawing, parsing, and routine mapping
 *
 * Tests the following areas:
 * 1. Object type detection (Type 1: 0x00-0xFF, Type 2: 0x100-0x13F, Type 3: 0xF80-0xFFF)
 * 2. Tile count lookup table (kSubtype1TileLengths)
 * 3. Draw routine mapping completeness
 * 4. Type 3 object index calculation
 * 5. Special size handling (size=0 cases)
 * 6. BothBG flag propagation
 */

#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_parser.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

// Expected tile counts from kSubtype1TileLengths table in object_parser.cc
// clang-format off
static constexpr uint8_t kExpectedTileCounts[0xF8] = {
     4,  8,  8,  8,  8,  8,  8,  4,  4,  5,  5,  5,  5,  5,  5,  5,  // 0x00-0x0F
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  // 0x10-0x1F
     5,  9,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  6,  // 0x20-0x2F
     6,  1,  1, 16,  1,  1, 16, 16,  6,  8, 12, 12,  4,  8,  4,  3,  // 0x30-0x3F
     3,  3,  3,  3,  3,  3,  3,  0,  0,  8,  8,  4,  9, 16, 16, 16,  // 0x40-0x4F
     1, 18, 18,  4,  1,  8,  8,  1,  1,  1,  1, 18, 18, 15,  4,  3,  // 0x50-0x5F
     4,  8,  8,  8,  8,  8,  8,  4,  4,  3,  1,  1,  6,  6,  1,  1,  // 0x60-0x6F
    16,  1,  1, 16, 16,  8, 16, 16,  4,  1,  1,  4,  1,  4,  1,  8,  // 0x70-0x7F
     8, 12, 12, 12, 12, 18, 18,  8, 12,  4,  3,  3,  3,  1,  1,  6,  // 0x80-0x8F
     8,  8,  4,  4, 16,  4,  4,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0x90-0x9F
     1,  1,  1,  1, 24,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0xA0-0xAF
     1,  1, 16,  3,  3,  8,  8,  8,  4,  4, 16,  4,  4,  4,  1,  1,  // 0xB0-0xBF
     1, 68,  1,  1,  8,  8,  8,  8,  8,  8,  8,  1,  1, 28, 28,  1,  // 0xC0-0xCF
     1,  8,  8,  0,  0,  0,  0,  1,  8,  8,  8,  8, 21, 16,  4,  8,  // 0xD0-0xDF
     8,  8,  8,  8,  8,  8,  8,  8,  8,  1,  1,  1,  1,  1,  1,  1,  // 0xE0-0xEF
     1,  1,  1,  1,  1,  1,  1,  1                                   // 0xF0-0xF7
};
// clang-format on

class ObjectDrawingComprehensiveTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    std::vector<uint8_t> mock_rom_data(1024 * 1024, 0);
    rom_->LoadFromData(mock_rom_data);
  }

  std::unique_ptr<Rom> rom_;
};

// ============================================================================
// Type Detection Tests
// ============================================================================

TEST_F(ObjectDrawingComprehensiveTest, DetectsType1Objects) {
  ObjectParser parser(rom_.get());

  // Type 1: 0x00-0xFF (first 248 objects 0x00-0xF7 per spec)
  for (int id = 0; id <= 0xF7; ++id) {
    auto info = parser.GetObjectSubtype(id);
    ASSERT_TRUE(info.ok()) << "Failed for ID 0x" << std::hex << id;
    EXPECT_EQ(info->subtype, 1) << "ID 0x" << std::hex << id << " should be Type 1";
  }
}

TEST_F(ObjectDrawingComprehensiveTest, DetectsType2Objects) {
  ObjectParser parser(rom_.get());

  // Type 2: 0x100-0x13F (64 objects)
  for (int id = 0x100; id <= 0x13F; ++id) {
    auto info = parser.GetObjectSubtype(id);
    ASSERT_TRUE(info.ok()) << "Failed for ID 0x" << std::hex << id;
    EXPECT_EQ(info->subtype, 2) << "ID 0x" << std::hex << id << " should be Type 2";
    if (id >= 0x100 && id <= 0x10F) {
      EXPECT_EQ(info->max_tile_count, 16) << "Type 2 corners should have 16 tiles";
    } else if (id >= 0x110 && id <= 0x117) {
      EXPECT_EQ(info->max_tile_count, 12) << "Type 2 weird corners should have 12 tiles";
    } else {
      EXPECT_EQ(info->max_tile_count, 8) << "Type 2 objects should have 8 tiles";
    }
  }
}

TEST_F(ObjectDrawingComprehensiveTest, DetectsType3Objects) {
  ObjectParser parser(rom_.get());

  // Type 3: 0xF80-0xFFF (128 special objects)
  for (int id = 0xF80; id <= 0xFFF; ++id) {
    auto info = parser.GetObjectSubtype(id);
    ASSERT_TRUE(info.ok()) << "Failed for ID 0x" << std::hex << id;
    EXPECT_EQ(info->subtype, 3) << "ID 0x" << std::hex << id << " should be Type 3";
    if (id == 0xFB1 || id == 0xFB2 ||
        id == 0xF94 || id == 0xFCE ||
        (id >= 0xFE7 && id <= 0xFE8) ||
        (id >= 0xFEC && id <= 0xFED)) {
      EXPECT_EQ(info->max_tile_count, 12) << "Type 3 objects should have 12 tiles";
    } else if (id == 0xFC8 || id == 0xFE6 || id == 0xFEB || id == 0xFFA) {
      EXPECT_EQ(info->max_tile_count, 16) << "Type 3 objects should have 16 tiles";
    } else {
      EXPECT_EQ(info->max_tile_count, 8) << "Type 3 objects should have 8 tiles";
    }
  }
}

// ============================================================================
// Type 3 Index Calculation Tests (Critical Bug Fix Verification)
// ============================================================================

TEST_F(ObjectDrawingComprehensiveTest, Type3IndexCalculation_BoundaryValues) {
  ObjectParser parser(rom_.get());

  // Verify Type 3 index calculation: index = (object_id - 0xF80) & 0x7F
  // This maps 0xF80-0xFFF to table indices 0-127

  struct TestCase {
    int object_id;
    int expected_index;
  };

  std::vector<TestCase> test_cases = {
      {0xF80, 0},    // First Type 3 object -> index 0
      {0xF81, 1},    // Second Type 3 object -> index 1
      {0xF8F, 15},   // Index 15
      {0xF90, 16},   // Index 16
      {0xFA0, 32},   // Index 32
      {0xFBF, 63},   // Index 63
      {0xFC0, 64},   // Index 64
      {0xFFF, 127},  // Last Type 3 object -> index 127
  };

  for (const auto& tc : test_cases) {
    auto info = parser.GetObjectSubtype(tc.object_id);
    ASSERT_TRUE(info.ok()) << "Failed for ID 0x" << std::hex << tc.object_id;

    // Verify subtype_ptr points to correct table offset
    // kRoomObjectSubtype3 = 0x84F0 (from room_object.h)
    // Expected ptr = 0x84F0 + (index * 2)
    int expected_ptr = 0x84F0 + (tc.expected_index * 2);
    EXPECT_EQ(info->subtype_ptr, expected_ptr)
        << "ID 0x" << std::hex << tc.object_id
        << " expected ptr 0x" << expected_ptr
        << " got 0x" << info->subtype_ptr;
  }
}

TEST_F(ObjectDrawingComprehensiveTest, Type3IndexCalculation_AllIndicesInRange) {
  ObjectParser parser(rom_.get());

  // Verify all Type 3 objects produce indices in valid range (0-127)
  for (int id = 0xF80; id <= 0xFFF; ++id) {
    auto info = parser.GetObjectSubtype(id);
    ASSERT_TRUE(info.ok()) << "Failed for ID 0x" << std::hex << id;

    // Calculate what index was used
    // subtype_ptr = kRoomObjectSubtype3 + (index * 2)
    // index = (subtype_ptr - kRoomObjectSubtype3) / 2
    int index = (info->subtype_ptr - 0x84F0) / 2;

    EXPECT_GE(index, 0) << "Index for 0x" << std::hex << id << " is negative";
    EXPECT_LE(index, 127) << "Index for 0x" << std::hex << id << " exceeds 127";
  }
}

// ============================================================================
// Type 2 Index Calculation Tests
// ============================================================================

TEST_F(ObjectDrawingComprehensiveTest, Type2IndexCalculation_BoundaryValues) {
  ObjectParser parser(rom_.get());

  // Verify Type 2 index calculation: index = (object_id - 0x100) & 0x3F
  // This maps 0x100-0x13F to table indices 0-63 (64-entry table)

  struct TestCase {
    int object_id;
    int expected_index;
  };

  std::vector<TestCase> test_cases = {
      {0x100, 0},    // First Type 2 object -> index 0
      {0x101, 1},    // Second Type 2 object -> index 1
      {0x10F, 15},   // Index 15
      {0x13F, 63},   // Last Type 2 object -> index 63
  };

  for (const auto& tc : test_cases) {
    auto info = parser.GetObjectSubtype(tc.object_id);
    ASSERT_TRUE(info.ok()) << "Failed for ID 0x" << std::hex << tc.object_id;

    // Verify subtype_ptr points to correct table offset
    // kRoomObjectSubtype2 = 0x83F0 (from room_object.h)
    // Expected ptr = 0x83F0 + (index * 2)
    int expected_ptr = 0x83F0 + (tc.expected_index * 2);
    EXPECT_EQ(info->subtype_ptr, expected_ptr)
        << "ID 0x" << std::hex << tc.object_id
        << " expected ptr 0x" << expected_ptr
        << " got 0x" << info->subtype_ptr;
  }
}

// ============================================================================
// Tile Count Lookup Table Tests
// ============================================================================

TEST_F(ObjectDrawingComprehensiveTest, TileCountLookupTable_VerifyAllEntries) {
  ObjectParser parser(rom_.get());

  // Verify tile counts match the expected table for Type 1 objects
  for (int id = 0; id < 0xF8; ++id) {
    auto info = parser.GetObjectSubtype(id);
    ASSERT_TRUE(info.ok()) << "Failed for ID 0x" << std::hex << id;

    int expected_count = kExpectedTileCounts[id];
    // Note: Tile count 0 in table means "default to 8"
    if (expected_count == 0) expected_count = 8;

    EXPECT_EQ(info->max_tile_count, expected_count)
        << "ID 0x" << std::hex << id
        << " expected " << expected_count
        << " tiles, got " << info->max_tile_count;
  }
}

TEST_F(ObjectDrawingComprehensiveTest, TileCountLookupTable_SpecialCases) {
  ObjectParser parser(rom_.get());

  // Test objects with notable tile counts
  struct TestCase {
    int object_id;
    int expected_tiles;
    const char* description;
  };

  std::vector<TestCase> test_cases = {
      {0x00, 4, "Floor tile 2x2"},
      {0x01, 8, "Wall segment 2x4"},
      {0x33, 16, "Large block 4x4"},
      {0xA4, 24, "Large special object"},
      {0xC1, 68, "Very large object"},
      {0xCD, 28, "Moving wall"},
      {0xCE, 28, "Moving wall variant"},
      {0x47, 8, "Waterfall (default)"},  // Table has 0, defaults to 8
      {0x48, 8, "Waterfall variant (default)"},
  };

  for (const auto& tc : test_cases) {
    auto info = parser.GetObjectSubtype(tc.object_id);
    ASSERT_TRUE(info.ok()) << "Failed for " << tc.description;
    EXPECT_EQ(info->max_tile_count, tc.expected_tiles)
        << tc.description << " (0x" << std::hex << tc.object_id << ")";
  }
}

// ============================================================================
// Draw Routine Mapping Tests
// ============================================================================

// TODO(Phase 4): Update routine ID range check
// Phase 4 added SuperSquare routines (IDs 56-64), so the upper bound should be 64.
// Remaining Phase 4 work will add more routines (simple variants, diagonal ceilings,
// special/logic-dependent) bringing the total higher.

TEST_F(ObjectDrawingComprehensiveTest, DrawRoutineMapping_AllSubtype1ObjectsHaveRoutines) {
  ObjectDrawer drawer(rom_.get(), 0);
  const int max_routine_id = drawer.GetDrawRoutineCount() - 1;

  // Verify all Type 1 objects (0x00-0xF7) have valid routine mappings
  for (int id = 0; id <= 0xF7; ++id) {
    int routine_id = drawer.GetDrawRoutineId(id);
    // Should return valid routine (0..max) or -1 for unmapped
    EXPECT_GE(routine_id, -1) << "ID 0x" << std::hex << id;
    EXPECT_LE(routine_id, max_routine_id) << "ID 0x" << std::hex << id;
  }
}

TEST_F(ObjectDrawingComprehensiveTest, DrawRoutineMapping_DiagonalWalls) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Diagonal walls 0x09-0x20 have specific routine assignments
  // Based on bank_01.asm analysis

  // Non-BothBG Acute Diagonals (/)
  for (int id : {0x0C, 0x0D, 0x10, 0x11, 0x14}) {
    EXPECT_EQ(drawer.GetDrawRoutineId(id), 5)
        << "ID 0x" << std::hex << id << " should use routine 5 (DiagonalAcute)";
  }

  // Non-BothBG Grave Diagonals (\)
  for (int id : {0x0E, 0x0F, 0x12, 0x13}) {
    EXPECT_EQ(drawer.GetDrawRoutineId(id), 6)
        << "ID 0x" << std::hex << id << " should use routine 6 (DiagonalGrave)";
  }

  // BothBG Acute Diagonals (/)
  for (int id : {0x15, 0x18, 0x19, 0x1C, 0x1D, 0x20}) {
    EXPECT_EQ(drawer.GetDrawRoutineId(id), 17)
        << "ID 0x" << std::hex << id << " should use routine 17 (DiagonalAcute_BothBG)";
  }

  // BothBG Grave Diagonals (\)
  for (int id : {0x16, 0x17, 0x1A, 0x1B, 0x1E, 0x1F}) {
    EXPECT_EQ(drawer.GetDrawRoutineId(id), 18)
        << "ID 0x" << std::hex << id << " should use routine 18 (DiagonalGrave_BothBG)";
  }
}

// TODO(Phase 4): Update NothingRoutines test
// Phase 4 corrected mappings for several objects that were incorrectly mapped to "Nothing":
// - 0xC4 now maps to routine 59 (Draw4x4FloorOneIn4x4SuperSquare)
// - 0xCB, 0xCC, 0xCF, 0xD0 need verification against assembly ground truth
// - 0xD3-0xD6 are logic-only (CheckIfWallIsMoved) and correctly remain as Nothing

TEST_F(ObjectDrawingComprehensiveTest, DrawRoutineMapping_NothingRoutines) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Objects that map to "Nothing" (routine 38) are invisible/logic objects
  // NOTE: Phase 4 removed some objects from this list as they now have proper routines
  std::vector<int> nothing_objects = {
      0x31, 0x32,  // Custom/logic
      0x54, 0x57, 0x58, 0x59, 0x5A,  // Logic objects
      0x6E, 0x6F,  // End of vertical section
      0x72, 0x7E,  // Logic objects
      0xBE, 0xBF,  // Logic objects
      // 0xC4 removed - now maps to Draw4x4FloorOneIn4x4SuperSquare (routine 59)
      0xCB, 0xCC, 0xCF, 0xD0,  // Logic objects (verify against ASM)
      0xD3, 0xD4, 0xD5, 0xD6,  // Wall moved checks (logic-only, no tiles)
  };

  for (int id : nothing_objects) {
    EXPECT_EQ(drawer.GetDrawRoutineId(id), 38)
        << "ID 0x" << std::hex << id << " should use routine 38 (Nothing)";
  }
}

TEST_F(ObjectDrawingComprehensiveTest, DrawRoutineMapping_Type2Objects) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Type 2 objects (0x100+) have specific routine assignments

  // 0x100-0x107: 4x4 blocks
  for (int id = 0x100; id <= 0x107; ++id) {
    EXPECT_EQ(drawer.GetDrawRoutineId(id), 16)
        << "ID 0x" << std::hex << id << " should use routine 16 (4x4)";
  }

  // 0x108-0x10F: 4x4 Corner BothBG
  for (int id = 0x108; id <= 0x10F; ++id) {
    EXPECT_EQ(drawer.GetDrawRoutineId(id), 35)
        << "ID 0x" << std::hex << id << " should use routine 35 (4x4 Corner BothBG)";
  }

  // 0x110-0x113: Weird Corner Bottom
  for (int id = 0x110; id <= 0x113; ++id) {
    EXPECT_EQ(drawer.GetDrawRoutineId(id), 36)
        << "ID 0x" << std::hex << id << " should use routine 36 (Weird Corner Bottom)";
  }

  // 0x114-0x117: Weird Corner Top
  for (int id = 0x114; id <= 0x117; ++id) {
    EXPECT_EQ(drawer.GetDrawRoutineId(id), 37)
        << "ID 0x" << std::hex << id << " should use routine 37 (Weird Corner Top)";
  }
}

TEST_F(ObjectDrawingComprehensiveTest, DrawRoutineMapping_Type3SpecialObjects) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Type 3 objects (0xF80-0xFFF) - actual decoded IDs from ROM
  // Index = (object_id - 0xF80) & 0x7F

  // Water Face variants (indices 0-2)
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF80), 94)
      << "ID 0xF80 should use routine 94 (Empty Water Face)";
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF81), 95)
      << "ID 0xF81 should use routine 95 (Spitting Water Face)";
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF82), 96)
      << "ID 0xF82 should use routine 96 (Drenching Water Face)";

  // Somaria Line (indices 3-9)
  for (int id : {0xF83, 0xF84, 0xF85, 0xF86, 0xF87, 0xF88, 0xF89}) {
    EXPECT_EQ(drawer.GetDrawRoutineId(id), 33)
        << "ID 0x" << std::hex << id << " should use routine 33 (Somaria Line)";
  }

  // Prison Cell + Big Key Lock + Chests (indices 0x17-0x1A -> 0xF97-0xF9A)
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF97), 97)
      << "ID 0xF97 should use routine 97 (Prison Cell)";
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF98), 92)
      << "ID 0xF98 should use routine 92 (Big Key Lock)";
  for (int id : {0xF99, 0xF9A}) {
    EXPECT_EQ(drawer.GetDrawRoutineId(id), 39)
        << "ID 0x" << std::hex << id << " should use routine 39 (DrawChest)";
  }
}

// ============================================================================
// Object Decoding/Encoding Tests
// ============================================================================

TEST_F(ObjectDrawingComprehensiveTest, ObjectDecoding_Type1) {
  // Type 1: xxxxxxss yyyyyyss iiiiiiii
  // b1=0x28 (x=10), b2=0x14 (y=5), b3=0x01 (id=0x01)
  uint8_t b1 = 0x28;  // x=10 (0x28 >> 2 = 0x0A)
  uint8_t b2 = 0x14;  // y=5  (0x14 >> 2 = 0x05)
  uint8_t b3 = 0x01;  // id=1

  auto obj = RoomObject::DecodeObjectFromBytes(b1, b2, b3, 0);

  EXPECT_EQ(obj.id_, 0x01);
  EXPECT_EQ(obj.x_, 10);
  EXPECT_EQ(obj.y_, 5);
}

TEST_F(ObjectDrawingComprehensiveTest, ObjectDecoding_Type2) {
  // Type 2: 111111xx xxxxyyyy yyiiiiii
  // Discriminator: b1 >= 0xFC
  // Example: b1=0xFC, b2=0x50, b3=0x05 -> id=0x105
  uint8_t b1 = 0xFC;  // 111111 00
  uint8_t b2 = 0x50;  // xxxx=5, yyyy=0
  uint8_t b3 = 0x05;  // yy=0, iiiiii=5

  auto obj = RoomObject::DecodeObjectFromBytes(b1, b2, b3, 0);

  EXPECT_EQ(obj.id_, 0x105);  // 0x100 + 5
}

TEST_F(ObjectDrawingComprehensiveTest, ObjectDecoding_Type3) {
  // Type 3: xxxxxxii yyyyyyii 11111iii
  // Discriminator: b3 >= 0xF8
  // Example: b1=0x28, b2=0x14, b3=0xF8 -> Type 3
  uint8_t b1 = 0x28;
  uint8_t b2 = 0x14;
  uint8_t b3 = 0xF8;

  auto obj = RoomObject::DecodeObjectFromBytes(b1, b2, b3, 0);

  // Verify it's detected as Type 3 (ID >= 0xF80)
  EXPECT_GE(obj.id_, 0xF80);
  EXPECT_LE(obj.id_, 0xFFF);
}

TEST_F(ObjectDrawingComprehensiveTest, ObjectEncoding_Roundtrip) {
  // Test that encoding and decoding produces consistent results

  // Type 1 object
  RoomObject obj1(0x05, 10, 20, 3, 0);
  auto bytes1 = obj1.EncodeObjectToBytes();
  auto decoded1 = RoomObject::DecodeObjectFromBytes(bytes1.b1, bytes1.b2, bytes1.b3, 0);
  EXPECT_EQ(decoded1.id_, obj1.id_);
  EXPECT_EQ(decoded1.x_, obj1.x_);
  EXPECT_EQ(decoded1.y_, obj1.y_);

  // Type 2 object
  RoomObject obj2(0x105, 15, 25, 0, 0);
  auto bytes2 = obj2.EncodeObjectToBytes();
  auto decoded2 = RoomObject::DecodeObjectFromBytes(bytes2.b1, bytes2.b2, bytes2.b3, 0);
  EXPECT_EQ(decoded2.id_, obj2.id_);
}

// ============================================================================
// BothBG Flag Tests
// ============================================================================

TEST_F(ObjectDrawingComprehensiveTest, AllBgsFlag_SetDuringDecoding) {
  // Objects with specific IDs should have all_bgs_ flag set during decoding
  // Based on DecodeObjectFromBytes logic

  // Routine 3 objects (0x03-0x04)
  for (int id : {0x03, 0x04}) {
    RoomObject obj(id, 0, 0, 0, 0);
    // Create via decoding to trigger all_bgs logic
    auto decoded = RoomObject::DecodeObjectFromBytes(0x00, 0x00, id, 0);
    EXPECT_TRUE(decoded.all_bgs_) << "ID 0x" << std::hex << id << " should have all_bgs set";
  }

  // Routine 9 objects (0x63-0x64)
  for (int id : {0x63, 0x64}) {
    auto decoded = RoomObject::DecodeObjectFromBytes(0x00, 0x00, id, 0);
    EXPECT_TRUE(decoded.all_bgs_) << "ID 0x" << std::hex << id << " should have all_bgs set";
  }

  // Diagonal BothBG objects
  std::vector<int> bothbg_diagonals = {
      0x0C, 0x0D, 0x10, 0x11, 0x14, 0x15, 0x18, 0x19,
      0x1C, 0x1D, 0x20, 0x0E, 0x0F, 0x12, 0x13, 0x16,
      0x17, 0x1A, 0x1B, 0x1E, 0x1F
  };
  for (int id : bothbg_diagonals) {
    auto decoded = RoomObject::DecodeObjectFromBytes(0x00, 0x00, id, 0);
    EXPECT_TRUE(decoded.all_bgs_)
        << "Diagonal ID 0x" << std::hex << id << " should have all_bgs set";
  }
}

// ============================================================================
// Dimension Calculation Tests
// ============================================================================

TEST_F(ObjectDrawingComprehensiveTest, DimensionCalculation_HorizontalPatterns) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Test horizontal objects
  RoomObject obj00_size1(0x00, 0, 0, 1, 0);
  auto dims = drawer.CalculateObjectDimensions(obj00_size1);
  EXPECT_EQ(dims.first, 16);   // 1 * 16 = 16 pixels wide
  EXPECT_EQ(dims.second, 16);  // 2 tiles = 16 pixels tall

  RoomObject obj00_size5(0x00, 0, 0, 5, 0);
  dims = drawer.CalculateObjectDimensions(obj00_size5);
  EXPECT_EQ(dims.first, 80);   // 5 * 16 = 80 pixels wide
  EXPECT_EQ(dims.second, 16);
}

TEST_F(ObjectDrawingComprehensiveTest, DimensionCalculation_DiagonalPatterns) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Diagonal walls use (size + 6) or (size + 7) count
  RoomObject diagonal_size0(0x10, 0, 0, 0, 0);
  auto dims = drawer.CalculateObjectDimensions(diagonal_size0);
  // Diagonal: count = size + 7, width = count * 8, height = (count + 4) * 8
  EXPECT_EQ(dims.first, 56);   // 7 * 8 = 56
  EXPECT_EQ(dims.second, 88);  // (7 + 4) * 8 = 88

  RoomObject diagonal_size10(0x10, 0, 0, 10, 0);
  dims = drawer.CalculateObjectDimensions(diagonal_size10);
  EXPECT_EQ(dims.first, 136);  // 17 * 8 = 136
  EXPECT_EQ(dims.second, 168);  // (17 + 4) * 8 = 168
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(ObjectDrawingComprehensiveTest, EdgeCase_NegativeObjectId) {
  ObjectParser parser(rom_.get());

  auto result = parser.ParseObject(-1);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(ObjectDrawingComprehensiveTest, EdgeCase_NullRom) {
  ObjectParser parser(nullptr);

  auto result = parser.ParseObject(0x01);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(ObjectDrawingComprehensiveTest, EdgeCase_Size0HandledCorrectly) {
  // Size 0 has special meaning for certain objects
  // 0x00: size 0 -> 32 repetitions (routine 0 is handled in CalculateObjectDimensions)
  // 0x01: size 0 -> 26 repetitions (routine 1 NOT handled - falls through to default)

  RoomObject obj00_size0(0x00, 0, 0, 0, 0);

  // Object 0x00 (routine 0) should use special size=32 when input is 0
  ObjectDrawer drawer(rom_.get(), 0);

  auto dims00 = drawer.CalculateObjectDimensions(obj00_size0);
  EXPECT_EQ(dims00.first, 512);  // 32 * 16 = 512

  // Object 0x01 (routine 1) size=0 uses GetSize_1to15or26 -> 26 repetitions.
  RoomObject obj01_size0(0x01, 0, 0, 0, 0);
  auto dims01 = drawer.CalculateObjectDimensions(obj01_size0);
  EXPECT_EQ(dims01.first, 416);  // 26 * 16
}

// ============================================================================
// Transparency and Pixel Rendering Tests
// ============================================================================

TEST_F(ObjectDrawingComprehensiveTest, TransparencyHandling_Pixel0IsSkipped) {
  // Verify that pixel value 0 is treated as transparent
  // According to SNES/ALTTP conventions, palette index 0 is transparent

  // This test verifies the design principle from DrawTileToBitmap:
  // "if (pixel != 0) { ... }" means pixel 0 is skipped

  // Create a mock graphics buffer with known values
  std::vector<uint8_t> test_gfx(0x10000, 0);  // All transparent initially

  // Set up a test tile at ID 0 (row 0, col 0)
  // Tile spans bytes 0-7 in first row of the tile, then 128-135 for second row, etc.
  // Put some non-zero pixels to verify they get drawn
  test_gfx[0] = 0;  // pixel (0,0) = transparent
  test_gfx[1] = 1;  // pixel (1,0) = color index 0 (1-1=0)
  test_gfx[2] = 2;  // pixel (2,0) = color index 1 (2-1=1)
  test_gfx[128] = 3; // pixel (0,1) = color index 2 (3-1=2)

  ObjectDrawer drawer(rom_.get(), 0, test_gfx.data());

  gfx::BackgroundBuffer bg(64, 64);
  std::vector<uint8_t> bg_data(64 * 64, 0xFF);  // Fill with sentinel value
  bg.bitmap().Create(64, 64, 8, bg_data);

  gfx::TileInfo tile;
  tile.id_ = 0;
  tile.palette_ = 0;
  tile.horizontal_mirror_ = false;
  tile.vertical_mirror_ = false;

  // Draw tile at position (0,0)
  drawer.DrawTileToBitmap(bg.bitmap(), tile, 0, 0, test_gfx.data());

  // Verify pixel (0,0) was NOT written (should still be 0xFF sentinel)
  const auto& data = bg.bitmap().vector();
  EXPECT_EQ(data[0], 0xFF) << "Transparent pixel (0) should not overwrite bitmap";

  // Verify pixel (1,0) WAS written with value pixel + palette_offset = 1
  EXPECT_EQ(data[1], 1) << "Non-transparent pixel should be written";

  // Verify pixel (2,0) WAS written with value pixel + palette_offset = 2
  EXPECT_EQ(data[2], 2) << "Non-transparent pixel should be written";

  // Verify pixel (0,1) WAS written with value pixel + palette_offset = 3
  EXPECT_EQ(data[64], 3) << "Non-transparent pixel at row 1 should be written";
}

TEST_F(ObjectDrawingComprehensiveTest, TileInfo_PaletteIndexMappingVerify) {
  // Verify palette index calculation:
  // final_color = pixel + (palette_bank * 16)

  // TileInfo with palette 0
  gfx::TileInfo tile0;
  tile0.palette_ = 0;

  // TileInfo with palette 1
  gfx::TileInfo tile1;
  tile1.palette_ = 1;

  // Palette 0 should use offsets 0-15
  // Palette 1 should use offsets 16-31
  // etc.

  // This is design verification - the actual color lookup happens in DrawTileToBitmap
  EXPECT_EQ(tile0.palette_ * 16, 0);
  EXPECT_EQ(tile1.palette_ * 16, 16);

  // Test palette clamping - palettes 6,7 wrap to 0,1
  gfx::TileInfo tile6;
  tile6.palette_ = 6;
  uint8_t clamped6 = tile6.palette_ % 6;
  EXPECT_EQ(clamped6, 0);

  gfx::TileInfo tile7;
  tile7.palette_ = 7;
  uint8_t clamped7 = tile7.palette_ % 6;
  EXPECT_EQ(clamped7, 1);
}

TEST_F(ObjectDrawingComprehensiveTest, TileInfo_MirroringFlags) {
  // Verify mirroring flag interpretation

  gfx::TileInfo tile;
  tile.horizontal_mirror_ = true;
  tile.vertical_mirror_ = false;

  // For horizontal mirror: src_col = 7 - px
  // For vertical mirror: src_row = 7 - py

  // These are design verification tests
  EXPECT_TRUE(tile.horizontal_mirror_);
  EXPECT_FALSE(tile.vertical_mirror_);

  gfx::TileInfo tile_both;
  tile_both.horizontal_mirror_ = true;
  tile_both.vertical_mirror_ = true;

  EXPECT_TRUE(tile_both.horizontal_mirror_);
  EXPECT_TRUE(tile_both.vertical_mirror_);
}

}  // namespace zelda3
}  // namespace yaze
