// Integration tests for Dungeon Graphics Buffer Transparency
// Verifies that 3BPP→8BPP conversion preserves transparent pixels (value 0)

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "rom/rom.h"
#include "test/test_utils.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace zelda3 {
namespace test {

class DungeonGraphicsTransparencyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();

    yaze::test::TestRomManager::SkipIfRomMissing(
        yaze::test::RomRole::kVanilla,
        "DungeonGraphicsTransparencyTest");
    const std::string rom_path =
        yaze::test::TestRomManager::GetRomPath(yaze::test::RomRole::kVanilla);
    auto status = rom_->LoadFromFile(rom_path);
    if (!status.ok()) {
      GTEST_SKIP() << "ROM file not available: " << status.message();
    }

    // Load all Zelda3 game data (metadata, palettes, gfx groups, graphics)
    auto load_status = LoadGameData(*rom_, game_data_);
    if (!load_status.ok()) {
      GTEST_SKIP() << "Graphics loading failed: " << load_status.message();
    }
  }

  std::unique_ptr<Rom> rom_;
  GameData game_data_;
};

// Test 1: Verify graphics buffer has transparent pixels
TEST_F(DungeonGraphicsTransparencyTest, GraphicsBufferHasTransparentPixels) {
  // The graphics buffer should contain many 0s representing transparent pixels
  auto& gfx_buffer = game_data_.graphics_buffer;
  ASSERT_GT(gfx_buffer.size(), 0);

  // Count zeros in first 10 sheets (dungeon graphics)
  int zero_count = 0;
  int total_pixels = 0;
  const int sheets_to_check = 10;
  const int pixels_per_sheet = 4096;

  for (int sheet = 0; sheet < sheets_to_check; sheet++) {
    int offset = sheet * pixels_per_sheet;
    if (offset + pixels_per_sheet > static_cast<int>(gfx_buffer.size())) break;

    for (int i = 0; i < pixels_per_sheet; i++) {
      if (gfx_buffer[offset + i] == 0) zero_count++;
      total_pixels++;
    }
  }

  float zero_percent = 100.0f * zero_count / total_pixels;
  printf("[GraphicsBuffer] Zeros: %d / %d (%.1f%%)\n", zero_count, total_pixels,
         zero_percent);

  // In 3BPP graphics, we expect significant transparent pixels (10%+)
  // If this is near 0%, something is wrong with the 8BPP conversion
  EXPECT_GT(zero_percent, 5.0f)
      << "Graphics buffer should have at least 5% transparent pixels. "
      << "Got " << zero_percent << "%. This indicates the 3BPP→8BPP "
      << "conversion may not be preserving transparency correctly.";
}

// Test 2: Verify room graphics buffer after CopyRoomGraphicsToBuffer
TEST_F(DungeonGraphicsTransparencyTest, RoomGraphicsBufferHasTransparentPixels) {
  // Create room 0 (Ganon's room - known to have walls)
  Room room = LoadRoomHeaderFromRom(rom_.get(), 0x00);
  room.SetGameData(&game_data_);
  room.LoadRoomGraphics(0xFF);
  room.CopyRoomGraphicsToBuffer();

  // Access the room's current_gfx16_ buffer
  const auto& gfx16 = room.get_gfx_buffer();
  ASSERT_GT(gfx16.size(), 0);

  // Count zeros in the room's graphics buffer (background blocks only)
  constexpr int kBlockSize = 4096;
  constexpr int kBgBlocks = 8;
  int zero_count = 0;
  int total_pixels = 0;
  for (int block = 0; block < kBgBlocks; block++) {
    int base = block * kBlockSize;
    for (int i = 0; i < kBlockSize; i++) {
      if (gfx16[base + i] == 0) zero_count++;
      total_pixels++;
    }
  }

  float zero_percent = 100.0f * zero_count / total_pixels;
  printf("[RoomGraphics] Room 0: Zeros: %d / %d (%.1f%%)\n", zero_count,
         total_pixels, zero_percent);

  // Log first 64 bytes (one tile's worth) to see actual values
  printf("[RoomGraphics] First 64 bytes:\n");
  for (int row = 0; row < 8; row++) {
    printf("  Row %d: ", row);
    for (int col = 0; col < 8; col++) {
      printf("%02X ", gfx16[row * 128 + col]);  // 128 = sheet width stride
    }
    printf("\n");
  }

  // Print value distribution
  int value_counts[16] = {0};
  int other_count = 0;
  for (int block = 0; block < kBgBlocks; block++) {
    int base = block * kBlockSize;
    for (int i = 0; i < kBlockSize; i++) {
      uint8_t value = gfx16[base + i];
      if (value < 16) {
        value_counts[value]++;
      } else {
        other_count++;
      }
    }
  }

  printf("[RoomGraphics] Value distribution:\n");
  for (int v = 0; v < 16; v++) {
    printf("  Value %d: %d (%.1f%%)\n", v, value_counts[v],
           100.0f * value_counts[v] / total_pixels);
  }
  if (other_count > 0) {
    printf("  Values >15: %d (%.1f%%) - UNEXPECTED for 4BPP!\n", other_count,
           100.0f * other_count / total_pixels);
  }

  EXPECT_GT(zero_percent, 5.0f)
      << "Room graphics buffer should have transparent pixels. "
      << "Got " << zero_percent << "%. Check CopyRoomGraphicsToBuffer().";

  // Background blocks should not exceed 4BPP (0-15) values.
  EXPECT_EQ(other_count, 0)
      << "Found " << other_count << " pixels with values > 15 in BG blocks. "
      << "BG graphics should only have values 0-15.";
}

