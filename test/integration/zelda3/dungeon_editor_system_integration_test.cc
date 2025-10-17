#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <map>
#include <chrono>

#include "app/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/dungeon_object_editor.h"

namespace yaze {
namespace zelda3 {

class DungeonEditorSystemIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Skip tests on Linux for automated github builds
#if defined(__linux__)
    GTEST_SKIP();
#endif
    
    // Use the real ROM from build directory
    rom_path_ = "build/bin/zelda3.sfc";
    
    // Load ROM
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromFile(rom_path_).ok());
    
    // Initialize dungeon editor system
    dungeon_editor_system_ = std::make_unique<DungeonEditorSystem>(rom_.get());
    ASSERT_TRUE(dungeon_editor_system_->Initialize().ok());
    
    // Load test room data
    ASSERT_TRUE(LoadTestRoomData().ok());
  }

  void TearDown() override {
    dungeon_editor_system_.reset();
    rom_.reset();
  }

  absl::Status LoadTestRoomData() {
    // Load representative rooms for testing
    test_rooms_ = {0x0000, 0x0001, 0x0002, 0x0010, 0x0012, 0x0020};
    
    for (int room_id : test_rooms_) {
      auto room_result = dungeon_editor_system_->GetRoom(room_id);
      if (room_result.ok()) {
        rooms_[room_id] = room_result.value();
        std::cout << "Loaded room 0x" << std::hex << room_id << std::dec << std::endl;
      }
    }
    
    return absl::OkStatus();
  }

  std::string rom_path_;
  std::unique_ptr<Rom> rom_;
  std::unique_ptr<DungeonEditorSystem> dungeon_editor_system_;
  
  std::vector<int> test_rooms_;
  std::map<int, Room> rooms_;
};

// Test basic dungeon editor system initialization
TEST_F(DungeonEditorSystemIntegrationTest, BasicInitialization) {
  EXPECT_NE(dungeon_editor_system_, nullptr);
  EXPECT_EQ(dungeon_editor_system_->GetROM(), rom_.get());
  EXPECT_FALSE(dungeon_editor_system_->IsDirty());
}

// Test room loading and management
TEST_F(DungeonEditorSystemIntegrationTest, RoomLoadingAndManagement) {
  // Test loading a specific room
  auto room_result = dungeon_editor_system_->GetRoom(0x0000);
  ASSERT_TRUE(room_result.ok()) << "Failed to load room 0x0000: " << room_result.status().message();
  
  const auto& room = room_result.value();
  // Note: room_id_ is private, so we can't directly access it in tests
  
  // Test setting current room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());
  EXPECT_EQ(dungeon_editor_system_->GetCurrentRoom(), 0x0000);
  
  // Test loading another room
  auto room2_result = dungeon_editor_system_->GetRoom(0x0001);
  ASSERT_TRUE(room2_result.ok()) << "Failed to load room 0x0001: " << room2_result.status().message();
  
  const auto& room2 = room2_result.value();
  // Note: room_id_ is private, so we can't directly access it in tests
}

// Test object editor integration
TEST_F(DungeonEditorSystemIntegrationTest, ObjectEditorIntegration) {
  // Get object editor from system
  auto object_editor = dungeon_editor_system_->GetObjectEditor();
  ASSERT_NE(object_editor, nullptr);
  
  // Set current room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());
  
  // Test object insertion
  ASSERT_TRUE(object_editor->InsertObject(5, 5, 0x10, 0x12, 0).ok());
  ASSERT_TRUE(object_editor->InsertObject(10, 10, 0x20, 0x22, 1).ok());
  
  // Verify objects were added
  EXPECT_EQ(object_editor->GetObjectCount(), 2);
  
  // Test object selection
  ASSERT_TRUE(object_editor->SelectObject(5 * 16, 5 * 16).ok());
  auto selection = object_editor->GetSelection();
  EXPECT_EQ(selection.selected_objects.size(), 1);
  
  // Test object deletion
  ASSERT_TRUE(object_editor->DeleteSelectedObjects().ok());
  EXPECT_EQ(object_editor->GetObjectCount(), 1);
}

