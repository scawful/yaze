#include "app/gfx/util/palette_manager.h"

#include <gtest/gtest.h>

#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_palette.h"
#include "rom/rom.h"

namespace yaze {
namespace gfx {
namespace {

// Test fixture for PaletteManager integration tests
class PaletteManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // PaletteManager is a singleton, so reset it for test isolation
    PaletteManager::Get().ResetForTesting();
  }

  void TearDown() override {
    // Clean up any test state
    PaletteManager::Get().ClearHistory();
  }
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(PaletteManagerTest, InitializationState) {
  auto& manager = PaletteManager::Get();

  // Before initialization, should not be initialized
  // Note: This might fail if other tests have already initialized it
  // In production, we'd need a Reset() method for testing

  // After initialization with null ROM, should handle gracefully
  manager.Initialize(static_cast<Rom*>(nullptr));
  EXPECT_FALSE(manager.IsInitialized());
}

TEST_F(PaletteManagerTest, HasNoUnsavedChangesInitially) {
  auto& manager = PaletteManager::Get();

  // Should have no unsaved changes initially
  EXPECT_FALSE(manager.HasUnsavedChanges());
  EXPECT_EQ(manager.GetModifiedColorCount(), 0);
}

// ============================================================================
// Dirty Tracking Tests
// ============================================================================

TEST_F(PaletteManagerTest, TracksModifiedGroups) {
  auto& manager = PaletteManager::Get();

  // Initially, no groups should be modified
  auto modified_groups = manager.GetModifiedGroups();
  EXPECT_TRUE(modified_groups.empty());
}

TEST_F(PaletteManagerTest, GetModifiedColorCount) {
  auto& manager = PaletteManager::Get();

  // Initially, no colors modified
  EXPECT_EQ(manager.GetModifiedColorCount(), 0);

  // After initialization and making changes, count should increase
  // (This would require a valid ROM to test properly)
}

// ============================================================================
// Undo/Redo Tests
// ============================================================================

TEST_F(PaletteManagerTest, UndoRedoInitialState) {
  auto& manager = PaletteManager::Get();

  // Initially, should not be able to undo or redo
  EXPECT_FALSE(manager.CanUndo());
  EXPECT_FALSE(manager.CanRedo());
  EXPECT_EQ(manager.GetUndoStackSize(), 0);
  EXPECT_EQ(manager.GetRedoStackSize(), 0);
}

TEST_F(PaletteManagerTest, ClearHistoryResetsStacks) {
  auto& manager = PaletteManager::Get();

  // Clear history should reset both stacks
  manager.ClearHistory();

  EXPECT_FALSE(manager.CanUndo());
  EXPECT_FALSE(manager.CanRedo());
  EXPECT_EQ(manager.GetUndoStackSize(), 0);
  EXPECT_EQ(manager.GetRedoStackSize(), 0);
}

TEST_F(PaletteManagerTest, UndoWithoutChangesIsNoOp) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.CanUndo());

  // Should not crash
  manager.Undo();

  EXPECT_FALSE(manager.CanUndo());
}

TEST_F(PaletteManagerTest, RedoWithoutUndoIsNoOp) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.CanRedo());

  // Should not crash
  manager.Redo();

  EXPECT_FALSE(manager.CanRedo());
}

// ============================================================================
// Batch Operations Tests
// ============================================================================

TEST_F(PaletteManagerTest, BatchModeTracking) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.InBatch());

  manager.BeginBatch();
  EXPECT_TRUE(manager.InBatch());

  manager.EndBatch();
  EXPECT_FALSE(manager.InBatch());
}

TEST_F(PaletteManagerTest, NestedBatchOperations) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.InBatch());

  manager.BeginBatch();
  EXPECT_TRUE(manager.InBatch());

  manager.BeginBatch();  // Nested
  EXPECT_TRUE(manager.InBatch());

  manager.EndBatch();
  EXPECT_TRUE(manager.InBatch());  // Still in batch (outer)

  manager.EndBatch();
  EXPECT_FALSE(manager.InBatch());  // Now out of batch
}

TEST_F(PaletteManagerTest, EndBatchWithoutBeginIsNoOp) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.InBatch());

  // Should not crash
  manager.EndBatch();

  EXPECT_FALSE(manager.InBatch());
}

// ============================================================================
// Change Notification Tests
// ============================================================================

TEST_F(PaletteManagerTest, RegisterAndUnregisterListener) {
  auto& manager = PaletteManager::Get();

  int callback_count = 0;
  auto callback = [&callback_count](const PaletteChangeEvent& event) {
    callback_count++;
  };

  // Register listener
  int id = manager.RegisterChangeListener(callback);
  EXPECT_GT(id, 0);

  // Unregister listener
  manager.UnregisterChangeListener(id);

  // After unregistering, callback should not be called
  // (Would need to trigger an event to test this properly)
}

