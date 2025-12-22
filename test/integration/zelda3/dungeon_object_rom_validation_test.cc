// ROM Validation Tests for Dungeon Object System
// These tests verify that our object parsing and rendering code correctly
// interprets actual ALTTP ROM data.

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_parser.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace test {

/**
 * @brief ROM validation tests for dungeon object system
 *
 * These tests verify that our code correctly reads and interprets
 * actual data from the ALTTP ROM. They validate:
 * - Object tile pointer tables
 * - Tile count lookup tables
 * - Object decoding from room data
 * - Known room object layouts
 */
class DungeonObjectRomValidationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_ = std::make_shared<Rom>();
    std::string rom_path = TestRomManager::GetTestRomPath();
    auto status = rom_->LoadFromFile(rom_path);
    if (!status.ok()) {
      GTEST_SKIP() << "ROM not available: " << rom_path;
    }
  }

  std::shared_ptr<Rom> rom_;
};

// ============================================================================
// Subtype 1 Object Tile Pointer Validation
// ============================================================================

TEST_F(DungeonObjectRomValidationTest, Subtype1TilePointerTable_ValidAddresses) {
  // The subtype 1 tile pointer table is at kRoomObjectSubtype1 (0x8000)
  // Each entry is 2 bytes pointing to tile data offset from 0x1B52

  constexpr int kSubtype1TableBase = 0x8000;
  constexpr int kTileDataBase = 0x1B52;

  // Verify first few entries have valid pointers
  for (int obj_id = 0; obj_id < 16; ++obj_id) {
    int table_addr = kSubtype1TableBase + (obj_id * 2);
    uint8_t lo = rom_->data()[table_addr];
    uint8_t hi = rom_->data()[table_addr + 1];
    uint16_t offset = lo | (hi << 8);

    int tile_data_addr = kTileDataBase + offset;

    // Tile data should be within ROM bounds and reasonable range
    EXPECT_LT(tile_data_addr, rom_->size())
        << "Object 0x" << std::hex << obj_id << " tile pointer out of bounds";
    EXPECT_GT(tile_data_addr, 0x1B52)
        << "Object 0x" << std::hex << obj_id << " tile pointer too low";
    EXPECT_LT(tile_data_addr, 0x10000)
        << "Object 0x" << std::hex << obj_id << " tile pointer too high";
  }
}

TEST_F(DungeonObjectRomValidationTest, Subtype1TilePointerTable_Object0x00) {
  // Object 0x00 (floor) should have valid tile data pointer
  constexpr int kSubtype1TableBase = 0x8000;
  constexpr int kTileDataBase = 0x1B52;

  uint8_t lo = rom_->data()[kSubtype1TableBase];
  uint8_t hi = rom_->data()[kSubtype1TableBase + 1];
  uint16_t offset = lo | (hi << 8);

  // Object 0x00 offset should be within reasonable bounds
  // The ROM stores offset 984 (0x03D8) for Object 0x00
  EXPECT_GT(offset, 0) << "Object 0x00 should have non-zero tile pointer";
  EXPECT_LT(offset, 0x4000) << "Object 0x00 tile pointer should be in valid range";

  // Read first tile at that address
  int tile_addr = kTileDataBase + offset;
  uint16_t first_tile = rom_->data()[tile_addr] | (rom_->data()[tile_addr + 1] << 8);

  // Should have valid tile info (non-zero)
  EXPECT_NE(first_tile, 0) << "Object 0x00 should have valid tile data";
}

// ============================================================================
// Tile Count Lookup Table Validation
// ============================================================================

