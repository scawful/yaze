#include "gtest/gtest.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room_object.h"
#include "rom/rom.h"

namespace yaze {
namespace zelda3 {

// =============================================================================
// DrawRoutineRegistry Object Mapping Tests (Phase 1)
// =============================================================================

TEST(DrawRoutineRegistryTest, GetRoutineIdForRepresentativeObjects) {
  auto& reg = DrawRoutineRegistry::Get();
  // 0x00 -> routine 0 (Rightwards2x2_1to15or32)
  EXPECT_EQ(reg.GetRoutineIdForObject(0x00), 0);
  // 0x09 -> routine 5 (DiagonalAcute_1to16)
  EXPECT_EQ(reg.GetRoutineIdForObject(0x09), 5);
  // 0x60 -> routine 7 (Downwards2x2_1to15or32)
  EXPECT_EQ(reg.GetRoutineIdForObject(0x60), 7);
  // 0xF9 -> routine 39 (Chest)
  EXPECT_EQ(reg.GetRoutineIdForObject(0xF9), 39);
  // 0xC0 -> routine 56 (4x4BlocksIn4x4SuperSquare)
  EXPECT_EQ(reg.GetRoutineIdForObject(0xC0), 56);
  // Type 2: 0x100 -> routine 16 (Rightwards4x4_1to16)
  EXPECT_EQ(reg.GetRoutineIdForObject(0x100), 16);
  // Type 3: 0xF80 -> routine 94 (EmptyWaterFace)
  EXPECT_EQ(reg.GetRoutineIdForObject(0xF80), 94);
}

TEST(DrawRoutineRegistryTest, UnmappedObjectReturnsNegativeOne) {
  auto& reg = DrawRoutineRegistry::Get();
  EXPECT_EQ(reg.GetRoutineIdForObject(0xF8), -1);
  EXPECT_EQ(reg.GetRoutineIdForObject(0x7FFF), -1);
}

TEST(DrawRoutineRegistryTest, RegistryMatchesObjectDrawer) {
  // Verify the registry returns the same IDs as ObjectDrawer for key objects
  Rom rom;
  std::vector<uint8_t> mock(1024 * 1024, 0);
  rom.LoadFromData(mock);
  ObjectDrawer drawer(&rom, 0);
  auto& reg = DrawRoutineRegistry::Get();

  for (int16_t obj_id : {0x00, 0x09, 0x22, 0x60, 0xA4, 0xC0, 0xF9}) {
    EXPECT_EQ(drawer.GetDrawRoutineId(obj_id),
              reg.GetRoutineIdForObject(obj_id))
        << "Mismatch for object 0x" << std::hex << obj_id;
  }
}

// =============================================================================
// ObjectDimensionTable Tests (Phase 3)
// =============================================================================

class ObjectDimensionTableTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    std::vector<uint8_t> mock_rom_data(1024 * 1024, 0);
    rom_->LoadFromData(mock_rom_data);
    // Reset singleton before each test
    ObjectDimensionTable::Get().Reset();
  }

  void TearDown() override {
    // Reset singleton after each test to avoid affecting other tests
    ObjectDimensionTable::Get().Reset();
  }

  std::unique_ptr<Rom> rom_;
};

TEST_F(ObjectDimensionTableTest, SingletonAccess) {
  auto& table1 = ObjectDimensionTable::Get();
  auto& table2 = ObjectDimensionTable::Get();
  EXPECT_EQ(&table1, &table2);
}

TEST_F(ObjectDimensionTableTest, LoadFromRomSucceeds) {
  auto& table = ObjectDimensionTable::Get();
  auto status = table.LoadFromRom(rom_.get());
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(table.IsLoaded());
}

TEST_F(ObjectDimensionTableTest, LoadFromNullRomFails) {
  auto& table = ObjectDimensionTable::Get();
  auto status = table.LoadFromRom(nullptr);
  EXPECT_FALSE(status.ok());
}

TEST_F(ObjectDimensionTableTest, GetBaseDimensionsReturnsDefaults) {
  auto& table = ObjectDimensionTable::Get();
  table.LoadFromRom(rom_.get());

  // Well-known object should have base dimensions
  auto [w, h] = table.GetBaseDimensions(0x07);
  EXPECT_GT(w, 0);
  EXPECT_GT(h, 0);

  // Unknown objects should fall back to 2x2 defaults
  auto [dw, dh] = table.GetBaseDimensions(0xFFFF);
  EXPECT_EQ(dw, 2);
  EXPECT_EQ(dh, 2);
}

TEST_F(ObjectDimensionTableTest, GetDimensionsAccountsForSize) {
  auto& table = ObjectDimensionTable::Get();
  table.LoadFromRom(rom_.get());

  // Horizontal walls extend with size
  auto [w0, h0] = table.GetDimensions(0x00, 0);
  auto [w5, h5] = table.GetDimensions(0x00, 5);

  // Size 0 uses the 32-tile default, so width should be larger than size 5
  EXPECT_GT(w0, w5);
}