TEST_F(PaletteManagerTest, MultipleListeners) {
  auto& manager = PaletteManager::Get();

  int callback1_count = 0;
  int callback2_count = 0;

  auto callback1 = [&callback1_count](const PaletteChangeEvent& event) {
    callback1_count++;
  };

  auto callback2 = [&callback2_count](const PaletteChangeEvent& event) {
    callback2_count++;
  };

  int id1 = manager.RegisterChangeListener(callback1);
  int id2 = manager.RegisterChangeListener(callback2);

  EXPECT_NE(id1, id2);

  // Clean up
  manager.UnregisterChangeListener(id1);
  manager.UnregisterChangeListener(id2);
}

// ============================================================================
// Color Query Tests (without ROM)
// ============================================================================

TEST_F(PaletteManagerTest, DISABLED_GetColorWithoutInitialization) {
  auto& manager = PaletteManager::Get();
  // Reset for this test
  manager.Initialize(static_cast<Rom*>(nullptr));
  
  // Should not crash, but return a default color or error
  // Note: Implementation detail - might return black or throw assertion in debug
  // This test ensures safe failure
  
  // Assuming GetColor handles uninitialized state by returning default or safe value
  // If it asserts, we can't easily test it here without death test
}

TEST_F(PaletteManagerTest, DISABLED_SetColorWithoutInitializationFails) {
  auto& manager = PaletteManager::Get();
  manager.Initialize(static_cast<Rom*>(nullptr));
  
  // Should return false/error instead of crashing
  // Assuming SetColor handles uninitialized state
  // EXPECT_FALSE(manager.SetColor(0, 0, gfx::SnesColor()));
}

TEST_F(PaletteManagerTest, ResetColorWithoutInitializationReturnsError) {
  auto& manager = PaletteManager::Get();

  auto status = manager.ResetColor("ow_main", 0, 0);

  // Should return an error or default color
  // Exact behavior depends on implementation
}

TEST_F(PaletteManagerTest, ResetPaletteWithoutInitializationFails) {
  auto& manager = PaletteManager::Get();

  auto status = manager.ResetPalette("ow_main", 0);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

// ============================================================================
// Save/Discard Tests (without ROM)
// ============================================================================

TEST_F(PaletteManagerTest, SaveGroupWithoutInitializationFails) {
  auto& manager = PaletteManager::Get();

  auto status = manager.SaveGroup("ow_main");

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(PaletteManagerTest, SaveAllWithoutInitializationFails) {
  auto& manager = PaletteManager::Get();

  auto status = manager.SaveAllToRom();

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(PaletteManagerTest, DiscardGroupWithoutInitializationIsNoOp) {
  auto& manager = PaletteManager::Get();

  // Should not crash
  manager.DiscardGroup("ow_main");

  // No unsaved changes
  EXPECT_FALSE(manager.HasUnsavedChanges());
}

TEST_F(PaletteManagerTest, DiscardAllWithoutInitializationIsNoOp) {
  auto& manager = PaletteManager::Get();

  // Should not crash
  manager.DiscardAllChanges();

  // No unsaved changes
  EXPECT_FALSE(manager.HasUnsavedChanges());
}

// ============================================================================
// Group Modification Query Tests
// ============================================================================

TEST_F(PaletteManagerTest, IsGroupModifiedInitiallyFalse) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.IsGroupModified("ow_main"));
  EXPECT_FALSE(manager.IsGroupModified("dungeon_main"));
  EXPECT_FALSE(manager.IsGroupModified("global_sprites"));
}

TEST_F(PaletteManagerTest, IsPaletteModifiedInitiallyFalse) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.IsPaletteModified("ow_main", 0));
  EXPECT_FALSE(manager.IsPaletteModified("ow_main", 5));
}

TEST_F(PaletteManagerTest, IsColorModifiedInitiallyFalse) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.IsColorModified("ow_main", 0, 0));
  EXPECT_FALSE(manager.IsColorModified("ow_main", 0, 7));
}

// ============================================================================
// Invalid Input Tests
// ============================================================================

TEST_F(PaletteManagerTest, SetColorInvalidGroupName) {
  auto& manager = PaletteManager::Get();

  SnesColor color(0x7FFF);
  auto status = manager.SetColor("invalid_group", 0, 0, color);

  EXPECT_FALSE(status.ok());
}

TEST_F(PaletteManagerTest, GetColorInvalidGroupName) {
  auto& manager = PaletteManager::Get();

  SnesColor color = manager.GetColor("invalid_group", 0, 0);

  // Should return default color
  auto rgb = color.rgb();
  EXPECT_FLOAT_EQ(rgb.x, 0.0f);
  EXPECT_FLOAT_EQ(rgb.y, 0.0f);
  EXPECT_FLOAT_EQ(rgb.z, 0.0f);
}

TEST_F(PaletteManagerTest, IsGroupModifiedInvalidGroupName) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.IsGroupModified("invalid_group"));
}

}  // namespace
}  // namespace gfx
}  // namespace yaze
