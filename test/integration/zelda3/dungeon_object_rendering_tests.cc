// Integration tests for dungeon object rendering using ObjectDrawer
// Updated for DungeonEditorV2 architecture - uses ObjectDrawer (production system)
// instead of the obsolete ObjectRenderer

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <vector>

#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_palette.h"
#include "app/rom.h"
#include "test_utils.h"
#include "testing.h"

namespace yaze {
namespace test {

/**
 * @brief Tests for ObjectDrawer with realistic dungeon scenarios
 * 
 * These tests validate that ObjectDrawer correctly renders dungeon objects
 * to BackgroundBuffers using pattern-based drawing routines.
 */
class DungeonObjectRenderingTests : public TestRomManager::BoundRomTest {
 protected:
  void SetUp() override {
    BoundRomTest::SetUp();

    // Create drawer
    drawer_ = std::make_unique<zelda3::ObjectDrawer>(rom());

    // Create background buffers
    bg1_ = std::make_unique<gfx::BackgroundBuffer>(512, 512);
    bg2_ = std::make_unique<gfx::BackgroundBuffer>(512, 512);

    // Setup test palette
    palette_group_ = CreateTestPaletteGroup();
  }

  void TearDown() override {
    bg2_.reset();
    bg1_.reset();
    drawer_.reset();
    BoundRomTest::TearDown();
  }

  gfx::PaletteGroup CreateTestPaletteGroup() {
    gfx::PaletteGroup group;
    gfx::SnesPalette palette;

    // Create standard dungeon palette
    for (int i = 0; i < 16; i++) {
      int intensity = i * 16;
      palette.AddColor(gfx::SnesColor(intensity, intensity, intensity));
    }

    group.AddPalette(palette);
    return group;
  }

  zelda3::RoomObject CreateTestObject(int id, int x, int y, int size = 0x12,
                                      int layer = 0) {
    zelda3::RoomObject obj(id, x, y, size, layer);
    obj.set_rom(rom());
    obj.EnsureTilesLoaded();
    return obj;
  }

  std::unique_ptr<zelda3::ObjectDrawer> drawer_;
  std::unique_ptr<gfx::BackgroundBuffer> bg1_;
  std::unique_ptr<gfx::BackgroundBuffer> bg2_;
  gfx::PaletteGroup palette_group_;
};

// Test basic object drawing
TEST_F(DungeonObjectRenderingTests, BasicObjectDrawing) {
  std::vector<zelda3::RoomObject> objects;
  objects.push_back(CreateTestObject(0x10, 5, 5, 0x12, 0));    // Wall
  objects.push_back(CreateTestObject(0x20, 10, 10, 0x22, 0));  // Floor

  bg1_->ClearBuffer();
  bg2_->ClearBuffer();

  auto status = drawer_->DrawObjectList(objects, *bg1_, *bg2_, palette_group_);
  ASSERT_TRUE(status.ok()) << "Drawing failed: " << status.message();

  // Verify buffers have content
  auto& bg1_bitmap = bg1_->bitmap();
  EXPECT_TRUE(bg1_bitmap.is_active());
  EXPECT_GT(bg1_bitmap.width(), 0);
}

// Test objects on different layers
TEST_F(DungeonObjectRenderingTests, MultiLayerRendering) {
  std::vector<zelda3::RoomObject> objects;
  objects.push_back(CreateTestObject(0x10, 5, 5, 0x12, 0));    // BG1
  objects.push_back(CreateTestObject(0x20, 10, 10, 0x22, 1));  // BG2
  objects.push_back(CreateTestObject(0x30, 15, 15, 0x12, 2));  // BG3

  bg1_->ClearBuffer();
  bg2_->ClearBuffer();

  auto status = drawer_->DrawObjectList(objects, *bg1_, *bg2_, palette_group_);
  ASSERT_TRUE(status.ok());

  // Both buffers should be active
  EXPECT_TRUE(bg1_->bitmap().is_active());
  EXPECT_TRUE(bg2_->bitmap().is_active());
}

// Test empty object list
TEST_F(DungeonObjectRenderingTests, EmptyObjectList) {
  std::vector<zelda3::RoomObject> objects;  // Empty

  bg1_->ClearBuffer();
  bg2_->ClearBuffer();

  auto status = drawer_->DrawObjectList(objects, *bg1_, *bg2_, palette_group_);
  // Should succeed (drawing nothing is valid)
  EXPECT_TRUE(status.ok());
}

// Test large object set
TEST_F(DungeonObjectRenderingTests, LargeObjectSet) {
  std::vector<zelda3::RoomObject> objects;

  // Create 100 test objects
  for (int i = 0; i < 100; i++) {
    int x = (i % 10) * 5;
    int y = (i / 10) * 5;
    objects.push_back(CreateTestObject(0x10 + (i % 20), x, y, 0x12, i % 2));
  }

  bg1_->ClearBuffer();
  bg2_->ClearBuffer();

  auto start = std::chrono::high_resolution_clock::now();
  auto status = drawer_->DrawObjectList(objects, *bg1_, *bg2_, palette_group_);
  auto end = std::chrono::high_resolution_clock::now();

  ASSERT_TRUE(status.ok());

  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  // Should complete in reasonable time
  EXPECT_LT(duration.count(), 1000)
      << "Rendered 100 objects in " << duration.count() << "ms";
}

// Test boundary conditions
TEST_F(DungeonObjectRenderingTests, BoundaryObjects) {
  std::vector<zelda3::RoomObject> objects;

  // Objects at boundaries
  objects.push_back(CreateTestObject(0x10, 0, 0, 0x12, 0));    // Origin
  objects.push_back(CreateTestObject(0x10, 63, 63, 0x12, 0));  // Max valid
  objects.push_back(CreateTestObject(0x10, 32, 32, 0x12, 0));  // Center

  bg1_->ClearBuffer();
  bg2_->ClearBuffer();

  auto status = drawer_->DrawObjectList(objects, *bg1_, *bg2_, palette_group_);
  EXPECT_TRUE(status.ok());
}

// Test various object types
TEST_F(DungeonObjectRenderingTests, VariousObjectTypes) {
  // Test common object types
  std::vector<int> object_types = {
      0x00, 0x01, 0x02, 0x03,  // Floor/wall objects
      0x09, 0x0A,              // Diagonal objects
      0x10, 0x11, 0x12,        // Standard objects
      0x20, 0x21,              // Decorative objects
      0x34,                    // Solid block
  };

  for (int obj_type : object_types) {
    std::vector<zelda3::RoomObject> objects;
    objects.push_back(CreateTestObject(obj_type, 10, 10, 0x12, 0));

    bg1_->ClearBuffer();
    bg2_->ClearBuffer();

    auto status =
        drawer_->DrawObjectList(objects, *bg1_, *bg2_, palette_group_);
    // Some object types might not be valid, that's okay
    if (!status.ok()) {
      std::cout << "Object type 0x" << std::hex << obj_type << std::dec
                << " not renderable: " << status.message() << std::endl;
    }
  }
}

// Test error handling
TEST_F(DungeonObjectRenderingTests, ErrorHandling) {
  // Test with null ROM
  zelda3::ObjectDrawer null_drawer(nullptr);
  std::vector<zelda3::RoomObject> objects;
  objects.push_back(CreateTestObject(0x10, 5, 5));

  bg1_->ClearBuffer();
  bg2_->ClearBuffer();

  auto status =
      null_drawer.DrawObjectList(objects, *bg1_, *bg2_, palette_group_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

}  // namespace test
}  // namespace yaze
