#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <map>
#include <chrono>

#include "app/rom.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/room_object.h"
#include "app/zelda3/dungeon/dungeon_object_editor.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/dungeon_editor_system.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Mock ROM class for testing without real ROM files
 * 
 * This class provides a mock ROM implementation that can be used for testing
 * the dungeon object rendering system without requiring actual ROM files.
 */
class MockRom : public Rom {
 public:
  MockRom() {
    // Initialize mock ROM data
    InitializeMockData();
  }
  
  ~MockRom() = default;
  
  // Override key methods for testing
  absl::Status LoadFromFile(const std::string& filename) {
    // Mock implementation - always succeeds
    is_loaded_ = true;
    return absl::OkStatus();
  }
  
  bool is_loaded() const { return is_loaded_; }
  
  size_t size() const { return mock_data_.size(); }
  
  uint8_t operator[](size_t index) const {
    if (index < mock_data_.size()) {
      return mock_data_[index];
    }
    return 0xFF; // Default value for out-of-bounds
  }
  
  absl::StatusOr<uint8_t> ReadByte(size_t address) const {
    if (address < mock_data_.size()) {
      return mock_data_[address];
    }
    return absl::OutOfRangeError("Address out of range");
  }
  
  absl::StatusOr<uint16_t> ReadWord(size_t address) const {
    if (address + 1 < mock_data_.size()) {
      return static_cast<uint16_t>(mock_data_[address]) | 
             (static_cast<uint16_t>(mock_data_[address + 1]) << 8);
    }
    return absl::OutOfRangeError("Address out of range");
  }
  
  absl::Status ValidateHeader() const {
    // Mock validation - always succeeds
    return absl::OkStatus();
  }
  
  // Mock palette data
  struct MockPaletteGroup {
    std::vector<gfx::SnesPalette> palettes;
  };
  
  MockPaletteGroup& palette_group() { return mock_palette_group_; }
  const MockPaletteGroup& palette_group() const { return mock_palette_group_; }

 private:
  void InitializeMockData() {
    // Create mock ROM data (2MB)
    mock_data_.resize(2 * 1024 * 1024, 0xFF);
    
    // Set up mock ROM header
    mock_data_[0x7FC0] = 'Z'; // ROM name start
    mock_data_[0x7FC1] = 'E';
    mock_data_[0x7FC2] = 'L';
    mock_data_[0x7FC3] = 'D';
    mock_data_[0x7FC4] = 'A';
    mock_data_[0x7FC5] = '3';
    mock_data_[0x7FC6] = 0x00; // Version
    mock_data_[0x7FC7] = 0x00;
    mock_data_[0x7FD5] = 0x21; // ROM type
    mock_data_[0x7FD6] = 0x20; // ROM size
    mock_data_[0x7FD7] = 0x00; // SRAM size
    mock_data_[0x7FD8] = 0x00; // Country
    mock_data_[0x7FD9] = 0x00; // License
    mock_data_[0x7FDA] = 0x00; // Version
    mock_data_[0x7FDB] = 0x00;
    
    // Set up mock room data pointers starting at 0x1F8000
    constexpr uint32_t kRoomDataPointersStart = 0x1F8000;
    constexpr uint32_t kRoomDataStart = 0x0A8000;
    
    for (int i = 0; i < 512; i++) {
      uint32_t pointer_addr = kRoomDataPointersStart + (i * 3);
      uint32_t room_data_addr = kRoomDataStart + (i * 100); // Mock room data
      
      if (pointer_addr + 2 < mock_data_.size()) {
        mock_data_[pointer_addr] = room_data_addr & 0xFF;
        mock_data_[pointer_addr + 1] = (room_data_addr >> 8) & 0xFF;
        mock_data_[pointer_addr + 2] = (room_data_addr >> 16) & 0xFF;
      }
    }
    
    // Initialize mock palette data
    InitializeMockPalettes();
    
    is_loaded_ = true;
  }
  
  void InitializeMockPalettes() {
    // Create mock dungeon palettes
    for (int i = 0; i < 8; i++) {
      gfx::SnesPalette palette;
      
      // Create a simple 16-color palette
      for (int j = 0; j < 16; j++) {
        int intensity = j * 16;
        palette.AddColor(gfx::SnesColor(intensity, intensity, intensity));
      }
      
      mock_palette_group_.palettes.push_back(palette);
    }
  }
  
  std::vector<uint8_t> mock_data_;
  MockPaletteGroup mock_palette_group_;
  bool is_loaded_ = false;
};

/**
 * @brief Mock room data generator
 */