// Test sprite management
TEST_F(DungeonEditorSystemIntegrationTest, SpriteManagement) {
  // Set current room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());
  
  // Create sprite data
  DungeonEditorSystem::SpriteData sprite_data;
  sprite_data.sprite_id = 1;
  sprite_data.name = "Test Sprite";
  sprite_data.type = DungeonEditorSystem::SpriteType::kEnemy;
  sprite_data.x = 100;
  sprite_data.y = 100;
  sprite_data.layer = 0;
  sprite_data.is_active = true;
  
  // Add sprite
  ASSERT_TRUE(dungeon_editor_system_->AddSprite(sprite_data).ok());
  
  // Get sprites for room
  auto sprites_result = dungeon_editor_system_->GetSpritesByRoom(0x0000);
  ASSERT_TRUE(sprites_result.ok()) << "Failed to get sprites: " << sprites_result.status().message();
  
  const auto& sprites = sprites_result.value();
  EXPECT_EQ(sprites.size(), 1);
  EXPECT_EQ(sprites[0].sprite_id, 1);
  EXPECT_EQ(sprites[0].name, "Test Sprite");
  
  // Update sprite
  sprite_data.x = 150;
  ASSERT_TRUE(dungeon_editor_system_->UpdateSprite(1, sprite_data).ok());
  
  // Get updated sprite
  auto sprite_result = dungeon_editor_system_->GetSprite(1);
  ASSERT_TRUE(sprite_result.ok());
  EXPECT_EQ(sprite_result.value().x, 150);
  
  // Remove sprite
  ASSERT_TRUE(dungeon_editor_system_->RemoveSprite(1).ok());
  
  // Verify sprite was removed
  auto sprites_after = dungeon_editor_system_->GetSpritesByRoom(0x0000);
  ASSERT_TRUE(sprites_after.ok());
  EXPECT_EQ(sprites_after.value().size(), 0);
}

// Test item management
TEST_F(DungeonEditorSystemIntegrationTest, ItemManagement) {
  // Set current room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());
  
  // Create item data
  DungeonEditorSystem::ItemData item_data;
  item_data.item_id = 1;
  item_data.type = DungeonEditorSystem::ItemType::kKey;
  item_data.name = "Small Key";
  item_data.x = 200;
  item_data.y = 200;
  item_data.room_id = 0x0000;
  item_data.is_hidden = false;
  
  // Add item
  ASSERT_TRUE(dungeon_editor_system_->AddItem(item_data).ok());
  
  // Get items for room
  auto items_result = dungeon_editor_system_->GetItemsByRoom(0x0000);
  ASSERT_TRUE(items_result.ok()) << "Failed to get items: " << items_result.status().message();
  
  const auto& items = items_result.value();
  EXPECT_EQ(items.size(), 1);
  EXPECT_EQ(items[0].item_id, 1);
  EXPECT_EQ(items[0].name, "Small Key");
  
  // Update item
  item_data.is_hidden = true;
  ASSERT_TRUE(dungeon_editor_system_->UpdateItem(1, item_data).ok());
  
  // Get updated item
  auto item_result = dungeon_editor_system_->GetItem(1);
  ASSERT_TRUE(item_result.ok());
  EXPECT_TRUE(item_result.value().is_hidden);
  
  // Remove item
  ASSERT_TRUE(dungeon_editor_system_->RemoveItem(1).ok());
  
  // Verify item was removed
  auto items_after = dungeon_editor_system_->GetItemsByRoom(0x0000);
  ASSERT_TRUE(items_after.ok());
  EXPECT_EQ(items_after.value().size(), 0);
}

