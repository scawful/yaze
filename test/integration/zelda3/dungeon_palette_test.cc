#include <gtest/gtest.h>
#include <vector>
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_tile.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/game_data.h"
#include "rom/rom.h"

namespace yaze {
namespace zelda3 {
namespace test {

class DungeonPaletteTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Mock ROM is not strictly needed for DrawTileToBitmap if we pass tiledata
    // but ObjectDrawer constructor needs it.
    rom_ = std::make_unique<Rom>();
    game_data_ = std::make_unique<GameData>(rom_.get());
    drawer_ = std::make_unique<ObjectDrawer>(rom_.get(), 0);
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<GameData> game_data_;
  std::unique_ptr<ObjectDrawer> drawer_;
};

TEST_F(DungeonPaletteTest, PaletteOffsetIsCorrectFor8BPP) {
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
  // Dungeon tiles use 15-color sub-palettes (not 8 like overworld).
  // Formula: final_color = (pixel - 1) + (palette * 15)
  // For palette 1, offset is 15.
  // Pixel at (0,0) was 1. Result should be (1-1) + 15 = 15.
  // Pixel at (1,0) was 2. Result should be (2-1) + 15 = 16.

  const auto& data = bitmap.vector();
  // Bitmap data is row-major.
  // (0,0) is index 0.
  EXPECT_EQ(data[0], 15);  // (1-1) + 15 = 15
  EXPECT_EQ(data[1], 16);  // (2-1) + 15 = 16

  // Test with palette 0
  tile_info.palette_ = 0;
  drawer_->DrawTileToBitmap(bitmap, tile_info, 0, 0, tiledata.data());
  // Offset 0 * 15 = 0.
  // Pixel 1 -> (1-1) + 0 = 0
  // Pixel 2 -> (2-1) + 0 = 1
  EXPECT_EQ(data[0], 0);
  EXPECT_EQ(data[1], 1);

  // Test with palette 7 (wraps to palette 1 due to 6 sub-palette limit)
  tile_info.palette_ = 7;
  drawer_->DrawTileToBitmap(bitmap, tile_info, 0, 0, tiledata.data());
  // Palette 7 wraps to 7 % 6 = 1, offset 1 * 15 = 15.
  EXPECT_EQ(data[0], 15);  // (1-1) + 15 = 15
  EXPECT_EQ(data[1], 16);  // (2-1) + 15 = 16
}

TEST_F(DungeonPaletteTest, PaletteOffsetWorksWithConvertedData) {
  gfx::Bitmap bitmap;
  bitmap.Create(8, 8, 8, std::vector<uint8_t>(64, 0));

  // Create 8BPP unpacked tile data (simulating converted buffer)
  // Layout: 128 bytes per tile row, 8 bytes per tile
  // For tile 0: base_x=0, base_y=0
  std::vector<uint8_t> tiledata(128 * 8, 0);

  // Set pixel pair at row 0: pixel 0 = 3, pixel 1 = 5
  tiledata[0] = 3;
  tiledata[1] = 5;

  gfx::TileInfo tile_info;
  tile_info.id_ = 0;
  tile_info.palette_ = 2;  // Palette 2 → offset 30 (2 * 15)
  tile_info.horizontal_mirror_ = false;
  tile_info.vertical_mirror_ = false;
  tile_info.over_ = false;

  drawer_->DrawTileToBitmap(bitmap, tile_info, 0, 0, tiledata.data());

  const auto& data = bitmap.vector();
  // Dungeon tiles use 15-color sub-palettes.
  // Formula: final_color = (pixel - 1) + (palette * 15)
  // Pixel 3: (3-1) + 30 = 32
  // Pixel 5: (5-1) + 30 = 34
  EXPECT_EQ(data[0], 32);
  EXPECT_EQ(data[1], 34);
}

TEST_F(DungeonPaletteTest, InspectActualPaletteColors) {
  // Load actual ROM file
  auto load_result = rom_->LoadFromFile("zelda3.sfc");
  if (!load_result.ok()) {
    GTEST_SKIP() << "ROM file not found, skipping";
  }

  // Load game data (palettes, etc.)
  auto game_data_result = LoadGameData(*rom_, *game_data_);
  if (!game_data_result.ok()) {
    GTEST_SKIP() << "Failed to load game data: " << game_data_result.message();
  }

  // Get dungeon main palette group
  const auto& dungeon_pal_group = game_data_->palette_groups.dungeon_main;
  
  ASSERT_FALSE(dungeon_pal_group.empty()) << "Dungeon palette group is empty!";
  
  // Get first palette (palette 0)
  const auto& palette0 = dungeon_pal_group[0];
  
  printf("\n=== Dungeon Palette 0 - First 16 colors ===\n");
  for (size_t i = 0; i < std::min(size_t(16), palette0.size()); ++i) {
    const auto& color = palette0[i];
    auto rgb = color.rgb();
    printf("Color %02zu: R=%03d G=%03d B=%03d (0x%02X%02X%02X)\n", 
           i, 
           static_cast<int>(rgb.x), 
           static_cast<int>(rgb.y), 
           static_cast<int>(rgb.z),
           static_cast<int>(rgb.x),
           static_cast<int>(rgb.y),
           static_cast<int>(rgb.z));
  }
  
  // Total palette size
  printf("\nTotal palette size: %zu colors\n", palette0.size());
  EXPECT_EQ(palette0.size(), 90) << "Expected 90 colors for dungeon palette";
  
  // Colors 56-63 (palette 7 offset: 7*8=56)
  printf("\n=== Colors 56-63 (pal=7 range) ===\n");
  for (size_t i = 56; i < std::min(size_t(64), palette0.size()); ++i) {
    const auto& color = palette0[i];
    auto rgb = color.rgb();
    printf("Color %02zu: R=%03d G=%03d B=%03d (0x%02X%02X%02X)\n", 
           i, 
           static_cast<int>(rgb.x), 
           static_cast<int>(rgb.y), 
           static_cast<int>(rgb.z),
           static_cast<int>(rgb.x),
           static_cast<int>(rgb.y),
           static_cast<int>(rgb.z));
  }
}

} // namespace test
} // namespace zelda3
} // namespace yaze
