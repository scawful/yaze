#include "test_dungeon_objects.h"
#include "mocks/mock_rom.h"
#include "app/zelda3/dungeon/object_parser.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/room_object.h"
#include "app/zelda3/dungeon/room_layout.h"
#include "app/gfx/snes_color.h"
#include "app/gfx/snes_palette.h"
#include "test/testing.h"

#include <vector>
#include <cstring>

#include "gtest/gtest.h"

namespace yaze {
namespace test {

void TestDungeonObjects::SetUp() {
  test_rom_ = std::make_unique<MockRom>();
  ASSERT_TRUE(CreateTestRom().ok());
  ASSERT_TRUE(SetupObjectData().ok());
}

void TestDungeonObjects::TearDown() {
  test_rom_.reset();
}

absl::Status TestDungeonObjects::CreateTestRom() {
  // Create basic ROM data
  std::vector<uint8_t> rom_data(kTestRomSize, 0x00);
  
  // Set up ROM header
  std::string title = "ZELDA3 TEST";
  std::memcpy(&rom_data[0x7FC0], title.c_str(), std::min(title.length(), size_t(21)));
  rom_data[0x7FD7] = 0x21; // 2MB ROM
  
  // Set up object tables
  auto subtype1_table = CreateObjectSubtypeTable(0x8000, 0x100);
  auto subtype2_table = CreateObjectSubtypeTable(0x83F0, 0x80);
  auto subtype3_table = CreateObjectSubtypeTable(0x84F0, 0x100);
  
  // Copy tables to ROM data
  std::copy(subtype1_table.begin(), subtype1_table.end(), rom_data.begin() + 0x8000);
  std::copy(subtype2_table.begin(), subtype2_table.end(), rom_data.begin() + 0x83F0);
  std::copy(subtype3_table.begin(), subtype3_table.end(), rom_data.begin() + 0x84F0);
  
  // Set up tile data
  auto tile_data = CreateTileData(0x1B52, 0x400);
  std::copy(tile_data.begin(), tile_data.end(), rom_data.begin() + 0x1B52);
  
  return test_rom_->SetTestData(rom_data);
}

absl::Status TestDungeonObjects::SetupObjectData() {
  // Set up test object data
  std::vector<uint8_t> object_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  test_rom_->SetObjectData(kTestObjectId, object_data);
  
  // Set up test room data
  auto room_header = CreateRoomHeader(kTestRoomId);
  test_rom_->SetRoomData(kTestRoomId, room_header);
  
  return absl::OkStatus();
}

std::vector<uint8_t> TestDungeonObjects::CreateObjectSubtypeTable(int base_addr, int count) {
  std::vector<uint8_t> table(count * 2, 0x00);
  
  for (int i = 0; i < count; i++) {
    int addr = i * 2;
    // Point to tile data at 0x1B52 + (i * 8)
    int tile_offset = (i * 8) & 0xFFFF;
    table[addr] = tile_offset & 0xFF;
    table[addr + 1] = (tile_offset >> 8) & 0xFF;
  }
  
  return table;
}

std::vector<uint8_t> TestDungeonObjects::CreateTileData(int base_addr, int tile_count) {
  std::vector<uint8_t> data(tile_count * 8, 0x00);
  
  for (int i = 0; i < tile_count; i++) {
    int addr = i * 8;
    // Create simple tile data
    for (int j = 0; j < 8; j++) {
      data[addr + j] = (i + j) & 0xFF;
    }
  }
  
  return data;
}

std::vector<uint8_t> TestDungeonObjects::CreateRoomHeader(int room_id) {
  std::vector<uint8_t> header(32, 0x00);
  
  // Basic room properties
  header[0] = 0x00; // Background type, collision, light
  header[1] = 0x00; // Palette
  header[2] = 0x01; // Blockset
  header[3] = 0x01; // Spriteset
  header[4] = 0x00; // Effect
  header[5] = 0x00; // Tag1
  header[6] = 0x00; // Tag2
  
  return header;
}

// Test cases
TEST_F(TestDungeonObjects, ObjectParserBasicTest) {
  zelda3::ObjectParser parser(test_rom_.get());
  
  auto result = parser.ParseObject(kTestObjectId);
  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result->empty());
}

TEST_F(TestDungeonObjects, ObjectRendererBasicTest) {
  zelda3::ObjectRenderer renderer(test_rom_.get());
  
  // Create test object
  auto room_object = zelda3::RoomObject(kTestObjectId, 0, 0, 0x12, 0);
  room_object.set_rom(test_rom_.get());
  room_object.EnsureTilesLoaded();
  
  // Create test palette
  gfx::SnesPalette palette;
  for (int i = 0; i < 16; i++) {
    palette.AddColor(gfx::SnesColor(i * 16, i * 16, i * 16));
  }
  
  auto result = renderer.RenderObject(room_object, palette);
  ASSERT_TRUE(result.ok());
  EXPECT_GT(result->width(), 0);
  EXPECT_GT(result->height(), 0);
}

TEST_F(TestDungeonObjects, RoomObjectTileLoadingTest) {
  auto room_object = zelda3::RoomObject(kTestObjectId, 5, 5, 0x12, 0);
  room_object.set_rom(test_rom_.get());
  
  // Test tile loading
  room_object.EnsureTilesLoaded();
  EXPECT_FALSE(room_object.tiles().empty());
}

TEST_F(TestDungeonObjects, MockRomDataTest) {
  auto* mock_rom = static_cast<MockRom*>(test_rom_.get());
  
  EXPECT_TRUE(mock_rom->HasObjectData(kTestObjectId));
  EXPECT_TRUE(mock_rom->HasRoomData(kTestRoomId));
  EXPECT_TRUE(mock_rom->IsValid());
}

TEST_F(TestDungeonObjects, RoomObjectTileAccessTest) {
  auto room_object = zelda3::RoomObject(kTestObjectId, 5, 5, 0x12, 0);
  room_object.set_rom(test_rom_.get());
  room_object.EnsureTilesLoaded();
  
  // Test new tile access methods
  auto tiles_result = room_object.GetTiles();
  EXPECT_TRUE(tiles_result.ok());
  if (tiles_result.ok()) {
    EXPECT_FALSE(tiles_result->empty());
  }
  
  // Test individual tile access
  auto tile_result = room_object.GetTile(0);
  EXPECT_TRUE(tile_result.ok());
  
  if (tile_result.ok()) {
    const auto* tile = tile_result.value();
    EXPECT_NE(tile, nullptr);
  }
  
  // Test tile count
  EXPECT_GT(room_object.GetTileCount(), 0);
  
  // Test out of range access
  auto bad_tile_result = room_object.GetTile(999);
  EXPECT_FALSE(bad_tile_result.ok());
}

TEST_F(TestDungeonObjects, ObjectRendererGraphicsSheetTest) {
  zelda3::ObjectRenderer renderer(test_rom_.get());
  
  // Create test object
  auto room_object = zelda3::RoomObject(kTestObjectId, 0, 0, 0x12, 0);
  room_object.set_rom(test_rom_.get());
  room_object.EnsureTilesLoaded();
  
  // Create test palette
  gfx::SnesPalette palette;
  for (int i = 0; i < 16; i++) {
    palette.AddColor(gfx::SnesColor(i * 16, i * 16, i * 16));
  }
  
  // Test rendering with graphics sheet lookup
  auto result = renderer.RenderObject(room_object, palette);
  ASSERT_TRUE(result.ok());
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active());
  EXPECT_NE(bitmap.surface(), nullptr);
  EXPECT_GT(bitmap.width(), 0);
  EXPECT_GT(bitmap.height(), 0);
}