// Test entrance management
TEST_F(DungeonEditorSystemIntegrationTest, EntranceManagement) {
  // Create entrance data
  DungeonEditorSystem::EntranceData entrance_data;
  entrance_data.entrance_id = 1;
  entrance_data.type = DungeonEditorSystem::EntranceType::kDoor;
  entrance_data.name = "Test Entrance";
  entrance_data.source_room_id = 0x0000;
  entrance_data.target_room_id = 0x0001;
  entrance_data.source_x = 100;
  entrance_data.source_y = 100;
  entrance_data.target_x = 200;
  entrance_data.target_y = 200;
  entrance_data.is_bidirectional = true;
  
  // Add entrance
  ASSERT_TRUE(dungeon_editor_system_->AddEntrance(entrance_data).ok());
  
  // Get entrances for room
  auto entrances_result = dungeon_editor_system_->GetEntrancesByRoom(0x0000);
  ASSERT_TRUE(entrances_result.ok()) << "Failed to get entrances: " << entrances_result.status().message();
  
  const auto& entrances = entrances_result.value();
  EXPECT_EQ(entrances.size(), 1);
  EXPECT_EQ(entrances[0].name, "Test Entrance");
  
  // Store the entrance ID for later removal
  int entrance_id = entrances[0].entrance_id;
  
  // Test room connection
  ASSERT_TRUE(dungeon_editor_system_->ConnectRooms(0x0000, 0x0001, 150, 150, 250, 250).ok());
  
  // Get updated entrances
  auto entrances_after = dungeon_editor_system_->GetEntrancesByRoom(0x0000);
  ASSERT_TRUE(entrances_after.ok());
  EXPECT_GE(entrances_after.value().size(), 1);
  
  // Remove entrance using the correct ID
  ASSERT_TRUE(dungeon_editor_system_->RemoveEntrance(entrance_id).ok());
  
  // Verify entrance was removed
  auto entrances_final = dungeon_editor_system_->GetEntrancesByRoom(0x0000);
  ASSERT_TRUE(entrances_final.ok());
  EXPECT_EQ(entrances_final.value().size(), 0);
}

// Test door management
TEST_F(DungeonEditorSystemIntegrationTest, DoorManagement) {
  // Create door data
  DungeonEditorSystem::DoorData door_data;
  door_data.door_id = 1;
  door_data.name = "Test Door";
  door_data.room_id = 0x0000;
  door_data.x = 100;
  door_data.y = 100;
  door_data.direction = 0; // up
  door_data.target_room_id = 0x0001;
  door_data.target_x = 200;
  door_data.target_y = 200;
  door_data.requires_key = false;
  door_data.key_type = 0;
  door_data.is_locked = false;
  
  // Add door
  ASSERT_TRUE(dungeon_editor_system_->AddDoor(door_data).ok());
  
  // Get doors for room
  auto doors_result = dungeon_editor_system_->GetDoorsByRoom(0x0000);
  ASSERT_TRUE(doors_result.ok()) << "Failed to get doors: " << doors_result.status().message();
  
  const auto& doors = doors_result.value();
  EXPECT_EQ(doors.size(), 1);
  EXPECT_EQ(doors[0].door_id, 1);
  EXPECT_EQ(doors[0].name, "Test Door");
  
  // Update door
  door_data.is_locked = true;
  ASSERT_TRUE(dungeon_editor_system_->UpdateDoor(1, door_data).ok());
  
  // Get updated door
  auto door_result = dungeon_editor_system_->GetDoor(1);
  ASSERT_TRUE(door_result.ok());
  EXPECT_TRUE(door_result.value().is_locked);
  
  // Set door key requirement
  ASSERT_TRUE(dungeon_editor_system_->SetDoorKeyRequirement(1, true, 1).ok());
  
  // Get door with key requirement
  auto door_with_key = dungeon_editor_system_->GetDoor(1);
  ASSERT_TRUE(door_with_key.ok());
  EXPECT_TRUE(door_with_key.value().requires_key);
  EXPECT_EQ(door_with_key.value().key_type, 1);
  
  // Remove door
  ASSERT_TRUE(dungeon_editor_system_->RemoveDoor(1).ok());
  
  // Verify door was removed
  auto doors_after = dungeon_editor_system_->GetDoorsByRoom(0x0000);
  ASSERT_TRUE(doors_after.ok());
  EXPECT_EQ(doors_after.value().size(), 0);
}