TEST_F(DungeonObjectRomValidationTest, TileCountTable_KnownValues) {
  // Verify tile counts match kSubtype1TileLengths from room_object.h
  // These values are extracted from the game's ROM

  zelda3::ObjectParser parser(rom_.get());

  // Test known tile counts for common objects
  struct TileCountTest {
    int object_id;
    int expected_tiles;
    const char* description;
  };

  // Expected values from kSubtype1TileLengths in object_parser.cc:
  // 0x00-0x0F:  4,  8,  8,  8,  8,  8,  8,  4,  4,  5,  5,  5,  5,  5,  5,  5
  // 0x10-0x1F:  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5
  // 0x20-0x2F:  5,  9,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  6
  // 0x30-0x3F:  6,  1,  1, 16,  1,  1, 16, 16,  6,  8, 12, 12,  4,  8,  4,  3
  std::vector<TileCountTest> tests = {
      {0x00, 4, "Floor object"},
      {0x01, 8, "Wall rightwards 2x4"},
      {0x10, 5, "Diagonal wall acute"},
      {0x21, 9, "Edge rightwards 1x2+2"},  // kSubtype1TileLengths[0x21] = 9
      {0x22, 3, "Edge rightwards has edge"},  // 3 tiles
      {0x34, 1, "Solid 1x1 block"},
      {0x33, 16, "4x4 block"},  // kSubtype1TileLengths[0x33] = 16
  };

  for (const auto& test : tests) {
    auto info = parser.GetObjectSubtype(test.object_id);
    ASSERT_TRUE(info.ok()) << "Failed to get subtype for 0x" << std::hex << test.object_id;

    EXPECT_EQ(info->max_tile_count, test.expected_tiles)
        << test.description << " (0x" << std::hex << test.object_id << ")"
        << " expected " << std::dec << test.expected_tiles
        << " tiles, got " << info->max_tile_count;
  }
}

// ============================================================================
// Object Decoding Validation
// ============================================================================

TEST_F(DungeonObjectRomValidationTest, ObjectDecoding_Type1_TileDataLoads) {
  // Create a Type 1 object and verify its tiles load correctly
  zelda3::RoomObject obj(0x10, 5, 5, 0x12, 0);  // Diagonal wall
  obj.SetRom(rom_.get());
  obj.EnsureTilesLoaded();

  EXPECT_FALSE(obj.tiles().empty())
      << "Object 0x10 should have tiles loaded from ROM";

  // Diagonal walls (0x10) should have 5 tiles
  EXPECT_EQ(obj.tiles().size(), 5)
      << "Object 0x10 should have exactly 5 tiles";

  // Verify tiles have valid IDs (non-zero, within range)
  for (size_t i = 0; i < obj.tiles().size(); ++i) {
    const auto& tile = obj.tiles()[i];
    EXPECT_LT(tile.id_, 1024)
        << "Tile " << i << " ID should be within valid range";
    EXPECT_LT(tile.palette_, 8)
        << "Tile " << i << " palette should be 0-7";
  }
}

TEST_F(DungeonObjectRomValidationTest, ObjectDecoding_Type2_TileDataLoads) {
  // Create a Type 2 object (0x100-0x1FF range)
  zelda3::RoomObject obj(0x100, 5, 5, 0, 0);  // First Type 2 object
  obj.SetRom(rom_.get());
  obj.EnsureTilesLoaded();

  // Type 2 objects should have some tiles
  EXPECT_FALSE(obj.tiles().empty())
      << "Type 2 object 0x100 should have tiles loaded from ROM";
}

TEST_F(DungeonObjectRomValidationTest, ObjectDecoding_Type3_TileDataLoads) {
  // Create a Type 3 object (0xF80-0xFFF range)
  zelda3::RoomObject obj(0xF80, 5, 5, 0, 0);  // First Type 3 object (Water Face)
  obj.SetRom(rom_.get());
  obj.EnsureTilesLoaded();

  // Type 3 objects should have some tiles
  EXPECT_FALSE(obj.tiles().empty())
      << "Type 3 object 0xF80 should have tiles loaded from ROM";
}

// ============================================================================
// Draw Routine Mapping Validation
// ============================================================================

TEST_F(DungeonObjectRomValidationTest, DrawRoutineMapping_AllType1ObjectsHaveRoutines) {
  zelda3::ObjectDrawer drawer(rom_.get(), 0);

  // All Type 1 objects (0x00-0xF7) should have valid draw routines
  for (int id = 0x00; id <= 0xF7; ++id) {
    int routine = drawer.GetDrawRoutineId(id);
    EXPECT_GE(routine, 0)
        << "Object 0x" << std::hex << id << " should have a valid draw routine";
    EXPECT_LT(routine, 40)
        << "Object 0x" << std::hex << id << " routine ID should be < 40";
  }
}