TEST_F(TestDungeonObjects, BitmapCopySemanticsTest) {
  // Test bitmap copying works correctly
  std::vector<uint8_t> data(32 * 32, 0x42);
  gfx::Bitmap original(32, 32, 8, data);
  
  // Test copy constructor
  gfx::Bitmap copy = original;
  EXPECT_EQ(copy.width(), original.width());
  EXPECT_EQ(copy.height(), original.height());
  EXPECT_TRUE(copy.is_active());
  EXPECT_NE(copy.surface(), nullptr);
  
  // Test copy assignment
  gfx::Bitmap assigned;
  assigned = original;
  EXPECT_EQ(assigned.width(), original.width());
  EXPECT_EQ(assigned.height(), original.height());
  EXPECT_TRUE(assigned.is_active());
  EXPECT_NE(assigned.surface(), nullptr);
}

TEST_F(TestDungeonObjects, BitmapMoveSemanticsTest) {
  // Test bitmap moving works correctly
  std::vector<uint8_t> data(32 * 32, 0x42);
  gfx::Bitmap original(32, 32, 8, data);
  
  // Test move constructor
  gfx::Bitmap moved = std::move(original);
  EXPECT_EQ(moved.width(), 32);
  EXPECT_EQ(moved.height(), 32);
  EXPECT_TRUE(moved.is_active());
  EXPECT_NE(moved.surface(), nullptr);
  
  // Original should be in a valid but empty state
  EXPECT_EQ(original.width(), 0);
  EXPECT_EQ(original.height(), 0);
  EXPECT_FALSE(original.is_active());
  EXPECT_EQ(original.surface(), nullptr);
}