// Test chest management
TEST_F(DungeonEditorSystemIntegrationTest, ChestManagement) {
  // Create chest data
  DungeonEditorSystem::ChestData chest_data;
  chest_data.chest_id = 1;
  chest_data.room_id = 0x0000;
  chest_data.x = 100;
  chest_data.y = 100;
  chest_data.is_big_chest = false;
  chest_data.item_id = 10;
  chest_data.item_quantity = 1;
  chest_data.is_opened = false;
  
  // Add chest
  ASSERT_TRUE(dungeon_editor_system_->AddChest(chest_data).ok());
  
  // Get chests for room
  auto chests_result = dungeon_editor_system_->GetChestsByRoom(0x0000);
  ASSERT_TRUE(chests_result.ok()) << "Failed to get chests: " << chests_result.status().message();
  
  const auto& chests = chests_result.value();
  EXPECT_EQ(chests.size(), 1);
  EXPECT_EQ(chests[0].chest_id, 1);
  EXPECT_EQ(chests[0].item_id, 10);
  
  // Update chest item
  ASSERT_TRUE(dungeon_editor_system_->SetChestItem(1, 20, 5).ok());
  
  // Get updated chest
  auto chest_result = dungeon_editor_system_->GetChest(1);
  ASSERT_TRUE(chest_result.ok());
  EXPECT_EQ(chest_result.value().item_id, 20);
  EXPECT_EQ(chest_result.value().item_quantity, 5);
  
  // Set chest as opened
  ASSERT_TRUE(dungeon_editor_system_->SetChestOpened(1, true).ok());
  
  // Get opened chest
  auto opened_chest = dungeon_editor_system_->GetChest(1);
  ASSERT_TRUE(opened_chest.ok());
  EXPECT_TRUE(opened_chest.value().is_opened);
  
  // Remove chest
  ASSERT_TRUE(dungeon_editor_system_->RemoveChest(1).ok());
  
  // Verify chest was removed
  auto chests_after = dungeon_editor_system_->GetChestsByRoom(0x0000);
  ASSERT_TRUE(chests_after.ok());
  EXPECT_EQ(chests_after.value().size(), 0);
}

// Test room properties management
TEST_F(DungeonEditorSystemIntegrationTest, RoomPropertiesManagement) {
  // Create room properties
  DungeonEditorSystem::RoomProperties properties;
  properties.room_id = 0x0000;
  properties.name = "Test Room";
  properties.description = "A test room for integration testing";
  properties.dungeon_id = 1;
  properties.floor_level = 0;
  properties.is_boss_room = false;
  properties.is_save_room = false;
  properties.is_shop_room = false;
  properties.music_id = 1;
  properties.ambient_sound_id = 0;
  
  // Set room properties
  ASSERT_TRUE(dungeon_editor_system_->SetRoomProperties(0x0000, properties).ok());
  
  // Get room properties
  auto properties_result = dungeon_editor_system_->GetRoomProperties(0x0000);
  ASSERT_TRUE(properties_result.ok()) << "Failed to get room properties: " << properties_result.status().message();
  
  const auto& retrieved_properties = properties_result.value();
  EXPECT_EQ(retrieved_properties.room_id, 0x0000);
  EXPECT_EQ(retrieved_properties.name, "Test Room");
  EXPECT_EQ(retrieved_properties.description, "A test room for integration testing");
  EXPECT_EQ(retrieved_properties.dungeon_id, 1);
  
  // Update properties
  properties.name = "Updated Test Room";
  properties.is_boss_room = true;
  ASSERT_TRUE(dungeon_editor_system_->SetRoomProperties(0x0000, properties).ok());
  
  // Verify update
  auto updated_properties = dungeon_editor_system_->GetRoomProperties(0x0000);
  ASSERT_TRUE(updated_properties.ok());
  EXPECT_EQ(updated_properties.value().name, "Updated Test Room");
  EXPECT_TRUE(updated_properties.value().is_boss_room);
}

