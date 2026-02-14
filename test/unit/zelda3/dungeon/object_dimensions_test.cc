#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/geometry/object_geometry.h"
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

  // Chests (0xF9, 0xFB) should be 4x4 tiles regardless of size
  auto [w1, h1] = table.GetDimensions(0xF9, 0);
  auto [w2, h2] = table.GetDimensions(0xF9, 5);

  EXPECT_EQ(w1, 4);
  EXPECT_EQ(h1, 4);
  EXPECT_EQ(w2, 4);
  EXPECT_EQ(h2, 4);
}

TEST_F(ObjectDimensionTableTest,
       FocusedScopeSelectionBoundsMatchObjectGeometry) {
  auto& table = ObjectDimensionTable::Get();
  table.LoadFromRom(rom_.get());

  struct ObjectCase {
    int16_t object_id;
    uint8_t size;
    const char* label;
  };

  // Focused validation scope from Oracle workflow:
  // - SuperSquare family (0xC0-0xEF)
  // - Chest family (0xF9-0xFD)
  // - Subtype 2 stairs/furniture representatives
  const ObjectCase cases[] = {
      {0xC0, 0x00, "SuperSquare_4x4_size0"},
      {0xC3, 0x09, "SuperSquare_3x3_size0x09"},
      {0xDE, 0x00, "SuperSquare_spike2x2_size0"},
      {0xF9, 0x00, "Chest_small_size0"},
      {0xFD, 0x0F, "Chest_big_size0x0F"},
      {0x122, 0x00, "Subtype2_bed_size0"},
      {0x123, 0x03, "Subtype2_table_size3"},
      {0x12D, 0x00, "Subtype2_interroom_stairs_size0"},
      {0x137, 0x02, "Subtype2_waterhop_stairs_size2"},
      {0x138, 0x00, "Subtype2_spiral_stairs_size0"},
      {0x13D, 0x04, "Subtype2_table_4x3_size4"},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.label);
    RoomObject obj(test_case.object_id, 0, 0, test_case.size, 0);

    auto geo_result = ObjectGeometry::Get().MeasureByObjectId(obj);
    auto selection = table.GetSelectionBounds(test_case.object_id, test_case.size);

    if (!geo_result.ok()) {
      // Some subtype-2 furniture objects are not yet replay-backed by ObjectGeometry.
      // In those cases, keep fallback semantics sane and non-zero.
      EXPECT_GE(selection.width, 1);
      EXPECT_GE(selection.height, 1);
      continue;
    }

    EXPECT_EQ(selection.offset_x, geo_result->min_x_tiles);
    EXPECT_EQ(selection.offset_y, geo_result->min_y_tiles);
    EXPECT_EQ(selection.width, geo_result->width_tiles);
    EXPECT_EQ(selection.height, geo_result->height_tiles);
  }
}

TEST_F(ObjectDimensionTableTest,
       Subtype3RepeatersSelectionBoundsMatchObjectGeometry) {
  auto& table = ObjectDimensionTable::Get();
  table.LoadFromRom(rom_.get());

  struct ObjectCase {
    int16_t object_id;
    uint8_t size;
    const char* label;
  };

  // Subtype-3 repeater sweep (table-rock and related patterned objects).
  const ObjectCase cases[] = {
      {0xF94, 0x03, "Subtype3_table_4x3_size3"},
      {0xFF9, 0x03, "Subtype3_table_rock_4x3_size3"},
      {0xFCE, 0x02, "Subtype3_table_variant_size2"},
      {0xFE7, 0x02, "Subtype3_table_variant_fe7_size2"},
      {0xFE8, 0x02, "Subtype3_table_variant_fe8_size2"},
      {0xFEC, 0x02, "Subtype3_table_variant_fec_size2"},
      {0xFED, 0x02, "Subtype3_table_variant_fed_size2"},
      {0xFB4, 0x04, "Subtype3_pattern_fb4_size4"},
      {0xFC8, 0x04, "Subtype3_pattern_fc8_size4"},
      {0xFD4, 0x04, "Subtype3_pattern_fd4_size4"},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.label);
    RoomObject obj(test_case.object_id, 0, 0, test_case.size, 0);

    auto geo_result = ObjectGeometry::Get().MeasureByObjectId(obj);
    auto selection = table.GetSelectionBounds(test_case.object_id, test_case.size);

    if (!geo_result.ok()) {
      EXPECT_GE(selection.width, 1);
      EXPECT_GE(selection.height, 1);
      continue;
    }

    EXPECT_EQ(selection.offset_x, geo_result->min_x_tiles);
    EXPECT_EQ(selection.offset_y, geo_result->min_y_tiles);
    EXPECT_EQ(selection.width, geo_result->width_tiles);
    EXPECT_EQ(selection.height, geo_result->height_tiles);
  }
}

