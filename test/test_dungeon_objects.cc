#include "test_dungeon_objects.h"
#include "mocks/mock_rom.h"
#include "app/zelda3/dungeon/object_parser.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/room_object.h"

#include <vector>

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
  
  static_cast<MockRom*>(test_rom_.get())->SetTestData(rom_data);
  
  return absl::OkStatus();
}

absl::Status TestDungeonObjects::SetupObjectData() {
  // Set up test object data
  std::vector<uint8_t> object_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  static_cast<MockRom*>(test_rom_.get())->SetObjectData(kTestObjectId, object_data);
  
  // Set up test room data
  auto room_header = CreateRoomHeader(kTestRoomId);
  static_cast<MockRom*>(test_rom_.get())->SetRoomData(kTestRoomId, room_header);
  
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
  ObjectParser parser(test_rom_.get());
  
  auto result = parser.ParseObject(kTestObjectId);
  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result->empty());
}

TEST_F(TestDungeonObjects, ObjectRendererBasicTest) {
  ObjectRenderer renderer(test_rom_.get());
  
  // Create test object
  auto room_object = RoomObject(kTestObjectId, 0, 0, 0x12, 0);
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
  auto room_object = RoomObject(kTestObjectId, 5, 5, 0x12, 0);
  room_object.set_rom(test_rom_.get());
  
  // Test tile loading
  room_object.EnsureTilesLoaded();
  EXPECT_FALSE(room_object.tiles_.empty());
}

TEST_F(TestDungeonObjects, MockRomDataTest) {
  auto* mock_rom = static_cast<MockRom*>(test_rom_.get());
  
  EXPECT_TRUE(mock_rom->HasObjectData(kTestObjectId));
  EXPECT_TRUE(mock_rom->HasRoomData(kTestRoomId));
  EXPECT_TRUE(mock_rom->IsValid());
}

}  // namespace test
}  // namespace yaze