TEST_F(ObjectDimensionTableTest, GetHitTestBoundsReturnsObjectPosition) {
  auto& table = ObjectDimensionTable::Get();
  table.LoadFromRom(rom_.get());

  RoomObject obj(0x00, 10, 20, 0, 0);
  auto [x, y, w, h] = table.GetHitTestBounds(obj);

  EXPECT_EQ(x, 10);
  EXPECT_EQ(y, 20);
  EXPECT_GT(w, 0);
  EXPECT_GT(h, 0);
}

TEST_F(ObjectDimensionTableTest, ChestObjectsHaveFixedSize) {
  auto& table = ObjectDimensionTable::Get();
  table.LoadFromRom(rom_.get());

  // Chests (0xF9, 0xFB) should be 2x2 tiles regardless of size
  auto [w1, h1] = table.GetDimensions(0xF9, 0);
  auto [w2, h2] = table.GetDimensions(0xF9, 5);

  EXPECT_EQ(w1, w2);
  EXPECT_EQ(h1, h2);
}

// =============================================================================
// ObjectDrawer Dimension Tests (Legacy compatibility)
// =============================================================================

class ObjectDimensionsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a mock ROM for testing
    rom_ = std::make_unique<Rom>();
    // Initialize with minimal ROM data for testing
    std::vector<uint8_t> mock_rom_data(1024 * 1024, 0);  // 1MB mock ROM
    rom_->LoadFromData(mock_rom_data);
    // Reset dimension table singleton
    ObjectDimensionTable::Get().Reset();
  }

  void TearDown() override {
    rom_.reset();
    ObjectDimensionTable::Get().Reset();
  }

  std::unique_ptr<Rom> rom_;
};

TEST_F(ObjectDimensionsTest, CalculatesDimensionsForType1Objects) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Test object 0x00 (horizontal floor tile)
  // Routine 0: DrawRightwards2x2_1to15or32
  // Logic: width = size * 16 (where size 0 -> 32)
  
  RoomObject obj00(0x00, 10, 10, 0, 0); // Size 0 -> 32
  // width = 32 * 16 = 512
  auto dims = drawer.CalculateObjectDimensions(obj00);
  EXPECT_EQ(dims.first, 512);
  EXPECT_EQ(dims.second, 16);

  RoomObject obj00_size1(0x00, 10, 10, 1, 0); // Size 1
  // width = 1 * 16 = 16
  dims = drawer.CalculateObjectDimensions(obj00_size1);
  EXPECT_EQ(dims.first, 16);
  EXPECT_EQ(dims.second, 16);
}

TEST_F(ObjectDimensionsTest, CalculatesDimensionsForDiagonalWalls) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Test object 0x10 (Diagonal Wall /)
  // Routine 5: DrawDiagonalAcute_1to16
  // Logic: width = (size + 7) * 8
  
  RoomObject obj10(0x10, 10, 10, 0, 0); // Size 0
  // width = (0 + 7) * 8 = 56
  auto dims = drawer.CalculateObjectDimensions(obj10);
  EXPECT_EQ(dims.first, 56);
  EXPECT_EQ(dims.second, 88);

  RoomObject obj10_size10(0x10, 10, 10, 10, 0); // Size 10
  // width = (10 + 7) * 8 = 136
  dims = drawer.CalculateObjectDimensions(obj10_size10);
  EXPECT_EQ(dims.first, 136);
  EXPECT_EQ(dims.second, 168);
}

TEST_F(ObjectDimensionsTest, CalculatesDimensionsForType2Corners) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Test object 0x40 (Type 2 Corner)
  // Routine 22: Edge 1x1
  // Width 24, Height 8 (corner + middle + end)
  RoomObject obj40(0x40, 10, 10, 0, 0);
  auto dims = drawer.CalculateObjectDimensions(obj40);
  EXPECT_EQ(dims.first, 24);
  EXPECT_EQ(dims.second, 8);
}

TEST_F(ObjectDimensionsTest, CalculatesDimensionsForType3Objects) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Test object 0x200 (Water Face)
  // Routine 34: Water Face (2x2 tiles = 16x16 pixels)
  // Currently falls back to default logic or specific if added.
  // If not added to switch, default is 8 + size*4.
  // Water Face size usually 0?
  
  RoomObject obj200(0x200, 10, 10, 0, 0);
  auto dims = drawer.CalculateObjectDimensions(obj200);
  // If unhandled, check fallback behavior or add case.
  // For now, just ensure it returns something reasonable > 0
  EXPECT_GT(dims.first, 0);
  EXPECT_GT(dims.second, 0);
}

TEST_F(ObjectDimensionsTest, CalculatesDimensionsForSomariaLine) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Test object 0x203 (Somaria Line)
  // NOTE: Subtype 3 objects (0x200+) are not yet mapped to draw routines.
  // Falls back to default dimension calculation: (size + 1) * 8
  // With size 0: width = 8, height = 8

  RoomObject obj203(0x203, 10, 10, 0, 0);
  auto dims = drawer.CalculateObjectDimensions(obj203);
  EXPECT_EQ(dims.first, 8);   // Default fallback for unmapped objects
  EXPECT_EQ(dims.second, 8);
}

}  // namespace zelda3
}  // namespace yaze
