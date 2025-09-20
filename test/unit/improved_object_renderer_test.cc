#include "integration/dungeon_editor_test.h"
#include "app/zelda3/dungeon/improved_object_renderer.h"
#include "app/zelda3/dungeon/room_object.h"

#include <vector>

#include "gtest/gtest.h"

namespace yaze {
namespace test {

class ImprovedObjectRendererTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_rom_ = std::make_unique<MockRom>();
    SetupMockData();
    renderer_ = std::make_unique<zelda3::ImprovedObjectRenderer>(mock_rom_.get());
    SetupTestObjects();
  }

  void SetupMockData() {
    std::vector<uint8_t> mock_data(0x100000, 0x00);
    
    // Set up object subtype tables
    SetupSubtypeTable(mock_data, 0x8000, 0x100); // Subtype 1 table
    SetupSubtypeTable(mock_data, 0x83F0, 0x80);  // Subtype 2 table  
    SetupSubtypeTable(mock_data, 0x84F0, 0x100); // Subtype 3 table
    
    // Set up tile data
    SetupTileData(mock_data, 0x1B52, 0x1000);
    
    static_cast<MockRom*>(mock_rom_.get())->SetMockData(mock_data);
  }

  void SetupSubtypeTable(std::vector<uint8_t>& data, int base_addr, int count) {
    for (int i = 0; i < count; i++) {
      int addr = base_addr + (i * 2);
      if (addr + 1 < (int)data.size()) {
        // Point to tile data at 0x1B52 + (i * 8)
        int tile_offset = (i * 8) & 0xFFFF;
        data[addr] = tile_offset & 0xFF;
        data[addr + 1] = (tile_offset >> 8) & 0xFF;
      }
    }
  }

  void SetupTileData(std::vector<uint8_t>& data, int base_addr, int size) {
    for (int i = 0; i < size; i += 8) {
      int addr = base_addr + i;
      if (addr + 7 < (int)data.size()) {
        // Create simple tile data (4 words per tile)
        for (int j = 0; j < 8; j++) {
          data[addr + j] = (i + j) & 0xFF;
        }
      }
    }
  }

  void SetupTestObjects() {
    // Create test room objects
    test_object_ = std::make_unique<zelda3::RoomObject>(0x01, 5, 5, 0x12, 0);
    test_object_->set_rom(mock_rom_.get());
    test_object_->EnsureTilesLoaded();

    // Create test palette
    test_palette_.resize(16);
    for (int i = 0; i < 16; i++) {
      test_palette_[i] = gfx::SnesColor(i * 16, i * 16, i * 16);
    }
  }

  std::unique_ptr<MockRom> mock_rom_;
  std::unique_ptr<zelda3::ImprovedObjectRenderer> renderer_;
  std::unique_ptr<zelda3::RoomObject> test_object_;
  gfx::SnesPalette test_palette_;
};

TEST_F(ImprovedObjectRendererTest, RenderSingleObject) {
  auto result = renderer_->RenderObject(*test_object_, test_palette_);
  ASSERT_TRUE(result.ok());
  
  const auto& bitmap = result.value();
  EXPECT_EQ(bitmap.width(), 32);
  EXPECT_EQ(bitmap.height(), 32);
}

TEST_F(ImprovedObjectRendererTest, RenderMultipleObjects) {
  std::vector<zelda3::RoomObject> objects;
  
  // Create multiple test objects
  for (int i = 0; i < 3; i++) {
    auto obj = zelda3::RoomObject(0x01 + i, i * 2, i * 2, 0x12, 0);
    obj.set_rom(mock_rom_.get());
    obj.EnsureTilesLoaded();
    objects.push_back(obj);
  }
  
  auto result = renderer_->RenderObjects(objects, test_palette_, 256, 256);
  ASSERT_TRUE(result.ok());
  
  const auto& bitmap = result.value();
  EXPECT_EQ(bitmap.width(), 256);
  EXPECT_EQ(bitmap.height(), 256);
}

TEST_F(ImprovedObjectRendererTest, RenderObjectWithSize) {
  zelda3::ObjectSizeInfo size_info;
  size_info.width_tiles = 4;
  size_info.height_tiles = 2;
  size_info.is_horizontal = true;
  size_info.is_repeatable = true;
  size_info.repeat_count = 2;
  
  auto result = renderer_->RenderObjectWithSize(*test_object_, test_palette_, size_info);
  ASSERT_TRUE(result.ok());
  
  const auto& bitmap = result.value();
  EXPECT_EQ(bitmap.width(), 64); // 4 tiles * 16 pixels
  EXPECT_EQ(bitmap.height(), 32); // 2 tiles * 16 pixels
}

TEST_F(ImprovedObjectRendererTest, GetObjectPreview) {
  auto result = renderer_->GetObjectPreview(*test_object_, test_palette_);
  ASSERT_TRUE(result.ok());
  
  const auto& bitmap = result.value();
  EXPECT_EQ(bitmap.width(), 16);
  EXPECT_EQ(bitmap.height(), 16);
}

TEST_F(ImprovedObjectRendererTest, RenderObjectWithoutTiles) {
  auto empty_object = zelda3::RoomObject(0x01, 5, 5, 0x12, 0);
  empty_object.set_rom(mock_rom_.get());
  // Don't load tiles
  
  auto result = renderer_->RenderObject(empty_object, test_palette_);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(ImprovedObjectRendererTest, RenderObjectWithNullRom) {
  auto null_rom_renderer = std::make_unique<zelda3::ImprovedObjectRenderer>(nullptr);
  auto result = null_rom_renderer->RenderObject(*test_object_, test_palette_);
  
  // Should still work if object has tiles loaded
  if (!test_object_->tiles_.empty()) {
    EXPECT_TRUE(result.ok());
  }
}

}  // namespace test
}  // namespace yaze