TEST_F(ObjectDimensionTableTest,
       BroadSelectionBoundsParitySweepAgainstObjectGeometry) {
  auto& table = ObjectDimensionTable::Get();
  table.LoadFromRom(rom_.get());
  auto& registry = DrawRoutineRegistry::Get();

  const std::array<uint8_t, 6> sizes = {0x00, 0x01, 0x02, 0x03, 0x07, 0x0F};
  std::vector<std::string> mismatches;
  int compared_cases = 0;
  int geometry_skips = 0;
  int deterministic_skips = 0;
  int clipped_skips = 0;

  auto sweep_object = [&](int object_id) {
    const int routine =
        registry.GetRoutineIdForObject(static_cast<int16_t>(object_id));
    if (routine < 0 || routine == DrawRoutineIds::kNothing) {
      return;
    }

    // These routines are currently not deterministic under ObjectGeometry replay:
    // - chest bounds depend on tile payload size (dummy payload forces 4x4 path)
    // - moving wall routines are still placeholder no-op in replay
    // - weird corner variants branch on tile payload shape
    if (routine == DrawRoutineIds::kChest ||
        routine == DrawRoutineIds::kMovingWallWest ||
        routine == DrawRoutineIds::kMovingWallEast ||
        routine == DrawRoutineIds::kWeirdCornerBottom_BothBG ||
        routine == DrawRoutineIds::kWeirdCornerTop_BothBG) {
      deterministic_skips += static_cast<int>(sizes.size());
      return;
    }

    for (uint8_t size : sizes) {
      RoomObject obj(static_cast<int16_t>(object_id), 0, 0, size, 0);
      auto geo_result = ObjectGeometry::Get().MeasureByObjectId(obj);
      if (!geo_result.ok()) {
        geometry_skips++;
        continue;
      }

      // Somaria down-left lines extend into negative X; current replay anchor
      // clips that leftward extent, so parity here is not meaningful yet.
      if (object_id == 0xF86) {
        clipped_skips++;
        continue;
      }

      // Replay uses a fixed 64x64 draw canvas; skip cases clipped at right/bottom
      // edges so we compare only un-clipped geometry against table formulas.
      const bool hits_right_edge =
          (geo_result->min_x_tiles + geo_result->width_tiles) >=
          DrawContext::kMaxTilesX;
      const bool hits_bottom_edge =
          (geo_result->min_y_tiles + geo_result->height_tiles) >=
          DrawContext::kMaxTilesY;
      if (hits_right_edge || hits_bottom_edge) {
        clipped_skips++;
        continue;
      }

      auto selection = table.GetSelectionBounds(object_id, size);
      if (selection.width > DrawContext::kMaxTilesX ||
          selection.height > DrawContext::kMaxTilesY) {
        clipped_skips++;
        continue;
      }

      compared_cases++;

      if (selection.offset_x != geo_result->min_x_tiles ||
          selection.offset_y != geo_result->min_y_tiles ||
          selection.width != geo_result->width_tiles ||
          selection.height != geo_result->height_tiles) {
        std::ostringstream oss;
        oss << "0x" << std::hex << object_id << " size 0x"
            << static_cast<int>(size) << std::dec << " sel=("
            << selection.offset_x << "," << selection.offset_y << ","
            << selection.width << "x" << selection.height << ") geo=("
            << geo_result->min_x_tiles << "," << geo_result->min_y_tiles
            << "," << geo_result->width_tiles << "x"
            << geo_result->height_tiles << ")";
        mismatches.push_back(oss.str());
      }
    }
  };

  for (int object_id = 0x000; object_id <= 0x13F; ++object_id) {
    sweep_object(object_id);
  }
  for (int object_id = 0xF80; object_id <= 0xFFF; ++object_id) {
    sweep_object(object_id);
  }

  EXPECT_GT(compared_cases, 300)
      << "Parity sweep should compare a broad object/sample space";

  if (!mismatches.empty()) {
    std::ostringstream summary;
    summary << "Found " << mismatches.size()
            << " selection-bound mismatches.";
    const size_t limit = std::min<size_t>(25, mismatches.size());
    for (size_t i = 0; i < limit; ++i) {
      summary << "\n  - " << mismatches[i];
    }
    if (mismatches.size() > limit) {
      summary << "\n  ... (" << (mismatches.size() - limit)
              << " more mismatches omitted)";
    }
    summary << "\nCompared cases: " << compared_cases
            << ", geometry skips: " << geometry_skips
            << ", deterministic skips: " << deterministic_skips
            << ", clipped skips: " << clipped_skips;
    FAIL() << summary.str();
  }
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

TEST_F(ObjectDimensionsTest, CalculatesDimensionsForSuperSquareObjectsUse2BitSizes) {
  ObjectDrawer drawer(rom_.get(), 0);

  // SuperSquare objects (e.g. 0xC2) use 2-bit X/Y sizes packed as:
  // size = (x_size << 2) | y_size, where each component is 0..3 (meaning 1..4).
  //
  // This test ensures CalculateObjectDimensions matches that encoding.
  RoomObject obj_c2_x1_y2(0xC2, 0, 0, /*size=*/(0 << 2) | 1, /*layer=*/1);
  auto dims = drawer.CalculateObjectDimensions(obj_c2_x1_y2);
  EXPECT_EQ(dims.first, 32);   // 1 super square * 32px
  EXPECT_EQ(dims.second, 64);  // 2 super squares * 32px

  RoomObject obj_c2_x4_y3(0xC2, 0, 0, /*size=*/(3 << 2) | 2, /*layer=*/1);
  dims = drawer.CalculateObjectDimensions(obj_c2_x4_y3);
  EXPECT_EQ(dims.first, 128);  // 4 super squares * 32px
  EXPECT_EQ(dims.second, 96);  // 3 super squares * 32px
}

}  // namespace zelda3
}  // namespace yaze
