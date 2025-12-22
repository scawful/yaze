/**
 * @file object_selection_integration_test.cc
 * @brief Integration tests for ObjectSelection + DungeonObjectInteraction
 *
 * These tests verify the unified selection system works correctly when
 * integrated with the dungeon editor interaction layer.
 */

#include <gtest/gtest.h>

#include "app/editor/dungeon/dungeon_object_interaction.h"
#include "app/editor/dungeon/object_selection.h"
#include "app/gui/canvas/canvas.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {
namespace {

class ObjectSelectionIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize rooms with some test objects
    auto& room = rooms_[0];
    room.AddTileObject(zelda3::RoomObject{0x01, 10, 10, 0x12, 0});
    room.AddTileObject(zelda3::RoomObject{0x02, 20, 10, 0x14, 0});
    room.AddTileObject(zelda3::RoomObject{0x03, 10, 20, 0x16, 1});
    room.AddTileObject(zelda3::RoomObject{0x04, 30, 30, 0x18, 2});

    // Set up interaction with the room
    interaction_.SetCurrentRoom(&rooms_, 0);
  }

  std::array<zelda3::Room, 0x128> rooms_;
  gui::Canvas canvas_{"TestCanvas", ImVec2(512, 512)};
  DungeonObjectInteraction interaction_{&canvas_};
};

// =============================================================================
// Basic Selection Tests
// =============================================================================

TEST_F(ObjectSelectionIntegrationTest, InitialStateHasNoSelection) {
  EXPECT_TRUE(interaction_.GetSelectedObjectIndices().empty());
  EXPECT_EQ(interaction_.GetSelectionCount(), 0);
  EXPECT_FALSE(interaction_.IsObjectSelectActive());
}

TEST_F(ObjectSelectionIntegrationTest, SetSelectedObjectsUpdatesSelection) {
  std::vector<size_t> indices = {0, 2};
  interaction_.SetSelectedObjects(indices);

  auto selected = interaction_.GetSelectedObjectIndices();
  EXPECT_EQ(selected.size(), 2);
  EXPECT_TRUE(interaction_.IsObjectSelected(0));
  EXPECT_FALSE(interaction_.IsObjectSelected(1));
  EXPECT_TRUE(interaction_.IsObjectSelected(2));
  EXPECT_FALSE(interaction_.IsObjectSelected(3));
}

TEST_F(ObjectSelectionIntegrationTest, ClearSelectionRemovesAllSelections) {
  interaction_.SetSelectedObjects({0, 1, 2});
  EXPECT_EQ(interaction_.GetSelectionCount(), 3);

  interaction_.ClearSelection();
  EXPECT_EQ(interaction_.GetSelectionCount(), 0);
  EXPECT_TRUE(interaction_.GetSelectedObjectIndices().empty());
}

TEST_F(ObjectSelectionIntegrationTest, IsObjectSelectedReturnsCorrectValue) {
  interaction_.SetSelectedObjects({1, 3});

  EXPECT_FALSE(interaction_.IsObjectSelected(0));
  EXPECT_TRUE(interaction_.IsObjectSelected(1));
  EXPECT_FALSE(interaction_.IsObjectSelected(2));
  EXPECT_TRUE(interaction_.IsObjectSelected(3));
}

// =============================================================================
// Selection Callback Tests
// =============================================================================

TEST_F(ObjectSelectionIntegrationTest, SelectionCallbackFires) {
  int callback_count = 0;
  interaction_.SetSelectionChangeCallback([&callback_count]() {
    callback_count++;
  });

  // Setting selection should trigger callback
  interaction_.SetSelectedObjects({0});
  EXPECT_GE(callback_count, 1);

  int count_after_first = callback_count;

  // Clearing selection should also trigger callback
  interaction_.ClearSelection();
  EXPECT_GT(callback_count, count_after_first);
}

TEST_F(ObjectSelectionIntegrationTest, MultipleSelectionChangesFireMultipleCallbacks) {
  std::vector<std::vector<size_t>> callback_selections;

  interaction_.SetSelectionChangeCallback([this, &callback_selections]() {
    callback_selections.push_back(interaction_.GetSelectedObjectIndices());
  });

  interaction_.SetSelectedObjects({0});
  interaction_.SetSelectedObjects({0, 1});
  interaction_.SetSelectedObjects({2});
  interaction_.ClearSelection();

  // Should have received multiple callbacks
  EXPECT_GE(callback_selections.size(), 2);
}

// =============================================================================
// Selection Count Tests
// =============================================================================