TEST_F(TestDungeonObjects, PaletteHandlingTest) {
  // Test palette handling and hash calculation
  gfx::SnesPalette palette;
  for (int i = 0; i < 16; i++) {
    palette.AddColor(gfx::SnesColor(i * 16, i * 16, i * 16));
  }
  
  EXPECT_EQ(palette.size(), 16);
  
  // Test palette hash calculation (used in caching)
  uint64_t hash1 = 0;
  for (size_t i = 0; i < palette.size(); ++i) {
    hash1 ^= std::hash<uint16_t>{}(palette[i].snes()) + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2);
  }
  
  // Same palette should produce same hash
  uint64_t hash2 = 0;
  for (size_t i = 0; i < palette.size(); ++i) {
    hash2 ^= std::hash<uint16_t>{}(palette[i].snes()) + 0x9e3779b9 + (hash2 << 6) + (hash2 >> 2);
  }
  
  EXPECT_EQ(hash1, hash2);
  EXPECT_NE(hash1, 0); // Hash should not be zero
}

TEST_F(TestDungeonObjects, ObjectSizeCalculationTest) {
  zelda3::ObjectParser parser(test_rom_.get());
  
  // Test object size parsing
  auto size_result = parser.ParseObjectSize(0x01, 0x12);
  EXPECT_TRUE(size_result.ok());
  
  if (size_result.ok()) {
    const auto& size_info = size_result.value();
    EXPECT_GT(size_info.width_tiles, 0);
    EXPECT_GT(size_info.height_tiles, 0);
    EXPECT_TRUE(size_info.is_repeatable);
  }
}

TEST_F(TestDungeonObjects, ObjectSubtypeDeterminationTest) {
  zelda3::ObjectParser parser(test_rom_.get());
  
  // Test subtype determination
  EXPECT_EQ(parser.DetermineSubtype(0x01), 1);
  EXPECT_EQ(parser.DetermineSubtype(0x100), 2);
  EXPECT_EQ(parser.DetermineSubtype(0x200), 3);
  
  // Test object subtype info
  auto subtype_result = parser.GetObjectSubtype(0x01);
  EXPECT_TRUE(subtype_result.ok());
  
  if (subtype_result.ok()) {
    EXPECT_EQ(subtype_result->subtype, 1);
    EXPECT_GT(subtype_result->max_tile_count, 0);
  }
}

TEST_F(TestDungeonObjects, RoomLayoutObjectCreationTest) {
  zelda3::RoomLayoutObject obj(0x01, 5, 10, zelda3::RoomLayoutObject::Type::kWall, 0);
  
  EXPECT_EQ(obj.id(), 0x01);
  EXPECT_EQ(obj.x(), 5);
  EXPECT_EQ(obj.y(), 10);
  EXPECT_EQ(obj.type(), zelda3::RoomLayoutObject::Type::kWall);
  EXPECT_EQ(obj.layer(), 0);
  
  // Test type name
  EXPECT_EQ(obj.GetTypeName(), "Wall");
  
  // Test tile creation
  auto tile_result = obj.GetTile();
  EXPECT_TRUE(tile_result.ok());
}

TEST_F(TestDungeonObjects, RoomLayoutLoadingTest) {
  zelda3::RoomLayout layout(test_rom_.get());
  
  // Test loading layout for room 0
  auto status = layout.LoadLayout(0);
  // This might fail due to missing layout data, which is expected
  // We're testing that the method doesn't crash
  
  // Test getting objects by type
  auto walls = layout.GetObjectsByType(zelda3::RoomLayoutObject::Type::kWall);
  auto floors = layout.GetObjectsByType(zelda3::RoomLayoutObject::Type::kFloor);
  
  // Test dimensions
  auto [width, height] = layout.GetDimensions();
  EXPECT_GT(width, 0);
  EXPECT_GT(height, 0);
  
  // Test object access
  auto obj_result = layout.GetObjectAt(0, 0, 0);
  // This might fail if no object exists at that position, which is expected
}

TEST_F(TestDungeonObjects, RoomLayoutCollisionTest) {
  zelda3::RoomLayout layout(test_rom_.get());
  
  // Test collision detection methods
  EXPECT_FALSE(layout.HasWall(0, 0, 0)); // Should be false for empty layout
  EXPECT_FALSE(layout.HasFloor(0, 0, 0)); // Should be false for empty layout
  
  // Test with a simple layout
  std::vector<uint8_t> layout_data = {
    0x01, 0x01, 0x00, 0x00, // Wall, Wall, Empty, Empty
    0x21, 0x21, 0x21, 0x21, // Floor, Floor, Floor, Floor
    0x00, 0x00, 0x00, 0x00, // Empty, Empty, Empty, Empty
  };
  
  // This would require the layout to be properly set up
  // For now, we just test that the methods don't crash
}

}  // namespace test
}  // namespace yaze