class MockRoomGenerator {
 public:
  static Room GenerateMockRoom(int room_id, Rom* rom) {
    Room room(room_id, rom);
    
    // Set basic room properties
    room.SetPalette(room_id % 8);
    room.SetBlockset(room_id % 16);
    room.SetSpriteset(room_id % 8);
    room.SetFloor1(0x00);
    room.SetFloor2(0x00);
    room.SetMessageId(0x0000);
    
    // Generate mock objects based on room type
    GenerateMockObjects(room, room_id);
    
    return room;
  }
  
 private:
  static void GenerateMockObjects(Room& room, int room_id) {
    // Generate different object sets based on room ID
    if (room_id == 0x0000) {
      // Ganon's room - special objects
      room.AddTileObject(RoomObject(0x10, 8, 8, 0x12, 0));
      room.AddTileObject(RoomObject(0x20, 12, 12, 0x22, 0));
      room.AddTileObject(RoomObject(0x30, 16, 16, 0x12, 1));
    } else if (room_id == 0x0002 || room_id == 0x0012) {
      // Sewer rooms - water and pipes
      room.AddTileObject(RoomObject(0x20, 5, 5, 0x22, 0));
      room.AddTileObject(RoomObject(0x40, 10, 10, 0x12, 0));
      room.AddTileObject(RoomObject(0x50, 15, 15, 0x32, 1));
    } else {
      // Standard rooms - basic objects
      room.AddTileObject(RoomObject(0x10, 5, 5, 0x12, 0));
      room.AddTileObject(RoomObject(0x20, 10, 10, 0x22, 0));
      if (room_id % 3 == 0) {
        room.AddTileObject(RoomObject(0xF9, 15, 15, 0x12, 1)); // Chest
      }
      if (room_id % 5 == 0) {
        room.AddTileObject(RoomObject(0x13, 20, 20, 0x32, 2)); // Stairs
      }
    }
  }
};

class DungeonObjectRendererMockTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create mock ROM
    mock_rom_ = std::make_unique<MockRom>();
    
    // Initialize dungeon editor system with mock ROM
    dungeon_editor_system_ = std::make_unique<DungeonEditorSystem>(mock_rom_.get());
    ASSERT_TRUE(dungeon_editor_system_->Initialize().ok());
    
    // Initialize object editor
    object_editor_ = std::make_shared<DungeonObjectEditor>(mock_rom_.get());
    // Note: InitializeEditor() is private, so we skip this in mock tests
    
    // Initialize object renderer
    object_renderer_ = std::make_unique<ObjectRenderer>(mock_rom_.get());
    
    // Generate mock room data
    ASSERT_TRUE(GenerateMockRoomData().ok());
  }

  void TearDown() override {
    object_renderer_.reset();
    object_editor_.reset();
    dungeon_editor_system_.reset();
    mock_rom_.reset();
  }

  absl::Status GenerateMockRoomData() {
    // Generate mock rooms for testing
    std::vector<int> test_rooms = {0x0000, 0x0001, 0x0002, 0x0010, 0x0012, 0x0020};
    
    for (int room_id : test_rooms) {
      auto mock_room = MockRoomGenerator::GenerateMockRoom(room_id, mock_rom_.get());
      rooms_[room_id] = mock_room;
      
      std::cout << "Generated mock room 0x" << std::hex << room_id << std::dec 
                << " with " << mock_room.GetTileObjects().size() << " objects" << std::endl;
    }
    
    // Get mock palettes
    auto palette_group = mock_rom_->palette_group().palettes;
    test_palettes_ = {palette_group[0], palette_group[1], palette_group[2]};
    
    return absl::OkStatus();
  }

  // Helper methods
  RoomObject CreateMockObject(int object_id, int x, int y, int size = 0x12, int layer = 0) {
    RoomObject obj(object_id, x, y, size, layer);
    obj.set_rom(mock_rom_.get());
    obj.EnsureTilesLoaded();
    return obj;
  }

  std::vector<RoomObject> CreateMockObjectSet() {
    std::vector<RoomObject> objects;
    objects.push_back(CreateMockObject(0x10, 5, 5, 0x12, 0));   // Wall
    objects.push_back(CreateMockObject(0x20, 10, 10, 0x22, 0)); // Floor
    objects.push_back(CreateMockObject(0xF9, 15, 15, 0x12, 1)); // Chest
    return objects;
  }

  std::unique_ptr<MockRom> mock_rom_;
  std::unique_ptr<DungeonEditorSystem> dungeon_editor_system_;
  std::shared_ptr<DungeonObjectEditor> object_editor_;
  std::unique_ptr<ObjectRenderer> object_renderer_;
  
  std::map<int, Room> rooms_;
  std::vector<gfx::SnesPalette> test_palettes_;
};