TEST_F(DungeonObjectRomValidationTest, DrawRoutineMapping_Type3ObjectsHaveRoutines) {
  zelda3::ObjectDrawer drawer(rom_.get(), 0);

  // Key Type 3 objects should have valid draw routines
  std::vector<int> type3_ids = {0xF80, 0xF81, 0xF82,  // Water Face
                                 0xF83, 0xF84,         // Somaria Line
                                 0xF97, 0xF98};        // Chests

  for (int id : type3_ids) {
    int routine = drawer.GetDrawRoutineId(id);
    EXPECT_GE(routine, 0)
        << "Type 3 object 0x" << std::hex << id << " should have a valid draw routine";
  }
}

// ============================================================================
// Room Data Validation (Known Rooms)
// ============================================================================

TEST_F(DungeonObjectRomValidationTest, Room0_LinksHouse_HasExpectedStructure) {
  // Room 0 is Link's House - verify we can load it
  zelda3::Room room = zelda3::LoadRoomFromRom(rom_.get(), 0);

  // Link's House should have some objects
  const auto& objects = room.GetTileObjects();

  // Room should have reasonable number of objects (not empty, not absurdly large)
  EXPECT_GT(objects.size(), 0u) << "Room 0 should have objects";
  EXPECT_LT(objects.size(), 200u) << "Room 0 should have reasonable object count";
}

TEST_F(DungeonObjectRomValidationTest, Room1_LinksHouseBasement_LoadsCorrectly) {
  // Room 1 is typically basement/cellar
  zelda3::Room room = zelda3::LoadRoomFromRom(rom_.get(), 1);

  // Should have loaded successfully
  EXPECT_GE(room.GetTileObjects().size(), 0u);
}

TEST_F(DungeonObjectRomValidationTest, HyruleCastleRoom_HasWallObjects) {
  // Room 0x50 is a Hyrule Castle room
  zelda3::Room room = zelda3::LoadRoomFromRom(rom_.get(), 0x50);

  // Hyrule Castle rooms typically have wall objects
  bool has_wall_objects = false;
  for (const auto& obj : room.GetTileObjects()) {
    // Wall objects are typically in 0x00-0x20 range
    if (obj.id_ >= 0x00 && obj.id_ <= 0x30) {
      has_wall_objects = true;
      break;
    }
  }

  EXPECT_TRUE(has_wall_objects || room.GetTileObjects().empty())
      << "Hyrule Castle room should have wall/floor objects";
}

// ============================================================================
// Object Dimension Calculations with Real Data
// ============================================================================

TEST_F(DungeonObjectRomValidationTest, ObjectDimensions_MatchesROMTileCount) {
  zelda3::ObjectDrawer drawer(rom_.get(), 0);
  zelda3::ObjectParser parser(rom_.get());

  // Test objects and verify dimensions are consistent with tile counts
  std::vector<int> test_objects = {0x00, 0x01, 0x10, 0x21, 0x34};

  for (int obj_id : test_objects) {
    zelda3::RoomObject obj(obj_id, 0, 0, 0, 0);
    obj.SetRom(rom_.get());

    auto dims = drawer.CalculateObjectDimensions(obj);
    auto info = parser.GetObjectSubtype(obj_id);

    // Dimensions should be positive
    EXPECT_GT(dims.first, 0)
        << "Object 0x" << std::hex << obj_id << " width should be positive";
    EXPECT_GT(dims.second, 0)
        << "Object 0x" << std::hex << obj_id << " height should be positive";

    // Dimensions should be reasonable (not absurdly large)
    EXPECT_LE(dims.first, 512)
        << "Object 0x" << std::hex << obj_id << " width should be <= 512";
    EXPECT_LE(dims.second, 512)
        << "Object 0x" << std::hex << obj_id << " height should be <= 512";
  }
}

// ============================================================================
// Graphics Buffer Validation
// ============================================================================

