#include <gtest/gtest.h>

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {

class DungeonEditorSystemIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Skip on Linux CI - requires ROM file and graphics context
#if defined(__linux__)
    GTEST_SKIP() << "Dungeon editor tests require ROM file (unavailable on Linux CI)";
#endif

    yaze::test::TestRomManager::SkipIfRomMissing(
        yaze::test::RomRole::kVanilla,
        "DungeonEditorSystemIntegrationTest");
    rom_path_ =
        yaze::test::TestRomManager::GetRomPath(yaze::test::RomRole::kVanilla);
    ASSERT_FALSE(rom_path_.empty())
        << "ROM path not set for vanilla role. "
        << yaze::test::TestRomManager::GetRomRoleHint(
               yaze::test::RomRole::kVanilla);

    // Load ROM
    rom_ = std::make_unique<Rom>();
    auto load_status = rom_->LoadFromFile(rom_path_);
    ASSERT_TRUE(load_status.ok())
        << "Failed to load ROM from " << rom_path_ << ": "
        << load_status.message();

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
        rooms_[room_id] = std::move(room_result.value());
        std::cout << "Loaded room 0x" << std::hex << room_id << std::dec
                  << std::endl;
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
  ASSERT_TRUE(room_result.ok())
      << "Failed to load room 0x0000: " << room_result.status().message();

  // Test setting current room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());
  EXPECT_EQ(dungeon_editor_system_->GetCurrentRoom(), 0x0000);

  // Test loading another room
  auto room2_result = dungeon_editor_system_->GetRoom(0x0001);
  ASSERT_TRUE(room2_result.ok())
      << "Failed to load room 0x0001: " << room2_result.status().message();
}

// Test object editor integration
TEST_F(DungeonEditorSystemIntegrationTest, ObjectEditorIntegration) {
  // Get object editor from system
  auto object_editor = dungeon_editor_system_->GetObjectEditor();
  ASSERT_NE(object_editor, nullptr);

  // Set current room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());

  // Test object insertion
  ASSERT_TRUE(object_editor->InsertObject(5, 5, 0x10, 0x0F, 0).ok());
  ASSERT_TRUE(object_editor->InsertObject(10, 10, 0x20, 0x0F, 1).ok());

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

// Test undo/redo functionality
TEST_F(DungeonEditorSystemIntegrationTest, UndoRedoFunctionality) {
  // Set current room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());

  // Get object editor
  auto object_editor = dungeon_editor_system_->GetObjectEditor();
  ASSERT_NE(object_editor, nullptr);

  // Add some objects
  ASSERT_TRUE(object_editor->InsertObject(5, 5, 0x10, 0x0F, 0).ok());
  ASSERT_TRUE(object_editor->InsertObject(10, 10, 0x20, 0x0F, 1).ok());

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

// Test save/load functionality
TEST_F(DungeonEditorSystemIntegrationTest, SaveLoadFunctionality) {
  // Set current room and add some objects
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0000).ok());

  auto object_editor = dungeon_editor_system_->GetObjectEditor();
  ASSERT_NE(object_editor, nullptr);

  ASSERT_TRUE(object_editor->InsertObject(5, 5, 0x10, 0x0F, 0).ok());
  ASSERT_TRUE(object_editor->InsertObject(10, 10, 0x20, 0x0F, 1).ok());

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

// Test error handling
TEST_F(DungeonEditorSystemIntegrationTest, ErrorHandling) {
  // Test with invalid room ID
  auto invalid_room = dungeon_editor_system_->GetRoom(-1);
  EXPECT_FALSE(invalid_room.ok());

  auto invalid_room_large = dungeon_editor_system_->GetRoom(10000);
  EXPECT_FALSE(invalid_room_large.ok());

  // Test setting invalid room ID
  auto invalid_set = dungeon_editor_system_->SetCurrentRoom(-1);
  EXPECT_FALSE(invalid_set.ok());

  auto invalid_set_large = dungeon_editor_system_->SetCurrentRoom(10000);
  EXPECT_FALSE(invalid_set_large.ok());
}

// Test editor state
TEST_F(DungeonEditorSystemIntegrationTest, EditorState) {
  // Get initial state
  auto state = dungeon_editor_system_->GetEditorState();
  EXPECT_EQ(state.current_room_id, 0);
  EXPECT_FALSE(state.is_dirty);

  // Change room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0x0010).ok());
  state = dungeon_editor_system_->GetEditorState();
  EXPECT_EQ(state.current_room_id, 0x0010);
}

}  // namespace zelda3
}  // namespace yaze