// Test dungeon settings management
TEST_F(DungeonEditorSystemIntegrationTest, DungeonSettingsManagement) {
  // Create dungeon settings
  DungeonEditorSystem::DungeonSettings settings;
  settings.dungeon_id = 1;
  settings.name = "Test Dungeon";
  settings.description = "A test dungeon for integration testing";
  settings.total_rooms = 10;
  settings.starting_room_id = 0x0000;
  settings.boss_room_id = 0x0001;
  settings.music_theme_id = 1;
  settings.color_palette_id = 0;
  settings.has_map = true;
  settings.has_compass = true;
  settings.has_big_key = true;
  
  // Set dungeon settings
  ASSERT_TRUE(dungeon_editor_system_->SetDungeonSettings(settings).ok());
  
  // Get dungeon settings
  auto settings_result = dungeon_editor_system_->GetDungeonSettings();
  ASSERT_TRUE(settings_result.ok()) << "Failed to get dungeon settings: " << settings_result.status().message();
  
  const auto& retrieved_settings = settings_result.value();
  EXPECT_EQ(retrieved_settings.dungeon_id, 1);
  EXPECT_EQ(retrieved_settings.name, "Test Dungeon");
  EXPECT_EQ(retrieved_settings.total_rooms, 10);
  EXPECT_EQ(retrieved_settings.starting_room_id, 0x0000);
  EXPECT_EQ(retrieved_settings.boss_room_id, 0x0001);
  EXPECT_TRUE(retrieved_settings.has_map);
  EXPECT_TRUE(retrieved_settings.has_compass);
  EXPECT_TRUE(retrieved_settings.has_big_key);
}

// Test undo/redo functionality
TEST_F(DungeonEditorSystemIntegrationTest, UndoRedoFunctionality) {
  // Set current room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());
  
  // Get object editor
  auto object_editor = dungeon_editor_system_->GetObjectEditor();
  ASSERT_NE(object_editor, nullptr);
  
  // Add some objects
  ASSERT_TRUE(object_editor->InsertObject(5, 5, 0x10, 0x12, 0).ok());
  ASSERT_TRUE(object_editor->InsertObject(10, 10, 0x20, 0x22, 1).ok());
  
  // Verify objects were added
  EXPECT_EQ(object_editor->GetObjectCount(), 2);
  
  // Test undo
  ASSERT_TRUE(dungeon_editor_system_->Undo().ok());
  EXPECT_EQ(object_editor->GetObjectCount(), 1);
  
  // Test redo
  ASSERT_TRUE(dungeon_editor_system_->Redo().ok());
  EXPECT_EQ(object_editor->GetObjectCount(), 2);
  
  // Test multiple undos
  ASSERT_TRUE(dungeon_editor_system_->Undo().ok());
  ASSERT_TRUE(dungeon_editor_system_->Undo().ok());
  EXPECT_EQ(object_editor->GetObjectCount(), 0);
  
  // Test multiple redos
  ASSERT_TRUE(dungeon_editor_system_->Redo().ok());
  ASSERT_TRUE(dungeon_editor_system_->Redo().ok());
  EXPECT_EQ(object_editor->GetObjectCount(), 2);
}

// Test validation functionality
TEST_F(DungeonEditorSystemIntegrationTest, ValidationFunctionality) {
  // Set current room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());
  
  // Validate room
  auto room_validation = dungeon_editor_system_->ValidateRoom(0x0000);
  ASSERT_TRUE(room_validation.ok()) << "Room validation failed: " << room_validation.message();
  
  // Validate dungeon
  auto dungeon_validation = dungeon_editor_system_->ValidateDungeon();
  ASSERT_TRUE(dungeon_validation.ok()) << "Dungeon validation failed: " << dungeon_validation.message();
}