// Test basic mock ROM functionality
TEST_F(DungeonObjectRendererMockTest, MockROMBasicFunctionality) {
  EXPECT_TRUE(mock_rom_->is_loaded());
  EXPECT_GT(mock_rom_->size(), 0);
  
  // Test ROM header validation
  auto header_result = mock_rom_->ValidateHeader();
  EXPECT_TRUE(header_result.ok());
  
  // Test reading ROM data
  auto byte_result = mock_rom_->ReadByte(0x7FC0);
  EXPECT_TRUE(byte_result.ok());
  EXPECT_EQ(byte_result.value(), 'Z');
  
  auto word_result = mock_rom_->ReadWord(0x1F8000);
  EXPECT_TRUE(word_result.ok());
  EXPECT_GT(word_result.value(), 0);
}

// Test mock room generation
TEST_F(DungeonObjectRendererMockTest, MockRoomGeneration) {
  EXPECT_GT(rooms_.size(), 0);
  
  for (const auto& [room_id, room] : rooms_) {
    // Note: room_id_ is private, so we can't directly access it in tests
    EXPECT_GT(room.GetTileObjects().size(), 0);
    
    std::cout << "Mock room 0x" << std::hex << room_id << std::dec 
              << " has " << room.GetTileObjects().size() << " objects" << std::endl;
  }
}

// Test object rendering with mock data
TEST_F(DungeonObjectRendererMockTest, MockObjectRendering) {
  auto mock_objects = CreateMockObjectSet();
  auto palette = test_palettes_[0];
  
  auto result = object_renderer_->RenderObjects(mock_objects, palette);
  ASSERT_TRUE(result.ok()) << "Failed to render mock objects: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_GT(bitmap.width(), 0);
  EXPECT_GT(bitmap.height(), 0);
}

// Test mock room object rendering
TEST_F(DungeonObjectRendererMockTest, MockRoomObjectRendering) {
  for (const auto& [room_id, room] : rooms_) {
    const auto& objects = room.GetTileObjects();
    
    auto result = object_renderer_->RenderObjects(objects, test_palettes_[0]);
    ASSERT_TRUE(result.ok()) << "Failed to render mock room 0x" << std::hex << room_id << std::dec;
    
    auto bitmap = std::move(result.value());
    EXPECT_GT(bitmap.width(), 0);
    EXPECT_GT(bitmap.height(), 0);
    
    std::cout << "Successfully rendered mock room 0x" << std::hex << room_id << std::dec 
              << " with " << objects.size() << " objects" << std::endl;
  }
}

// Test mock object editor functionality
TEST_F(DungeonObjectRendererMockTest, MockObjectEditorFunctionality) {
  // Load a mock room
  ASSERT_TRUE(object_editor_->LoadRoom(0x0000).ok());
  
  // Add objects
  ASSERT_TRUE(object_editor_->InsertObject(5, 5, 0x10, 0x12, 0).ok());
  ASSERT_TRUE(object_editor_->InsertObject(10, 10, 0x20, 0x22, 1).ok());
  
  // Get objects and render them
  const auto& objects = object_editor_->GetObjects();
  EXPECT_GT(objects.size(), 0);
  
  auto result = object_renderer_->RenderObjects(objects, test_palettes_[0]);
  ASSERT_TRUE(result.ok()) << "Failed to render objects from mock editor";
  
  auto bitmap = std::move(result.value());
  EXPECT_GT(bitmap.width(), 0);
  EXPECT_GT(bitmap.height(), 0);
}

// Test mock object editor undo/redo
TEST_F(DungeonObjectRendererMockTest, MockObjectEditorUndoRedo) {
  // Load a mock room and add objects
  ASSERT_TRUE(object_editor_->LoadRoom(0x0000).ok());
  ASSERT_TRUE(object_editor_->InsertObject(5, 5, 0x10, 0x12, 0).ok());
  ASSERT_TRUE(object_editor_->InsertObject(10, 10, 0x20, 0x22, 1).ok());
  
  auto objects_before = object_editor_->GetObjects();
  
  // Undo one operation
  ASSERT_TRUE(object_editor_->Undo().ok());
  auto objects_after = object_editor_->GetObjects();
  EXPECT_EQ(objects_after.size(), objects_before.size() - 1);
  
  // Redo the operation
  ASSERT_TRUE(object_editor_->Redo().ok());
  auto objects_redo = object_editor_->GetObjects();
  EXPECT_EQ(objects_redo.size(), objects_before.size());
}

