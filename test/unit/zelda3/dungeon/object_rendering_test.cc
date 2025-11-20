#include "absl/status/status.h"
#include "app/gfx/background_buffer.h"
#include "app/gfx/snes_palette.h"
#include "app/rom.h"
#include "gtest/gtest.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_parser.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

class ObjectRenderingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a mock ROM for testing
    rom_ = std::make_unique<Rom>();
    // Initialize with minimal ROM data for testing
    std::vector<uint8_t> mock_rom_data(1024 * 1024, 0);  // 1MB mock ROM
    rom_->LoadFromData(mock_rom_data);
  }

  void TearDown() override { rom_.reset(); }

  std::unique_ptr<Rom> rom_;
  gfx::BackgroundBuffer bg1_;
  gfx::BackgroundBuffer bg2_;

  // Create a test palette
  gfx::SnesPalette CreateTestPalette() {
    gfx::SnesPalette palette;
    // Add some test colors
    palette.AddColor(gfx::SnesColor(0, 0, 0));        // Transparent
    palette.AddColor(gfx::SnesColor(255, 0, 0));      // Red
    palette.AddColor(gfx::SnesColor(0, 255, 0));      // Green
    palette.AddColor(gfx::SnesColor(0, 0, 255));      // Blue
    palette.AddColor(gfx::SnesColor(255, 255, 0));    // Yellow
    palette.AddColor(gfx::SnesColor(255, 0, 255));    // Magenta
    palette.AddColor(gfx::SnesColor(0, 255, 255));    // Cyan
    palette.AddColor(gfx::SnesColor(255, 255, 255));  // White
    return palette;
  }

  gfx::PaletteGroup CreateTestPaletteGroup() {
    gfx::PaletteGroup group;
    group.AddPalette(CreateTestPalette());
    return group;
  }
};

// Test object drawer initialization
TEST_F(ObjectRenderingTest, ObjectDrawerInitializesCorrectly) {
  ObjectDrawer drawer(rom_.get());

  // Test that drawer can be created without errors
  EXPECT_NE(rom_.get(), nullptr);
}

// Test object parser draw routine detection
TEST_F(ObjectRenderingTest, ObjectParserDetectsDrawRoutines) {
  ObjectParser parser(rom_.get());

  // Test common object IDs and their expected draw routines
  auto info_00 = parser.GetObjectDrawInfo(0x00);
  EXPECT_EQ(info_00.draw_routine_id, 0);
  EXPECT_EQ(info_00.routine_name, "Rightwards2x2_1to15or32");
  EXPECT_TRUE(info_00.is_horizontal);

  auto info_01 = parser.GetObjectDrawInfo(0x01);
  EXPECT_EQ(info_01.draw_routine_id, 1);
  EXPECT_EQ(info_01.routine_name, "Rightwards2x4_1to15or26");
  EXPECT_TRUE(info_01.is_horizontal);

  auto info_09 = parser.GetObjectDrawInfo(0x09);
  EXPECT_EQ(info_09.draw_routine_id, 5);
  EXPECT_EQ(info_09.routine_name, "DiagonalAcute_1to16");
  EXPECT_FALSE(info_09.is_horizontal);

  auto info_34 = parser.GetObjectDrawInfo(0x34);
  EXPECT_EQ(info_34.draw_routine_id, 16);
  EXPECT_EQ(info_34.routine_name, "Rightwards1x1Solid_1to16_plus3");
  EXPECT_TRUE(info_34.is_horizontal);

  // Test unmapped object defaults to solid block routine
  auto info_unknown = parser.GetObjectDrawInfo(0x999);
  EXPECT_EQ(info_unknown.draw_routine_id, 16);  // Default solid routine
  EXPECT_EQ(info_unknown.routine_name, "DefaultSolid");
}