// Test save/load functionality
TEST_F(DungeonEditorSystemIntegrationTest, SaveLoadFunctionality) {
  // Set current room and add some objects
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());
  
  auto object_editor = dungeon_editor_system_->GetObjectEditor();
  ASSERT_NE(object_editor, nullptr);
  
  ASSERT_TRUE(object_editor->InsertObject(5, 5, 0x10, 0x12, 0).ok());
  ASSERT_TRUE(object_editor->InsertObject(10, 10, 0x20, 0x22, 1).ok());
  
  // Save room
  ASSERT_TRUE(dungeon_editor_system_->SaveRoom(0x0000).ok());
  
  // Reload room
  ASSERT_TRUE(dungeon_editor_system_->ReloadRoom(0x0000).ok());
  
  // Verify objects are still there
  auto reloaded_objects = object_editor->GetObjects();
  EXPECT_EQ(reloaded_objects.size(), 2);
  
  // Save entire dungeon
  ASSERT_TRUE(dungeon_editor_system_->SaveDungeon().ok());
}

// Test performance with multiple operations
TEST_F(DungeonEditorSystemIntegrationTest, PerformanceTest) {
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Perform many operations
  for (int i = 0; i < 100; i++) {
    // Add sprite
    DungeonEditorSystem::SpriteData sprite_data;
    sprite_data.sprite_id = i;
    sprite_data.type = DungeonEditorSystem::SpriteType::kEnemy;
    sprite_data.x = i * 10;
    sprite_data.y = i * 10;
    sprite_data.layer = 0;
    
    ASSERT_TRUE(dungeon_editor_system_->AddSprite(sprite_data).ok());
    
    // Add item
    DungeonEditorSystem::ItemData item_data;
    item_data.item_id = i;
    item_data.type = DungeonEditorSystem::ItemType::kKey;
    item_data.x = i * 15;
    item_data.y = i * 15;
    item_data.room_id = 0x0000;
    
    ASSERT_TRUE(dungeon_editor_system_->AddItem(item_data).ok());
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Should complete in reasonable time (less than 5 seconds for 200 operations)
  EXPECT_LT(duration.count(), 5000) << "Performance test too slow: " << duration.count() << "ms";
  
  std::cout << "Performance test: 200 operations took " << duration.count() << "ms" << std::endl;
}

// Test error handling
TEST_F(DungeonEditorSystemIntegrationTest, ErrorHandling) {
  // Test with invalid room ID
  auto invalid_room = dungeon_editor_system_->GetRoom(-1);
  EXPECT_FALSE(invalid_room.ok());
  
  auto invalid_room_large = dungeon_editor_system_->GetRoom(10000);
  EXPECT_FALSE(invalid_room_large.ok());
  
  // Test with invalid sprite ID
  auto invalid_sprite = dungeon_editor_system_->GetSprite(-1);
  EXPECT_FALSE(invalid_sprite.ok());
  
  // Test with invalid item ID
  auto invalid_item = dungeon_editor_system_->GetItem(-1);
  EXPECT_FALSE(invalid_item.ok());
  
  // Test with invalid entrance ID
  auto invalid_entrance = dungeon_editor_system_->GetEntrance(-1);
  EXPECT_FALSE(invalid_entrance.ok());
  
  // Test with invalid door ID
  auto invalid_door = dungeon_editor_system_->GetDoor(-1);
  EXPECT_FALSE(invalid_door.ok());
  
  // Test with invalid chest ID
  auto invalid_chest = dungeon_editor_system_->GetChest(-1);
  EXPECT_FALSE(invalid_chest.ok());
}

}  // namespace zelda3
}  // namespace yaze
