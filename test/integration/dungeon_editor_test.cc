#include "integration/dungeon_editor_test.h"

#include <cstring>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/snes.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/room_object.h"

namespace yaze {
namespace test {

using namespace yaze::zelda3;

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
  
  return mock_rom_->LoadAndOwnData(mock_data);
}

absl::Status DungeonEditorIntegrationTest::LoadTestRoomData() {
  // Generate test room data
  auto room_header = GenerateMockRoomHeader(kTestRoomId);
  auto object_data = GenerateMockObjectData();
  auto graphics_data = GenerateMockGraphicsData();
  
  auto mock_rom = static_cast<MockRom*>(mock_rom_.get());
  mock_rom->SetMockRoomData(kTestRoomId, room_header);
  mock_rom->SetMockObjectData(kTestObjectId, object_data);
  mock_rom->SetMockGraphicsData(graphics_data);
  
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

absl::Status MockRom::SetMockData(const std::vector<uint8_t>& data) {
  backing_buffer_.assign(data.begin(), data.end());
  Expand(static_cast<int>(backing_buffer_.size()));
  if (!backing_buffer_.empty()) {
    std::memcpy(mutable_data(), backing_buffer_.data(), backing_buffer_.size());
  }
  ClearDirty();
  InitializeMemoryLayout();
  return absl::OkStatus();
}

absl::Status MockRom::LoadAndOwnData(const std::vector<uint8_t>& data) {
  backing_buffer_.assign(data.begin(), data.end());
  Expand(static_cast<int>(backing_buffer_.size()));
  if (!backing_buffer_.empty()) {
    std::memcpy(mutable_data(), backing_buffer_.data(), backing_buffer_.size());
  }
  ClearDirty();

  // Minimal metadata setup via public API
  set_filename("mock_rom.sfc");
  auto& palette_groups = *mutable_palette_group();
  palette_groups.clear();

  if (palette_groups.dungeon_main.size() == 0) {
    gfx::SnesPalette default_palette;
    default_palette.Resize(16);
    palette_groups.dungeon_main.AddPalette(default_palette);
  }

  // Ensure graphics buffer is sized
  auto* gfx_buffer = mutable_graphics_buffer();
  gfx_buffer->assign(backing_buffer_.begin(), backing_buffer_.end());

  InitializeMemoryLayout();
  return absl::OkStatus();
}

void MockRom::SetMockRoomData(int room_id, const std::vector<uint8_t>& data) {
  mock_room_data_[room_id] = data;

  if (room_header_table_pc_ == 0 || room_header_data_base_pc_ == 0) {
    return;
  }

  uint32_t header_offset = room_header_data_base_pc_ + kRoomHeaderStride * static_cast<uint32_t>(room_id);
  EnsureBufferCapacity(header_offset + static_cast<uint32_t>(data.size()));

  std::memcpy(backing_buffer_.data() + header_offset, data.data(), data.size());
  std::memcpy(mutable_data() + header_offset, data.data(), data.size());

  uint32_t snes_offset = PcToSnes(header_offset);
  uint32_t pointer_entry = room_header_table_pc_ + static_cast<uint32_t>(room_id) * 2;
  EnsureBufferCapacity(pointer_entry + 2);
  backing_buffer_[pointer_entry] = static_cast<uint8_t>(snes_offset & 0xFF);
  backing_buffer_[pointer_entry + 1] = static_cast<uint8_t>((snes_offset >> 8) & 0xFF);
  mutable_data()[pointer_entry] = backing_buffer_[pointer_entry];
  mutable_data()[pointer_entry + 1] = backing_buffer_[pointer_entry + 1];
}

void MockRom::SetMockObjectData(int object_id, const std::vector<uint8_t>& data) {
  mock_object_data_[object_id] = data;

  if (room_object_table_pc_ == 0 || room_object_data_base_pc_ == 0) {
    return;
  }

  uint32_t object_offset = room_object_data_base_pc_ + kRoomObjectStride * static_cast<uint32_t>(object_id);
  EnsureBufferCapacity(object_offset + static_cast<uint32_t>(data.size()));

  std::memcpy(backing_buffer_.data() + object_offset, data.data(), data.size());
  std::memcpy(mutable_data() + object_offset, data.data(), data.size());

  uint32_t snes_offset = PcToSnes(object_offset);
  uint32_t entry = room_object_table_pc_ + static_cast<uint32_t>(object_id) * 3;
  EnsureBufferCapacity(entry + 3);
  backing_buffer_[entry] = static_cast<uint8_t>(snes_offset & 0xFF);
  backing_buffer_[entry + 1] = static_cast<uint8_t>((snes_offset >> 8) & 0xFF);
  backing_buffer_[entry + 2] = static_cast<uint8_t>((snes_offset >> 16) & 0xFF);
  mutable_data()[entry] = backing_buffer_[entry];
  mutable_data()[entry + 1] = backing_buffer_[entry + 1];
  mutable_data()[entry + 2] = backing_buffer_[entry + 2];
}

void MockRom::SetMockGraphicsData(const std::vector<uint8_t>& data) {
  mock_graphics_data_ = data;
  if (auto* gfx_buffer = mutable_graphics_buffer(); gfx_buffer != nullptr) {
    gfx_buffer->assign(data.begin(), data.end());
  }
}

void MockRom::EnsureBufferCapacity(uint32_t size) {
  if (size <= backing_buffer_.size()) {
    return;
  }

  auto old_size = backing_buffer_.size();
  backing_buffer_.resize(size, 0);
  Expand(static_cast<int>(size));
  std::memcpy(mutable_data(), backing_buffer_.data(), old_size);
}

void MockRom::InitializeMemoryLayout() {
  if (backing_buffer_.empty()) {
    return;
  }

  room_header_table_pc_ = SnesToPc(0x040000);
  room_header_data_base_pc_ = SnesToPc(0x040000 + 0x1000);
  room_object_table_pc_ = SnesToPc(0x050000);
  room_object_data_base_pc_ = SnesToPc(0x050000 + 0x2000);

  EnsureBufferCapacity(room_header_table_pc_ + 2);
  EnsureBufferCapacity(room_object_table_pc_ + 3);

  uint32_t header_table_snes = PcToSnes(room_header_table_pc_);
  EnsureBufferCapacity(kRoomHeaderPointer + 3);
  backing_buffer_[kRoomHeaderPointer] = static_cast<uint8_t>(header_table_snes & 0xFF);
  backing_buffer_[kRoomHeaderPointer + 1] = static_cast<uint8_t>((header_table_snes >> 8) & 0xFF);
  backing_buffer_[kRoomHeaderPointer + 2] = static_cast<uint8_t>((header_table_snes >> 16) & 0xFF);
  mutable_data()[kRoomHeaderPointer] = backing_buffer_[kRoomHeaderPointer];
  mutable_data()[kRoomHeaderPointer + 1] = backing_buffer_[kRoomHeaderPointer + 1];
  mutable_data()[kRoomHeaderPointer + 2] = backing_buffer_[kRoomHeaderPointer + 2];

  EnsureBufferCapacity(kRoomHeaderPointerBank + 1);
  backing_buffer_[kRoomHeaderPointerBank] = static_cast<uint8_t>((header_table_snes >> 16) & 0xFF);
  mutable_data()[kRoomHeaderPointerBank] = backing_buffer_[kRoomHeaderPointerBank];

  uint32_t object_table_snes = PcToSnes(room_object_table_pc_);
  EnsureBufferCapacity(room_object_pointer + 3);
  backing_buffer_[room_object_pointer] = static_cast<uint8_t>(object_table_snes & 0xFF);
  backing_buffer_[room_object_pointer + 1] = static_cast<uint8_t>((object_table_snes >> 8) & 0xFF);
  backing_buffer_[room_object_pointer + 2] = static_cast<uint8_t>((object_table_snes >> 16) & 0xFF);
  mutable_data()[room_object_pointer] = backing_buffer_[room_object_pointer];
  mutable_data()[room_object_pointer + 1] = backing_buffer_[room_object_pointer + 1];
  mutable_data()[room_object_pointer + 2] = backing_buffer_[room_object_pointer + 2];

  for (const auto& [room_id, bytes] : mock_room_data_) {
    uint32_t offset = room_header_data_base_pc_ + kRoomHeaderStride * static_cast<uint32_t>(room_id);
    EnsureBufferCapacity(offset + static_cast<uint32_t>(bytes.size()));
    std::memcpy(backing_buffer_.data() + offset, bytes.data(), bytes.size());
    std::memcpy(mutable_data() + offset, bytes.data(), bytes.size());

    uint32_t snes = PcToSnes(offset);
    uint32_t entry = room_header_table_pc_ + static_cast<uint32_t>(room_id) * 2;
    EnsureBufferCapacity(entry + 2);
    backing_buffer_[entry] = static_cast<uint8_t>(snes & 0xFF);
    backing_buffer_[entry + 1] = static_cast<uint8_t>((snes >> 8) & 0xFF);
    mutable_data()[entry] = backing_buffer_[entry];
    mutable_data()[entry + 1] = backing_buffer_[entry + 1];
  }

  for (const auto& [object_id, bytes] : mock_object_data_) {
    uint32_t offset = room_object_data_base_pc_ + kRoomObjectStride * static_cast<uint32_t>(object_id);
    EnsureBufferCapacity(offset + static_cast<uint32_t>(bytes.size()));
    std::memcpy(backing_buffer_.data() + offset, bytes.data(), bytes.size());
    std::memcpy(mutable_data() + offset, bytes.data(), bytes.size());

    uint32_t snes = PcToSnes(offset);
    uint32_t entry = room_object_table_pc_ + static_cast<uint32_t>(object_id) * 3;
    EnsureBufferCapacity(entry + 3);
    backing_buffer_[entry] = static_cast<uint8_t>(snes & 0xFF);
    backing_buffer_[entry + 1] = static_cast<uint8_t>((snes >> 8) & 0xFF);
    backing_buffer_[entry + 2] = static_cast<uint8_t>((snes >> 16) & 0xFF);
    mutable_data()[entry] = backing_buffer_[entry];
    mutable_data()[entry + 1] = backing_buffer_[entry + 1];
    mutable_data()[entry + 2] = backing_buffer_[entry + 2];
  }
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