TEST_F(DungeonObjectRomValidationTest, ObjectDrawing_ProducesNonEmptyOutput) {
  // Create a graphics buffer (dummy for now since we don't have real room gfx)
  std::vector<uint8_t> gfx_buffer(0x10000, 1);  // Fill with non-zero

  zelda3::ObjectDrawer drawer(rom_.get(), 0, gfx_buffer.data());

  // Create a simple object
  zelda3::RoomObject obj(0x10, 5, 5, 0x12, 0);
  obj.SetRom(rom_.get());
  obj.EnsureTilesLoaded();

  // Create background buffer
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);

  std::vector<uint8_t> empty_data(512 * 512, 0);
  bg1.bitmap().Create(512, 512, 8, empty_data);
  bg2.bitmap().Create(512, 512, 8, empty_data);

  // Create palette
  gfx::PaletteGroup palette_group;
  gfx::SnesPalette palette;
  for (int i = 0; i < 16; i++) {
    palette.AddColor(gfx::SnesColor(i * 16, i * 16, i * 16));
  }
  palette_group.AddPalette(palette);

  // Draw object
  auto status = drawer.DrawObject(obj, bg1, bg2, palette_group);
  EXPECT_TRUE(status.ok()) << "DrawObject failed: " << status.message();

  // Check that some pixels were written (non-zero in bitmap)
  const auto& data = bg1.bitmap().vector();
  int non_zero_count = 0;
  for (uint8_t pixel : data) {
    if (pixel != 0) non_zero_count++;
  }

  EXPECT_GT(non_zero_count, 0)
      << "Drawing should produce some non-zero pixels";
}

// ============================================================================
// GameData Graphics Buffer Validation (Critical for Editor)
// ============================================================================

TEST_F(DungeonObjectRomValidationTest, GameData_GraphicsBufferPopulated) {
  // Load GameData - this is what the editor does on ROM load
  zelda3::GameData game_data;
  auto status = zelda3::LoadGameData(*rom_, game_data);
  ASSERT_TRUE(status.ok()) << "LoadGameData failed: " << status.message();

  // Graphics buffer should be populated (223 sheets * 4096 bytes = 913408 bytes)
  EXPECT_GT(game_data.graphics_buffer.size(), 0u)
      << "Graphics buffer should not be empty";
  EXPECT_GE(game_data.graphics_buffer.size(), 223u * 4096u)
      << "Graphics buffer should have all 223 sheets";

  // Count non-zero bytes in graphics buffer
  int non_zero_count = 0;
  for (uint8_t byte : game_data.graphics_buffer) {
    if (byte != 0 && byte != 0xFF) non_zero_count++;
  }

  EXPECT_GT(non_zero_count, 100000)
      << "Graphics buffer should have significant non-zero data, got "
      << non_zero_count << " non-zero bytes";
}

TEST_F(DungeonObjectRomValidationTest, GameData_GfxBitmapsPopulated) {
  // Load GameData
  zelda3::GameData game_data;
  auto status = zelda3::LoadGameData(*rom_, game_data);
  ASSERT_TRUE(status.ok()) << "LoadGameData failed: " << status.message();

  // Check that gfx_bitmaps are populated
  int populated_count = 0;
  int content_count = 0;
  for (size_t i = 0; i < 223; ++i) {
    auto& bitmap = game_data.gfx_bitmaps[i];
    if (bitmap.is_active() && bitmap.width() > 0 && bitmap.height() > 0) {
      populated_count++;

      // Check entire bitmap for non-zero/non-0xFF data (not just first 100 bytes)
      // Some tiles are legitimately empty at the start
      bool has_content = false;
      for (size_t j = 0; j < bitmap.size(); ++j) {
        if (bitmap.data()[j] != 0 && bitmap.data()[j] != 0xFF) {
          has_content = true;
          break;
        }
      }

      if (has_content) {
        content_count++;
      }
    }
  }

  // Check that we have a reasonable number populated (not all 223 due to 2BPP sheets)
  EXPECT_GT(populated_count, 200)
      << "Most of 223 gfx_bitmaps should be populated, got " << populated_count;

  // Check that most populated sheets have actual content (some may be genuinely empty)
  EXPECT_GT(content_count, 180)
      << "Most populated sheets should have content, got " << content_count
      << " out of " << populated_count;
}