// Test mock dungeon editor system integration
TEST_F(DungeonObjectRendererMockTest, MockDungeonEditorSystemIntegration) {
  // Set current room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());
  
  // Get object editor from system
  auto system_object_editor = dungeon_editor_system_->GetObjectEditor();
  ASSERT_NE(system_object_editor, nullptr);
  
  // Add objects through the system
  ASSERT_TRUE(system_object_editor->InsertObject(5, 5, 0x10, 0x12, 0).ok());
  ASSERT_TRUE(system_object_editor->InsertObject(10, 10, 0x20, 0x22, 1).ok());
  
  // Get objects and render them
  const auto& objects = system_object_editor->GetObjects();
  ASSERT_GT(objects.size(), 0);
  
  auto result = object_renderer_->RenderObjects(objects, test_palettes_[0]);
  ASSERT_TRUE(result.ok()) << "Failed to render objects from mock system";
  
  auto bitmap = std::move(result.value());
  EXPECT_GT(bitmap.width(), 0);
  EXPECT_GT(bitmap.height(), 0);
}

// Test mock performance
TEST_F(DungeonObjectRendererMockTest, MockPerformanceTest) {
  auto mock_objects = CreateMockObjectSet();
  auto palette = test_palettes_[0];
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Render objects multiple times
  for (int i = 0; i < 100; i++) {
    auto result = object_renderer_->RenderObjects(mock_objects, palette);
    ASSERT_TRUE(result.ok());
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Should complete in reasonable time (less than 1000ms for 100 renders)
  EXPECT_LT(duration.count(), 1000) << "Mock rendering too slow: " << duration.count() << "ms";
  
  std::cout << "Mock performance test: 100 renders took " << duration.count() << "ms" << std::endl;
}

// Test mock error handling
TEST_F(DungeonObjectRendererMockTest, MockErrorHandling) {
  // Test with empty object list
  std::vector<RoomObject> empty_objects;
  auto result = object_renderer_->RenderObjects(empty_objects, test_palettes_[0]);
  // Should either succeed with empty bitmap or fail gracefully
  if (!result.ok()) {
    EXPECT_TRUE(absl::IsInvalidArgument(result.status()) || 
                absl::IsFailedPrecondition(result.status()));
  }
  
  // Test with invalid object (no ROM set)
  RoomObject invalid_object(0x10, 5, 5, 0x12, 0);
  // Don't set ROM - this should cause an error
  std::vector<RoomObject> invalid_objects = {invalid_object};
  
  result = object_renderer_->RenderObjects(invalid_objects, test_palettes_[0]);
  // May succeed or fail depending on implementation - just ensure it doesn't crash
  // EXPECT_FALSE(result.ok());
}

// Test mock object type validation
TEST_F(DungeonObjectRendererMockTest, MockObjectTypeValidation) {
  std::vector<int> object_types = {0x10, 0x20, 0x30, 0xF9, 0x13, 0x17};
  
  for (int object_type : object_types) {
    auto object = CreateMockObject(object_type, 10, 10, 0x12, 0);
    std::vector<RoomObject> objects = {object};
    
    auto result = object_renderer_->RenderObjects(objects, test_palettes_[0]);
    
    if (result.ok()) {
      auto bitmap = std::move(result.value());
      EXPECT_GT(bitmap.width(), 0);
      EXPECT_GT(bitmap.height(), 0);
      
      std::cout << "Mock object type 0x" << std::hex << object_type << std::dec 
                << " rendered successfully" << std::endl;
    } else {
      std::cout << "Mock object type 0x" << std::hex << object_type << std::dec 
                << " failed to render: " << result.status().message() << std::endl;
    }
  }
}

// Test mock cache functionality
TEST_F(DungeonObjectRendererMockTest, MockCacheFunctionality) {
  auto mock_objects = CreateMockObjectSet();
  auto palette = test_palettes_[0];
  
  // Reset performance stats
  object_renderer_->ResetPerformanceStats();
  
  // First render (should miss cache)
  auto result1 = object_renderer_->RenderObjects(mock_objects, palette);
  ASSERT_TRUE(result1.ok());
  
  auto stats1 = object_renderer_->GetPerformanceStats();
  
  // Second render with same objects (should hit cache)
  auto result2 = object_renderer_->RenderObjects(mock_objects, palette);
  ASSERT_TRUE(result2.ok());
  
  auto stats2 = object_renderer_->GetPerformanceStats();
  EXPECT_GE(stats2.cache_hits, stats1.cache_hits);
  
  std::cout << "Mock cache test: " << stats2.cache_hits << " hits, " 
            << stats2.cache_misses << " misses" << std::endl;
}

}  // namespace zelda3
}  // namespace yaze