// Test 3: Verify specific tile has expected mix of transparent/opaque
TEST_F(DungeonGraphicsTransparencyTest, SpecificTileTransparency) {
  Room room = LoadRoomHeaderFromRom(rom_.get(), 0x00);
  room.SetGameData(&game_data_);
  room.LoadRoomGraphics(0xFF);
  room.CopyRoomGraphicsToBuffer();

  const auto& gfx16 = room.get_gfx_buffer();

  // Check tile 0 in block 0 (should be typical dungeon graphics)
  // Tile layout: 16 tiles per row, each tile 8x8 pixels
  // Row stride: 128 bytes (16 tiles * 8 pixels)
  int tile_id = 0;
  int tile_col = tile_id % 16;
  int tile_row = tile_id / 16;
  int tile_base_x = tile_col * 8;
  int tile_base_y = tile_row * 1024;  // 8 rows * 128 bytes per row

  int zeros_in_tile = 0;
  int total_in_tile = 64;  // 8x8

  printf("[Tile %d] Pixel values:\n", tile_id);
  for (int py = 0; py < 8; py++) {
    printf("  ");
    for (int px = 0; px < 8; px++) {
      int src_index = (py * 128) + px + tile_base_x + tile_base_y;
      uint8_t pixel = gfx16[src_index];
      printf("%d ", pixel);
      if (pixel == 0) zeros_in_tile++;
    }
    printf("\n");
  }

  float tile_zero_percent = 100.0f * zeros_in_tile / total_in_tile;
  printf("[Tile %d] Transparent pixels: %d / %d (%.1f%%)\n", tile_id,
         zeros_in_tile, total_in_tile, tile_zero_percent);

  // Check a wall tile (ID 0x90 is commonly a wall tile)
  tile_id = 0x90;
  tile_col = tile_id % 16;
  tile_row = tile_id / 16;
  tile_base_x = tile_col * 8;
  tile_base_y = tile_row * 1024;

  zeros_in_tile = 0;
  printf("\n[Tile 0x%02X] Pixel values:\n", tile_id);
  for (int py = 0; py < 8; py++) {
    printf("  ");
    for (int px = 0; px < 8; px++) {
      int src_index = (py * 128) + px + tile_base_x + tile_base_y;
      if (src_index < static_cast<int>(gfx16.size())) {
        uint8_t pixel = gfx16[src_index];
        printf("%d ", pixel);
        if (pixel == 0) zeros_in_tile++;
      }
    }
    printf("\n");
  }
  printf("[Tile 0x%02X] Transparent pixels: %d / %d\n", tile_id, zeros_in_tile,
         total_in_tile);
}