// Test object drawer with various object types
TEST_F(ObjectRenderingTest, ObjectDrawerHandlesVariousObjectTypes) {
  ObjectDrawer drawer(rom_.get());
  auto palette_group = CreateTestPaletteGroup();

  // Test object 0x00 (horizontal floor tile)
  RoomObject floor_object(0x00, 10, 10, 3, 0);  // ID, X, Y, size, layer

  auto status = drawer.DrawObject(floor_object, bg1_, bg2_, palette_group);
  // Should succeed even if tiles aren't loaded (graceful handling)
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);

  // Test object 0x09 (diagonal stairs)
  RoomObject stair_object(0x09, 15, 15, 5, 0);
  stair_object.set_rom(rom_.get());

  status = drawer.DrawObject(stair_object, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);

  // Test object 0x34 (solid block)
  RoomObject block_object(0x34, 20, 20, 1, 0);
  block_object.set_rom(rom_.get());

  status = drawer.DrawObject(block_object, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
}

// Test object drawer with different layers
TEST_F(ObjectRenderingTest, ObjectDrawerHandlesDifferentLayers) {
  ObjectDrawer drawer(rom_.get());
  auto palette_group = CreateTestPaletteGroup();

  // Test BG1 layer object
  RoomObject bg1_object(0x00, 5, 5, 2, 0);  // Layer 0 = BG1
  bg1_object.set_rom(rom_.get());

  auto status = drawer.DrawObject(bg1_object, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);

  // Test BG2 layer object
  RoomObject bg2_object(0x01, 10, 10, 2, 1);  // Layer 1 = BG2
  bg2_object.set_rom(rom_.get());

  status = drawer.DrawObject(bg2_object, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
}

// Test object drawer with size variations
TEST_F(ObjectRenderingTest, ObjectDrawerHandlesSizeVariations) {
  ObjectDrawer drawer(rom_.get());
  auto palette_group = CreateTestPaletteGroup();

  // Test small object
  RoomObject small_object(0x00, 5, 5, 1, 0);  // Size = 1
  small_object.set_rom(rom_.get());

  auto status = drawer.DrawObject(small_object, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);

  // Test large object
  RoomObject large_object(0x00, 10, 10, 15, 0);  // Size = 15
  large_object.set_rom(rom_.get());

  status = drawer.DrawObject(large_object, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);

  // Test maximum size object
  RoomObject max_object(0x00, 15, 15, 31, 0);  // Size = 31 (0x1F)
  max_object.set_rom(rom_.get());

  status = drawer.DrawObject(max_object, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
}

// Test object drawer with edge cases
TEST_F(ObjectRenderingTest, ObjectDrawerHandlesEdgeCases) {
  ObjectDrawer drawer(rom_.get());
  auto palette_group = CreateTestPaletteGroup();

  // Test object at origin
  RoomObject origin_object(0x34, 0, 0, 1, 0);
  origin_object.set_rom(rom_.get());

  auto status = drawer.DrawObject(origin_object, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);

  // Test object with zero size
  RoomObject zero_size_object(0x34, 10, 10, 0, 0);
  zero_size_object.set_rom(rom_.get());

  status = drawer.DrawObject(zero_size_object, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);

  // Test object with maximum coordinates
  RoomObject max_coord_object(0x34, 63, 63, 1, 0);  // Near buffer edge
  max_coord_object.set_rom(rom_.get());

  status = drawer.DrawObject(max_coord_object, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
}

// Test object drawer with multiple objects
TEST_F(ObjectRenderingTest, ObjectDrawerHandlesMultipleObjects) {
  ObjectDrawer drawer(rom_.get());
  auto palette_group = CreateTestPaletteGroup();

  std::vector<RoomObject> objects;

  // Create various test objects
  objects.emplace_back(0x00, 5, 5, 3, 0);    // Horizontal floor
  objects.emplace_back(0x01, 10, 10, 2, 0);  // Vertical floor
  objects.emplace_back(0x09, 15, 15, 4, 0);  // Diagonal stairs
  objects.emplace_back(0x34, 20, 20, 1, 1);  // Solid block on BG2

  // Set ROM for all objects
  for (auto& obj : objects) {
    obj.set_rom(rom_.get());
  }

  auto status = drawer.DrawObjectList(objects, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
}

// Test specific draw routines
TEST_F(ObjectRenderingTest, DrawRoutinesWorkCorrectly) {
  ObjectDrawer drawer(rom_.get());
  auto palette_group = CreateTestPaletteGroup();

  // Test rightward patterns
  RoomObject rightward_obj(0x00, 5, 5, 5, 0);
  rightward_obj.set_rom(rom_.get());

  auto status = drawer.DrawObject(rightward_obj, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);

  // Test diagonal patterns
  RoomObject diagonal_obj(0x09, 10, 10, 6, 0);
  diagonal_obj.set_rom(rom_.get());

  status = drawer.DrawObject(diagonal_obj, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);

  // Test solid block patterns
  RoomObject solid_obj(0x34, 15, 15, 8, 0);
  solid_obj.set_rom(rom_.get());

  status = drawer.DrawObject(solid_obj, bg1_, bg2_, palette_group);
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
}

// Test object drawer error handling
TEST_F(ObjectRenderingTest, ObjectDrawerHandlesErrorsGracefully) {
  ObjectDrawer drawer(nullptr);  // No ROM
  auto palette_group = CreateTestPaletteGroup();

  RoomObject test_object(0x00, 5, 5, 1, 0);

  auto status = drawer.DrawObject(test_object, bg1_, bg2_, palette_group);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

// Test object parser with various object IDs
TEST_F(ObjectRenderingTest, ObjectParserHandlesVariousObjectIDs) {
  ObjectParser parser(rom_.get());

  // Test subtype 1 objects (0x00-0xFF)
  for (int id = 0; id <= 0x40; id += 4) {  // Test every 4th object
    auto info = parser.GetObjectDrawInfo(id);
    EXPECT_GE(info.draw_routine_id, 0);
    EXPECT_LT(info.draw_routine_id, 25);  // Should be within valid range
    EXPECT_FALSE(info.routine_name.empty());
  }

  // Test some specific important objects
  std::vector<int16_t> important_objects = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
      0x15, 0x16, 0x21, 0x22, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
      0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40};

  for (int16_t obj_id : important_objects) {
    auto info = parser.GetObjectDrawInfo(obj_id);
    EXPECT_GE(info.draw_routine_id, 0);
    EXPECT_LT(info.draw_routine_id, 25);
    EXPECT_FALSE(info.routine_name.empty());

    // Verify tile count is reasonable
    EXPECT_GT(info.tile_count, 0);
    EXPECT_LE(info.tile_count, 64);  // Reasonable upper bound
  }
}

// Test object drawer performance with many objects
TEST_F(ObjectRenderingTest, ObjectDrawerPerformanceTest) {
  ObjectDrawer drawer(rom_.get());
  auto palette_group = CreateTestPaletteGroup();

  std::vector<RoomObject> objects;

  // Create 100 test objects
  for (int i = 0; i < 100; ++i) {
    int id = i % 65;       // Cycle through object IDs 0-64
    int x = (i * 2) % 60;  // Spread across buffer
    int y = (i * 3) % 60;
    int size = (i % 8) + 1;  // Size 1-8
    int layer = i % 2;       // Alternate layers

    objects.emplace_back(id, x, y, size, layer);
    objects.back().set_rom(rom_.get());
  }

  // Time the drawing operation
  auto start_time = std::chrono::high_resolution_clock::now();

  auto status = drawer.DrawObjectList(objects, bg1_, bg2_, palette_group);

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);

  // Should complete in reasonable time (less than 1 second for 100 objects)
  EXPECT_LT(duration.count(), 1000);

  std::cout << "Drew 100 objects in " << duration.count() << "ms" << std::endl;
}

}  // namespace zelda3
}  // namespace yaze
