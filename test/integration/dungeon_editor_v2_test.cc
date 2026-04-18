#include "integration/dungeon_editor_v2_test.h"

#include "core/features.h"

namespace yaze {
namespace test {

namespace {

struct DungeonFeatureFlagsGuard {
  decltype(core::FeatureFlags::get().dungeon) prev =
      core::FeatureFlags::get().dungeon;
  ~DungeonFeatureFlagsGuard() { core::FeatureFlags::get().dungeon = prev; }
};

}  // namespace

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

TEST_F(DungeonEditorV2IntegrationTest,
       QueueWorkbenchWorkflowModeDefersDisableUntilUpdate) {
  DungeonFeatureFlagsGuard guard;
  core::FeatureFlags::get().dungeon.kUseWorkbench = true;

  dungeon_editor_v2_->Initialize();
  const size_t session_id = window_manager_->GetActiveSessionId();

  ASSERT_TRUE(window_manager_->IsWindowOpen(session_id, "dungeon.workbench"));
  EXPECT_FALSE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomSelectorId));
  EXPECT_FALSE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomMatrixId));

  dungeon_editor_v2_->QueueWorkbenchWorkflowMode(false, /*show_toast=*/false);

  // Queued mode changes are deferred to Update().
  EXPECT_TRUE(window_manager_->IsWindowOpen(session_id, "dungeon.workbench"));
  EXPECT_FALSE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomSelectorId));

  auto status = dungeon_editor_v2_->Update();
  ASSERT_TRUE(status.ok());

  EXPECT_FALSE(window_manager_->IsWindowOpen(session_id, "dungeon.workbench"));
  EXPECT_TRUE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomSelectorId));
  EXPECT_TRUE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomMatrixId));
}

TEST_F(DungeonEditorV2IntegrationTest,
       QueueWorkbenchWorkflowModeDefersEnableUntilUpdate) {
  DungeonFeatureFlagsGuard guard;
  core::FeatureFlags::get().dungeon.kUseWorkbench = true;

  dungeon_editor_v2_->Initialize();
  const size_t session_id = window_manager_->GetActiveSessionId();

  dungeon_editor_v2_->SetWorkbenchWorkflowMode(false, /*show_toast=*/false);
  ASSERT_FALSE(window_manager_->IsWindowOpen(session_id, "dungeon.workbench"));
  ASSERT_TRUE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomSelectorId));
  ASSERT_TRUE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomMatrixId));

  dungeon_editor_v2_->QueueWorkbenchWorkflowMode(true, /*show_toast=*/false);

  // Mode does not flip until the next update tick.
  EXPECT_FALSE(window_manager_->IsWindowOpen(session_id, "dungeon.workbench"));
  EXPECT_TRUE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomSelectorId));

  auto status = dungeon_editor_v2_->Update();
  ASSERT_TRUE(status.ok());

  EXPECT_TRUE(window_manager_->IsWindowOpen(session_id, "dungeon.workbench"));
  EXPECT_FALSE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomSelectorId));
  EXPECT_FALSE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomMatrixId));
}

TEST_F(DungeonEditorV2IntegrationTest,
       ToggleWorkbenchWorkflowModeDefersFlipUntilUpdate) {
  DungeonFeatureFlagsGuard guard;
  core::FeatureFlags::get().dungeon.kUseWorkbench = true;

  dungeon_editor_v2_->Initialize();
  const size_t session_id = window_manager_->GetActiveSessionId();
  ASSERT_TRUE(window_manager_->IsWindowOpen(session_id, "dungeon.workbench"));

  dungeon_editor_v2_->ToggleWorkbenchWorkflowMode(/*show_toast=*/false);
  EXPECT_TRUE(window_manager_->IsWindowOpen(session_id, "dungeon.workbench"));

  auto status = dungeon_editor_v2_->Update();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(window_manager_->IsWindowOpen(session_id, "dungeon.workbench"));
  EXPECT_TRUE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomSelectorId));

  dungeon_editor_v2_->ToggleWorkbenchWorkflowMode(/*show_toast=*/false);
  EXPECT_FALSE(window_manager_->IsWindowOpen(session_id, "dungeon.workbench"));

  status = dungeon_editor_v2_->Update();
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(window_manager_->IsWindowOpen(session_id, "dungeon.workbench"));
  EXPECT_FALSE(window_manager_->IsWindowOpen(
      session_id, editor::DungeonEditorV2::kRoomSelectorId));
}

// ============================================================================
// Save Tests - Component Delegation
// ============================================================================

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

// ============================================================================
// Unimplemented Methods Tests
// ============================================================================

TEST_F(DungeonEditorV2IntegrationTest, EditingCommandsFallback) {
  // Undo/Redo should report precondition when history is empty
  EXPECT_EQ(dungeon_editor_v2_->Undo().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(dungeon_editor_v2_->Redo().code(),
            absl::StatusCode::kFailedPrecondition);

  // Cut/Copy/Paste should be callable even without a selection
  EXPECT_EQ(dungeon_editor_v2_->Cut().code(), absl::StatusCode::kOk);
  EXPECT_EQ(dungeon_editor_v2_->Copy().code(), absl::StatusCode::kOk);
  EXPECT_EQ(dungeon_editor_v2_->Paste().code(), absl::StatusCode::kOk);

  // Find remains unimplemented
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
