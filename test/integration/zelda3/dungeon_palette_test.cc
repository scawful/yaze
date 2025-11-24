#include <gtest/gtest.h>
#include <vector>
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_tile.h"
#include "zelda3/dungeon/object_drawer.h"
#include "app/rom.h"

namespace yaze {
namespace zelda3 {
namespace test {

class DungeonPaletteTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Mock ROM is not strictly needed for DrawTileToBitmap if we pass tiledata
    // but ObjectDrawer constructor needs it.
    rom_ = std::make_unique<Rom>();
    drawer_ = std::make_unique<ObjectDrawer>(rom_.get());
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<ObjectDrawer> drawer_;
};

TEST_F(DungeonPaletteTest, PaletteOffsetIsCorrectFor4BPP) {
  // Create a bitmap
  gfx::Bitmap bitmap;
  bitmap.Create(8, 8, 8, std::vector<uint8_t>(64, 0));
  
  // Create dummy tile data (128x128 pixels worth, but we only need enough for one tile)
  // 128 pixels wide = 16 tiles.
  // We will use tile ID 0.
  // Tile 0 is at (0,0) in sheet.
  // src_index = (0 + py) * 128 + (0 + px)
  // We need a buffer of size 128 * 8 at least.
  std::vector<uint8_t> tiledata(128 * 8, 0);
  
  // Set some pixels in the tile data
  // Row 0, Col 0: Index 1
  tiledata[0] = 1;
  // Row 0, Col 1: Index 2
  tiledata[1] = 2;
  
  // Create TileInfo with palette index 1
  gfx::TileInfo tile_info;
  tile_info.id_ = 0;
  tile_info.palette_ = 1; // Palette 1
  tile_info.horizontal_mirror_ = false;
  tile_info.vertical_mirror_ = false;
  tile_info.over_ = false;
  
  // Draw
  drawer_->DrawTileToBitmap(bitmap, tile_info, 0, 0, tiledata.data());
  
  // Check pixels
  // Palette offset should be (palette & 0x07) * 16
  // For palette 1, offset is 16.
  // Pixel at (0,0) was 1. Result should be 1 + 16 = 17.
  // Pixel at (1,0) was 2. Result should be 2 + 16 = 18.
  
  const auto& data = bitmap.vector();
  // Bitmap data is row-major.
  // (0,0) is index 0.
  EXPECT_EQ(data[0], 17);
  EXPECT_EQ(data[1], 18);
  
  // Test with palette 0
  tile_info.palette_ = 0;
  drawer_->DrawTileToBitmap(bitmap, tile_info, 0, 0, tiledata.data());
  // Offset 0.
  // Note: DrawTileToBitmap does not clear the bitmap, it overwrites.
  // Pixel 0 was 17. Now it should be 1.
  EXPECT_EQ(data[0], 1);
  EXPECT_EQ(data[1], 2);
  
  // Test with palette 7
  tile_info.palette_ = 7;
  drawer_->DrawTileToBitmap(bitmap, tile_info, 0, 0, tiledata.data());
  // Offset 7 * 16 = 112.
  EXPECT_EQ(data[0], 1 + 112);
  EXPECT_EQ(data[1], 2 + 112);
}

TEST_F(DungeonPaletteTest, PaletteOffsetWorksWithConvertedData) {
  gfx::Bitmap bitmap;
  bitmap.Create(8, 8, 8, std::vector<uint8_t>(64, 0));

  // Create 4BPP packed tile data (simulating converted buffer)
  // Layout: 512 bytes per tile row, 4 bytes per tile
  // For tile 0: base_x=0, base_y=0
  std::vector<uint8_t> tiledata(512 * 8, 0);

  // Set pixel pair at row 0: high nibble = 3, low nibble = 5
  tiledata[0] = 0x35;

  gfx::TileInfo tile_info;
  tile_info.id_ = 0;
  tile_info.palette_ = 2;  // Palette 2 → offset 32
  tile_info.horizontal_mirror_ = false;
  tile_info.vertical_mirror_ = false;
  tile_info.over_ = false;

  drawer_->DrawTileToBitmap(bitmap, tile_info, 0, 0, tiledata.data());

  const auto& data = bitmap.vector();
  // Pixel 0 (high nibble 3) + offset 32 = 35
  EXPECT_EQ(data[0], 35);
  // Pixel 1 (low nibble 5) + offset 32 = 37
  EXPECT_EQ(data[1], 37);
}

} // namespace test
} // namespace zelda3
} // namespace yaze
