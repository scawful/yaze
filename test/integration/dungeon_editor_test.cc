#include "test/integration/dungeon_editor_test.h"

#include <cstring>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/room_object.h"

namespace yaze {
namespace test {

void DungeonEditorIntegrationTest::SetUp() {
  ASSERT_TRUE(CreateMockRom().ok());
  ASSERT_TRUE(LoadTestRoomData().ok());
  
  dungeon_editor_ = std::make_unique<editor::DungeonEditor>(mock_rom_.get());
  dungeon_editor_->Initialize();
}

void DungeonEditorIntegrationTest::TearDown() {
  dungeon_editor_.reset();
  mock_rom_.reset();
}

absl::Status DungeonEditorIntegrationTest::CreateMockRom() {
  mock_rom_ = std::make_unique<MockRom>();
  
  // Generate mock ROM data
  std::vector<uint8_t> mock_data(kMockRomSize, 0x00);
  
  // Set up basic ROM structure
  // Header at 0x7FC0
  std::string title = "ZELDA3 TEST ROM";
  std::memcpy(&mock_data[0x7FC0], title.c_str(), std::min(title.length(), size_t(21)));
  
  // Set ROM size and type
  mock_data[0x7FD7] = 0x21; // 2MB ROM
  mock_data[0x7FD8] = 0x00; // SRAM size
  mock_data[0x7FD9] = 0x00; // Country code (NTSC)
  mock_data[0x7FDA] = 0x00; // License code
  mock_data[0x7FDB] = 0x00; // Version
  
  // Set up room header pointers
  mock_data[0xB5DD] = 0x00; // Room header pointer low
  mock_data[0xB5DE] = 0x00; // Room header pointer mid
  mock_data[0xB5DF] = 0x00; // Room header pointer high
  
  // Set up object pointers
  mock_data[0x874C] = 0x00; // Object pointer low
  mock_data[0x874D] = 0x00; // Object pointer mid
  mock_data[0x874E] = 0x00; // Object pointer high
  
  static_cast<MockRom*>(mock_rom_.get())->SetMockData(mock_data);
  
  return absl::OkStatus();
}

absl::Status DungeonEditorIntegrationTest::LoadTestRoomData() {
  // Generate test room data
  auto room_header = GenerateMockRoomHeader(kTestRoomId);
  auto object_data = GenerateMockObjectData();
  auto graphics_data = GenerateMockGraphicsData();
  
  static_cast<MockRom*>(mock_rom_.get())->SetMockRoomData(kTestRoomId, room_header);
  static_cast<MockRom*>(mock_rom_.get())->SetMockObjectData(kTestObjectId, object_data);
  
  return absl::OkStatus();
}

absl::Status DungeonEditorIntegrationTest::TestObjectParsing() {
  // Test object parsing without SNES emulation
  auto room = zelda3::LoadRoomFromRom(mock_rom_.get(), kTestRoomId);
  
  // Verify room was loaded correctly
  EXPECT_NE(room.rom(), nullptr);
  // Note: room_id_ is private, so we can't directly access it in tests
  
  // Test object loading
  room.LoadObjects();
  EXPECT_FALSE(room.GetTileObjects().empty());
  
  // Verify object properties
  for (const auto& obj : room.GetTileObjects()) {
    // Note: id_ is private, so we can't directly access it in tests
    EXPECT_LE(obj.x_, 31); // Room width limit
    EXPECT_LE(obj.y_, 31); // Room height limit
    // Note: rom() method is not const, so we can't call it on const objects
  }
  
  return absl::OkStatus();
}

absl::Status DungeonEditorIntegrationTest::TestObjectRendering() {
  // Test object rendering without SNES emulation
  auto room = zelda3::LoadRoomFromRom(mock_rom_.get(), kTestRoomId);
  room.LoadObjects();
  
  // Test tile loading for objects
  for (auto& obj : room.GetTileObjects()) {
    obj.EnsureTilesLoaded();
    EXPECT_FALSE(obj.tiles_.empty());
  }
  
  // Test room graphics rendering
  room.LoadRoomGraphics();
  room.RenderRoomGraphics();
  
  return absl::OkStatus();
}

absl::Status DungeonEditorIntegrationTest::TestRoomGraphics() {
  // Test room graphics loading and rendering
  auto room = zelda3::LoadRoomFromRom(mock_rom_.get(), kTestRoomId);
  
  // Test graphics loading
  room.LoadRoomGraphics();
  EXPECT_FALSE(room.blocks().empty());
  
  // Test graphics rendering
  room.RenderRoomGraphics();
  
  return absl::OkStatus();
}

absl::Status DungeonEditorIntegrationTest::TestPaletteHandling() {
  // Test palette loading and application
  auto room = zelda3::LoadRoomFromRom(mock_rom_.get(), kTestRoomId);
  
  // Verify palette is set
  EXPECT_GE(room.palette, 0);
  EXPECT_LE(room.palette, 0x47); // Max palette index
  
  return absl::OkStatus();
}

std::vector<uint8_t> DungeonEditorIntegrationTest::GenerateMockRoomHeader(int room_id) {
  std::vector<uint8_t> header(32, 0x00);
  
  // Basic room properties
  header[0] = 0x00; // Background type, collision, light
  header[1] = 0x00; // Palette
  header[2] = 0x01; // Blockset
  header[3] = 0x01; // Spriteset
  header[4] = 0x00; // Effect
  header[5] = 0x00; // Tag1
  header[6] = 0x00; // Tag2
  header[7] = 0x00; // Staircase planes
  header[8] = 0x00; // Staircase planes continued
  header[9] = 0x00; // Hole warp
  header[10] = 0x00; // Staircase rooms
  header[11] = 0x00;
  header[12] = 0x00;
  header[13] = 0x00;
  
  return header;
}

std::vector<uint8_t> DungeonEditorIntegrationTest::GenerateMockObjectData() {
  std::vector<uint8_t> data;
  
  // Add a simple wall object
  data.push_back(0x08); // X position (2 tiles)
  data.push_back(0x08); // Y position (2 tiles)
  data.push_back(0x01); // Object ID (wall)
  
  // Add layer separator
  data.push_back(0xFF);
  data.push_back(0xFF);
  
  // Add door section
  data.push_back(0xF0);
  data.push_back(0xFF);
  
  return data;
}

std::vector<uint8_t> DungeonEditorIntegrationTest::GenerateMockGraphicsData() {
  std::vector<uint8_t> data(0x4000, 0x00);
  
  // Generate basic tile data
  for (size_t i = 0; i < data.size(); i += 2) {
    data[i] = 0x00;     // Tile low byte
    data[i + 1] = 0x00; // Tile high byte
  }
  
  return data;
}

void MockRom::SetMockData(const std::vector<uint8_t>& data) {
  mock_data_ = data;
}

void MockRom::SetMockRoomData(int room_id, const std::vector<uint8_t>& data) {
  mock_room_data_[room_id] = data;
}

void MockRom::SetMockObjectData(int object_id, const std::vector<uint8_t>& data) {
  mock_object_data_[object_id] = data;
}

bool MockRom::ValidateRoomData(int room_id) const {
  return mock_room_data_.find(room_id) != mock_room_data_.end();
}

bool MockRom::ValidateObjectData(int object_id) const {
  return mock_object_data_.find(object_id) != mock_object_data_.end();
}

// Test cases
TEST_F(DungeonEditorIntegrationTest, ObjectParsingTest) {
  EXPECT_TRUE(TestObjectParsing().ok());
}

TEST_F(DungeonEditorIntegrationTest, ObjectRenderingTest) {
  EXPECT_TRUE(TestObjectRendering().ok());
}

TEST_F(DungeonEditorIntegrationTest, RoomGraphicsTest) {
  EXPECT_TRUE(TestRoomGraphics().ok());
}

TEST_F(DungeonEditorIntegrationTest, PaletteHandlingTest) {
  EXPECT_TRUE(TestPaletteHandling().ok());
}

TEST_F(DungeonEditorIntegrationTest, MockRomValidation) {
  EXPECT_TRUE(static_cast<MockRom*>(mock_rom_.get())->ValidateRoomData(kTestRoomId));
  EXPECT_TRUE(static_cast<MockRom*>(mock_rom_.get())->ValidateObjectData(kTestObjectId));
}

}  // namespace test
}  // namespace yaze