TEST_F(ObjectSelectionIntegrationTest, GetSelectionCountReturnsCorrectCount) {
  EXPECT_EQ(interaction_.GetSelectionCount(), 0);

  interaction_.SetSelectedObjects({0});
  EXPECT_EQ(interaction_.GetSelectionCount(), 1);

  interaction_.SetSelectedObjects({0, 1, 2});
  EXPECT_EQ(interaction_.GetSelectionCount(), 3);

  interaction_.SetSelectedObjects({0, 1, 2, 3});
  EXPECT_EQ(interaction_.GetSelectionCount(), 4);
}

// =============================================================================
// Selection Mode Tests (via SetSelectedObjects behavior)
// =============================================================================

TEST_F(ObjectSelectionIntegrationTest, SetSelectedObjectsReplacesPreviousSelection) {
  interaction_.SetSelectedObjects({0, 1});
  EXPECT_EQ(interaction_.GetSelectionCount(), 2);
  EXPECT_TRUE(interaction_.IsObjectSelected(0));
  EXPECT_TRUE(interaction_.IsObjectSelected(1));

  // Setting new selection should replace, not add
  interaction_.SetSelectedObjects({2, 3});
  EXPECT_EQ(interaction_.GetSelectionCount(), 2);
  EXPECT_FALSE(interaction_.IsObjectSelected(0));
  EXPECT_FALSE(interaction_.IsObjectSelected(1));
  EXPECT_TRUE(interaction_.IsObjectSelected(2));
  EXPECT_TRUE(interaction_.IsObjectSelected(3));
}

TEST_F(ObjectSelectionIntegrationTest, DuplicateIndicesAreHandled) {
  // Setting the same index twice should only count once (using set internally)
  interaction_.SetSelectedObjects({0, 0, 0, 1, 1});

  // Should have 2 unique selections, not 5
  EXPECT_EQ(interaction_.GetSelectionCount(), 2);
}

// =============================================================================
// Integration with Room Data
// =============================================================================

TEST_F(ObjectSelectionIntegrationTest, SelectionPersistsAcrossRoomAccess) {
  interaction_.SetSelectedObjects({0, 2});

  // Access room data (simulating what ObjectEditorPanel would do)
  auto& room = rooms_[0];
  const auto& objects = room.GetTileObjects();
  EXPECT_EQ(objects.size(), 4);

  // Selection should still be valid
  EXPECT_EQ(interaction_.GetSelectionCount(), 2);
  EXPECT_TRUE(interaction_.IsObjectSelected(0));
  EXPECT_TRUE(interaction_.IsObjectSelected(2));
}

TEST_F(ObjectSelectionIntegrationTest, OutOfBoundsIndicesAreAccepted) {
  // The selection system accepts indices without validating against room size
  // This is intentional - the room might not be loaded yet
  interaction_.SetSelectedObjects({100, 200});
  EXPECT_EQ(interaction_.GetSelectionCount(), 2);
  EXPECT_TRUE(interaction_.IsObjectSelected(100));
}

// =============================================================================
// IsObjectSelectActive Tests
// =============================================================================

TEST_F(ObjectSelectionIntegrationTest, IsObjectSelectActiveWhenHasSelection) {
  EXPECT_FALSE(interaction_.IsObjectSelectActive());

  interaction_.SetSelectedObjects({0});
  EXPECT_TRUE(interaction_.IsObjectSelectActive());

  interaction_.ClearSelection();
  EXPECT_FALSE(interaction_.IsObjectSelectActive());
}

// =============================================================================
// Empty Selection Tests
// =============================================================================

TEST_F(ObjectSelectionIntegrationTest, EmptyVectorClearsSelection) {
  interaction_.SetSelectedObjects({0, 1, 2});
  EXPECT_EQ(interaction_.GetSelectionCount(), 3);

  interaction_.SetSelectedObjects({});
  EXPECT_EQ(interaction_.GetSelectionCount(), 0);
}

TEST_F(ObjectSelectionIntegrationTest, ClearSelectionIsIdempotent) {
  interaction_.ClearSelection();
  EXPECT_EQ(interaction_.GetSelectionCount(), 0);

  interaction_.ClearSelection();
  EXPECT_EQ(interaction_.GetSelectionCount(), 0);

  interaction_.SetSelectedObjects({0});
  interaction_.ClearSelection();
  interaction_.ClearSelection();
  EXPECT_EQ(interaction_.GetSelectionCount(), 0);
}

}  // namespace
}  // namespace editor
}  // namespace yaze