// Test 4: Verify wall objects have tiles loaded
TEST_F(DungeonGraphicsTransparencyTest, WallObjectsHaveTiles) {
  Room room(0x00, rom_.get());
  room.LoadRoomGraphics(0xFF);
  room.LoadObjects();  // Load objects from ROM!
  room.CopyRoomGraphicsToBuffer();

  // Get the room's objects
  auto& objects = room.GetTileObjects();
  printf("[Objects] Room 0 has %zu objects\n", objects.size());

  // Count objects by type and check tiles
  int walls_0x00 = 0, walls_0x01_02 = 0, walls_0x60_plus = 0, other = 0;
  int missing_tiles = 0;

  for (size_t i = 0; i < objects.size() && i < 20; i++) {  // First 20 objects
    auto& obj = objects[i];
    obj.SetRom(rom_.get());
    obj.EnsureTilesLoaded();

    printf("[Object %zu] id=0x%03X pos=(%d,%d) size=%d tiles=%zu\n", i, obj.id_,
           obj.x(), obj.y(), obj.size(), obj.tiles().size());

    if (obj.id_ == 0x00) {
      walls_0x00++;
    } else if (obj.id_ >= 0x01 && obj.id_ <= 0x02) {
      walls_0x01_02++;
    } else if (obj.id_ >= 0x60 && obj.id_ <= 0x6F) {
      walls_0x60_plus++;
    } else {
      other++;
    }

    if (obj.tiles().empty()) {
      missing_tiles++;
      printf("  WARNING: Object 0x%03X has NO tiles!\n", obj.id_);
    } else {
      // Note: Some objects only need 1 tile (e.g., 0xC0) per ZScream's lookup table
      // This is valid behavior, not a bug
      // Print first 4 tile IDs
      printf("  Tile IDs: ");
      for (size_t t = 0; t < std::min(obj.tiles().size(), size_t(4)); t++) {
        printf("0x%03X ", obj.tiles()[t].id_);
      }
      printf("\n");
    }
  }

  printf("\n[Summary] walls_0x00=%d walls_0x01_02=%d walls_0x60+=%d other=%d\n",
         walls_0x00, walls_0x01_02, walls_0x60_plus, other);
  printf("[Summary] missing_tiles=%d\n", missing_tiles);

  // Every object should have tiles loaded (tile count varies per object type)
  EXPECT_EQ(missing_tiles, 0)
      << "Some objects have no tiles loaded - check EnsureTilesLoaded()";
}

// Test 5: Verify objects are actually drawn to bitmaps
TEST_F(DungeonGraphicsTransparencyTest, ObjectsDrawToBitmap) {
  Room room(0x00, rom_.get());
  room.LoadRoomGraphics(0xFF);
  room.LoadObjects();
  room.CopyRoomGraphicsToBuffer();

  // Get background buffers - they create their own bitmaps when needed
  auto& bg1 = room.bg1_buffer();
  auto& bg2 = room.bg2_buffer();

  // DON'T manually create bitmaps - let DrawFloor/DrawBackground create them
  // with the correct size (512*512 = 262144 bytes)
  // The DrawFloor call initializes the bitmap properly
  bg1.DrawFloor(rom_->vector(), zelda3::tile_address, zelda3::tile_address_floor,
                room.floor1());
  bg2.DrawFloor(rom_->vector(), zelda3::tile_address, zelda3::tile_address_floor,
                room.floor2());

  // Get objects
  auto& objects = room.GetTileObjects();
  printf("[DrawTest] Room 0 has %zu objects\n", objects.size());

  // Create ObjectDrawer with room's graphics buffer
  ObjectDrawer drawer(rom_.get(), 0, room.get_gfx_buffer().data());

  // Create a palette group (needed for draw)
  gfx::PaletteGroup palette_group;
  auto& dungeon_pal = game_data_.palette_groups.dungeon_main;
  if (!dungeon_pal.empty()) {
    palette_group.AddPalette(dungeon_pal[0]);
  }

  // Draw objects
  auto status = drawer.DrawObjectList(objects, bg1, bg2, palette_group);
  if (!status.ok()) {
    printf("[DrawTest] DrawObjectList failed: %s\n",
           std::string(status.message()).c_str());
  }

  // Check if any pixels were written to bg1
  int nonzero_pixels_bg1 = 0;
  int nonzero_pixels_bg2 = 0;
  size_t bg1_size = 512 * 512;
  size_t bg2_size = 512 * 512;
  auto bg1_data = bg1.bitmap().data();
  auto bg2_data = bg2.bitmap().data();

  for (size_t i = 0; i < bg1_size; i++) {
    if (bg1_data[i] != 0) nonzero_pixels_bg1++;
  }
  for (size_t i = 0; i < bg2_size; i++) {
    if (bg2_data[i] != 0) nonzero_pixels_bg2++;
  }

  printf("[DrawTest] BG1 non-zero pixels: %d / %zu (%.2f%%)\n",
         nonzero_pixels_bg1, bg1_size,
         100.0f * nonzero_pixels_bg1 / bg1_size);
  printf("[DrawTest] BG2 non-zero pixels: %d / %zu (%.2f%%)\n",
         nonzero_pixels_bg2, bg2_size,
         100.0f * nonzero_pixels_bg2 / bg2_size);

  // We should have SOME pixels drawn
  EXPECT_GT(nonzero_pixels_bg1 + nonzero_pixels_bg2, 0)
      << "No pixels were drawn to either background!";

  // Print first few rows of bg1 to see the pattern
  printf("[DrawTest] BG1 first 16x4 pixels:\n");
  for (int y = 0; y < 4; y++) {
    printf("  Row %d: ", y);
    for (int x = 0; x < 16; x++) {
      printf("%02X ", bg1_data[y * 512 + x]);
    }
    printf("\n");
  }
}

}  // namespace test
}  // namespace zelda3
}  // namespace yaze
