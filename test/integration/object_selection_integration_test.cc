/**
 * @file object_selection_integration_test.cc
 * @brief Integration tests for ObjectSelection + DungeonObjectInteraction
 *
 * These tests verify the unified selection system works correctly when
 * integrated with the dungeon editor interaction layer.
 */

#include <gtest/gtest.h>

#include "app/editor/dungeon/dungeon_object_interaction.h"
#include "app/editor/dungeon/dungeon_room_store.h"
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

  DungeonRoomStore rooms_;
  gui::Canvas canvas_{"TestCanvas", ImVec2(512, 512)};
  DungeonObjectInteraction interaction_{&canvas_};
};

// =============================================================================
// Selection Callback Tests
// =============================================================================

TEST_F(ObjectSelectionIntegrationTest, SelectionCallbackFires) {
  int callback_count = 0;
  interaction_.SetSelectionChangeCallback(
      [&callback_count]() { callback_count++; });

  // Setting selection should trigger callback
  interaction_.SetSelectedObjects({0});
  EXPECT_GE(callback_count, 1);

  int count_after_first = callback_count;

  // Clearing selection should also trigger callback
  interaction_.ClearSelection();
  EXPECT_GT(callback_count, count_after_first);
}

// =============================================================================
// Selection Mode Tests (via SetSelectedObjects behavior)
// =============================================================================

TEST_F(ObjectSelectionIntegrationTest,
       SetSelectedObjectsReplacesPreviousSelection) {
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
// IsObjectSelectActive Tests
// =============================================================================

TEST_F(ObjectSelectionIntegrationTest, IsObjectSelectActiveWhenHasSelection) {
  EXPECT_FALSE(interaction_.IsObjectSelectActive());

  interaction_.SetSelectedObjects({0});
  EXPECT_TRUE(interaction_.IsObjectSelectActive());

  interaction_.ClearSelection();
  EXPECT_FALSE(interaction_.IsObjectSelectActive());
}

}  // namespace
}  // namespace editor
}  // namespace yaze