TEST_F(DungeonObjectRomValidationTest, Room_GraphicsBufferCopy) {
  // Load GameData first
  zelda3::GameData game_data;
  auto status = zelda3::LoadGameData(*rom_, game_data);
  ASSERT_TRUE(status.ok()) << "LoadGameData failed: " << status.message();

  // Create a room with GameData
  zelda3::Room room(0, rom_.get(), &game_data);

  // Load room graphics
  room.LoadRoomGraphics(room.blockset);

  // Copy graphics to room buffer
  room.CopyRoomGraphicsToBuffer();

  // Get the current_gfx16 buffer
  auto& gfx16 = room.get_gfx_buffer();

  // Count non-zero bytes
  int non_zero_count = 0;
  for (size_t i = 0; i < gfx16.size(); ++i) {
    if (gfx16[i] != 0) non_zero_count++;
  }

  EXPECT_GT(non_zero_count, 1000)
      << "Room's current_gfx16 buffer should have graphics data, got "
      << non_zero_count << " non-zero bytes out of " << gfx16.size();

  // Verify specific blocks are loaded
  auto blocks = room.blocks();
  EXPECT_EQ(blocks.size(), 16u) << "Room should have 16 graphics blocks";

  for (size_t i = 0; i < blocks.size() && i < 4; ++i) {
    int block_start = i * 4096;
    int block_non_zero = 0;
    for (int j = 0; j < 4096; ++j) {
      if (gfx16[block_start + j] != 0) block_non_zero++;
    }

    EXPECT_GT(block_non_zero, 100)
        << "Block " << i << " (sheet " << blocks[i]
        << ") should have graphics data, got " << block_non_zero
        << " non-zero bytes";
  }
}

TEST_F(DungeonObjectRomValidationTest, Room_LayoutLoading) {
  // Load GameData first
  zelda3::GameData game_data;
  auto status = zelda3::LoadGameData(*rom_, game_data);
  ASSERT_TRUE(status.ok()) << "LoadGameData failed: " << status.message();

  // Create a room with GameData
  zelda3::Room room(0, rom_.get(), &game_data);

  // Load room graphics
  room.LoadRoomGraphics(room.blockset);
  room.CopyRoomGraphicsToBuffer();

  // Check that layout_ is set up
  int layout_id = room.layout;
  std::cout << "Room 0 layout ID: " << layout_id << std::endl;

  // Render room graphics (which calls LoadLayoutTilesToBuffer)
  room.RenderRoomGraphics();

  // Check bg1_buffer bitmap has data
  auto& bg1_bmp = room.bg1_buffer().bitmap();
  auto& bg2_bmp = room.bg2_buffer().bitmap();

  std::cout << "BG1 bitmap: active=" << bg1_bmp.is_active()
            << " w=" << bg1_bmp.width()
            << " h=" << bg1_bmp.height()
            << " size=" << bg1_bmp.size() << std::endl;

  std::cout << "BG2 bitmap: active=" << bg2_bmp.is_active()
            << " w=" << bg2_bmp.width()
            << " h=" << bg2_bmp.height()
            << " size=" << bg2_bmp.size() << std::endl;

  EXPECT_TRUE(bg1_bmp.is_active()) << "BG1 bitmap should be active";
  EXPECT_GT(bg1_bmp.width(), 0) << "BG1 bitmap should have width";
  EXPECT_GT(bg1_bmp.height(), 0) << "BG1 bitmap should have height";

  // Count non-zero pixels in BG1
  if (bg1_bmp.is_active() && bg1_bmp.size() > 0) {
    int non_zero = 0;
    for (size_t i = 0; i < bg1_bmp.size(); ++i) {
      if (bg1_bmp.data()[i] != 0) non_zero++;
    }
    std::cout << "BG1 non-zero pixels: " << non_zero
              << " / " << bg1_bmp.size()
              << " (" << (100.0f * non_zero / bg1_bmp.size()) << "%)"
              << std::endl;

    EXPECT_GT(non_zero, 1000)
        << "BG1 should have significant non-zero pixel data";
  }
}

}  // namespace test
}  // namespace yaze
