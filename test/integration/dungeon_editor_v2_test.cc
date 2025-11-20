#include "integration/dungeon_editor_v2_test.h"

namespace yaze {
namespace test {

// ============================================================================
// Basic Initialization Tests
// ============================================================================

TEST_F(DungeonEditorV2IntegrationTest, EditorInitialization) {
  // Initialize should not fail
  dungeon_editor_v2_->Initialize();
  EXPECT_TRUE(dungeon_editor_v2_->rom() != nullptr);
}

TEST_F(DungeonEditorV2IntegrationTest, RomLoadStatus) {
  EXPECT_TRUE(dungeon_editor_v2_->IsRomLoaded());
  std::string status = dungeon_editor_v2_->GetRomStatus();
  EXPECT_FALSE(status.empty());
  EXPECT_NE(status, "No ROM loaded");
}

// ============================================================================
// Load Tests - Component Delegation
// ============================================================================

TEST_F(DungeonEditorV2IntegrationTest, LoadAllRooms) {
  // Test that Load() properly delegates to room_loader_
  dungeon_editor_v2_->Initialize();
  auto status = dungeon_editor_v2_->Load();
  ASSERT_TRUE(status.ok()) << "Load failed: " << status.message();
}

TEST_F(DungeonEditorV2IntegrationTest, LoadWithoutRom) {
  // Test error handling when ROM is not available
  editor::DungeonEditorV2 editor(nullptr);
  auto status = editor.Load();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(DungeonEditorV2IntegrationTest, LoadSequence) {
  // Test the full initialization sequence
  dungeon_editor_v2_->Initialize();

  auto load_status = dungeon_editor_v2_->Load();
  ASSERT_TRUE(load_status.ok());

  // After loading, Update() should work
  (void)dungeon_editor_v2_->Update();
}

// ============================================================================
// Update Tests - UI Coordination
// ============================================================================

TEST_F(DungeonEditorV2IntegrationTest, UpdateBeforeLoad) {
  // Update before Load should show loading message but not crash
  auto status = dungeon_editor_v2_->Update();
  EXPECT_TRUE(status.ok());
}

TEST_F(DungeonEditorV2IntegrationTest, UpdateAfterLoad) {
  dungeon_editor_v2_->Initialize();
  (void)dungeon_editor_v2_->Load();

  // Update should delegate to components
  auto status = dungeon_editor_v2_->Update();
  EXPECT_TRUE(status.ok());
}

// ============================================================================
// Save Tests - Component Delegation
// ============================================================================

TEST_F(DungeonEditorV2IntegrationTest, SaveWithoutRom) {
  // Test error handling when ROM is not available
  editor::DungeonEditorV2 editor(nullptr);
  auto status = editor.Save();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(DungeonEditorV2IntegrationTest, SaveAfterLoad) {
  dungeon_editor_v2_->Initialize();
  auto load_status = dungeon_editor_v2_->Load();
  ASSERT_TRUE(load_status.ok());

  // Save should delegate to room objects
  auto save_status = dungeon_editor_v2_->Save();
  EXPECT_TRUE(save_status.ok());
}

// ============================================================================
// Room Management Tests
// ============================================================================

TEST_F(DungeonEditorV2IntegrationTest, AddRoomTab) {
  dungeon_editor_v2_->Initialize();
  (void)dungeon_editor_v2_->Load();

  // Add a room tab
  dungeon_editor_v2_->add_room(kTestRoomId);

  // This should not crash or fail
  auto status = dungeon_editor_v2_->Update();
  EXPECT_TRUE(status.ok());
}

TEST_F(DungeonEditorV2IntegrationTest, AddMultipleRoomTabs) {
  dungeon_editor_v2_->Initialize();
  (void)dungeon_editor_v2_->Load();

  // Add multiple rooms
  dungeon_editor_v2_->add_room(0x00);
  dungeon_editor_v2_->add_room(0x01);
  dungeon_editor_v2_->add_room(0x02);

  auto status = dungeon_editor_v2_->Update();
  EXPECT_TRUE(status.ok());
}

// ============================================================================
// Component Delegation Tests
// ============================================================================

TEST_F(DungeonEditorV2IntegrationTest, RoomLoaderDelegation) {
  // Verify that Load() delegates to room_loader_
  dungeon_editor_v2_->Initialize();
  auto status = dungeon_editor_v2_->Load();

  // If Load succeeds, room_loader_ must have worked
  EXPECT_TRUE(status.ok());
}

TEST_F(DungeonEditorV2IntegrationTest, ComponentsInitializedAfterLoad) {
  dungeon_editor_v2_->Initialize();
  auto status = dungeon_editor_v2_->Load();
  ASSERT_TRUE(status.ok());

  // After Load(), all components should be properly initialized
  // We can't directly test this, but Update() should work
  (void)dungeon_editor_v2_->Update();
}

// ============================================================================
// ROM Management Tests
// ============================================================================

TEST_F(DungeonEditorV2IntegrationTest, SetRomAfterConstruction) {
  // Create editor without ROM
  editor::DungeonEditorV2 editor;
  EXPECT_EQ(editor.rom(), nullptr);

  // Set ROM
  editor.set_rom(rom_.get());
  EXPECT_EQ(editor.rom(), rom_.get());
  EXPECT_TRUE(editor.IsRomLoaded());
}

TEST_F(DungeonEditorV2IntegrationTest, SetRomAndLoad) {
  // Create editor without ROM
  editor::DungeonEditorV2 editor;

  // Set ROM and load
  editor.set_rom(rom_.get());
  editor.Initialize();
  auto status = editor.Load();

  EXPECT_TRUE(status.ok());
}

// ============================================================================
// Unimplemented Methods Tests
// ============================================================================

TEST_F(DungeonEditorV2IntegrationTest, UnimplementedMethods) {
  // These should return UnimplementedError
  EXPECT_EQ(dungeon_editor_v2_->Undo().code(),
            absl::StatusCode::kUnimplemented);
  EXPECT_EQ(dungeon_editor_v2_->Redo().code(),
            absl::StatusCode::kUnimplemented);
  EXPECT_EQ(dungeon_editor_v2_->Cut().code(), absl::StatusCode::kUnimplemented);
  EXPECT_EQ(dungeon_editor_v2_->Copy().code(),
            absl::StatusCode::kUnimplemented);
  EXPECT_EQ(dungeon_editor_v2_->Paste().code(),
            absl::StatusCode::kUnimplemented);
  EXPECT_EQ(dungeon_editor_v2_->Find().code(),
            absl::StatusCode::kUnimplemented);
}

// ============================================================================
// Stress Testing
// ============================================================================

TEST_F(DungeonEditorV2IntegrationTest, MultipleUpdateCycles) {
  dungeon_editor_v2_->Initialize();
  auto load_status = dungeon_editor_v2_->Load();
  ASSERT_TRUE(load_status.ok());

  // Run multiple update cycles
  for (int i = 0; i < 10; i++) {
    (void)dungeon_editor_v2_->Update();
  }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(DungeonEditorV2IntegrationTest, InvalidRoomId) {
  dungeon_editor_v2_->Initialize();
  (void)dungeon_editor_v2_->Load();

  // Add invalid room ID (beyond 0x128)
  dungeon_editor_v2_->add_room(0x200);

  // Update should handle gracefully
  auto status = dungeon_editor_v2_->Update();
  EXPECT_TRUE(status.ok());
}

TEST_F(DungeonEditorV2IntegrationTest, NegativeRoomId) {
  dungeon_editor_v2_->Initialize();
  (void)dungeon_editor_v2_->Load();

  // Add negative room ID
  dungeon_editor_v2_->add_room(-1);

  // Update should handle gracefully
  auto status = dungeon_editor_v2_->Update();
  EXPECT_TRUE(status.ok());
}

TEST_F(DungeonEditorV2IntegrationTest, LoadTwice) {
  dungeon_editor_v2_->Initialize();

  // Load twice
  auto status1 = dungeon_editor_v2_->Load();
  auto status2 = dungeon_editor_v2_->Load();

  // Both should succeed
  EXPECT_TRUE(status1.ok());
  EXPECT_TRUE(status2.ok());
}

}  // namespace test
}  // namespace yaze
