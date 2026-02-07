#include "app/editor/dungeon/object_selection.h"

#include <gtest/gtest.h>

#include "zelda3/dungeon/room_object.h"

namespace yaze::editor {
namespace {

// Helper to create test objects
zelda3::RoomObject CreateTestObject(uint8_t x, uint8_t y, uint8_t size = 0x00,
                                    int16_t id = 0x01) {
  return zelda3::RoomObject(id, x, y, size, 0);
}

// ============================================================================
// Single Selection Tests
// ============================================================================

TEST(ObjectSelectionTest, SelectSingleObject) {
  ObjectSelection selection;

  selection.SelectObject(0, ObjectSelection::SelectionMode::Single);

  EXPECT_TRUE(selection.IsObjectSelected(0));
  EXPECT_EQ(selection.GetSelectionCount(), 1);
  EXPECT_EQ(selection.GetPrimarySelection().value(), 0);
}

TEST(ObjectSelectionTest, SelectSingleObjectReplacesExisting) {
  ObjectSelection selection;

  // Select object 0
  selection.SelectObject(0, ObjectSelection::SelectionMode::Single);
  EXPECT_TRUE(selection.IsObjectSelected(0));

  // Select object 1 (should replace object 0)
  selection.SelectObject(1, ObjectSelection::SelectionMode::Single);
  EXPECT_FALSE(selection.IsObjectSelected(0));
  EXPECT_TRUE(selection.IsObjectSelected(1));
  EXPECT_EQ(selection.GetSelectionCount(), 1);
}

TEST(ObjectSelectionTest, ClearSelection) {
  ObjectSelection selection;

  selection.SelectObject(0, ObjectSelection::SelectionMode::Single);
  EXPECT_TRUE(selection.HasSelection());

  selection.ClearSelection();
  EXPECT_FALSE(selection.HasSelection());
  EXPECT_EQ(selection.GetSelectionCount(), 0);
}

// ============================================================================
// Multi-Selection Tests (Shift+Click)
// ============================================================================

TEST(ObjectSelectionTest, AddToSelectionMode) {
  ObjectSelection selection;

  // Select object 0
  selection.SelectObject(0, ObjectSelection::SelectionMode::Single);
  EXPECT_EQ(selection.GetSelectionCount(), 1);

  // Add object 1 (Shift+click)
  selection.SelectObject(1, ObjectSelection::SelectionMode::Add);
  EXPECT_TRUE(selection.IsObjectSelected(0));
  EXPECT_TRUE(selection.IsObjectSelected(1));
  EXPECT_EQ(selection.GetSelectionCount(), 2);

  // Add object 2
  selection.SelectObject(2, ObjectSelection::SelectionMode::Add);
  EXPECT_EQ(selection.GetSelectionCount(), 3);
}

TEST(ObjectSelectionTest, AddToSelectionDoesNotRemoveExisting) {
  ObjectSelection selection;

  selection.SelectObject(0, ObjectSelection::SelectionMode::Single);
  selection.SelectObject(2, ObjectSelection::SelectionMode::Add);
  selection.SelectObject(4, ObjectSelection::SelectionMode::Add);

  EXPECT_TRUE(selection.IsObjectSelected(0));
  EXPECT_TRUE(selection.IsObjectSelected(2));
  EXPECT_TRUE(selection.IsObjectSelected(4));
  EXPECT_FALSE(selection.IsObjectSelected(1));
  EXPECT_FALSE(selection.IsObjectSelected(3));
}

// ============================================================================
// Toggle Selection Tests (Ctrl+Click)
// ============================================================================

TEST(ObjectSelectionTest, ToggleSelectionMode) {
  ObjectSelection selection;

  // Select object 0
  selection.SelectObject(0, ObjectSelection::SelectionMode::Single);
  EXPECT_TRUE(selection.IsObjectSelected(0));

  // Toggle object 0 (should deselect)
  selection.SelectObject(0, ObjectSelection::SelectionMode::Toggle);
  EXPECT_FALSE(selection.IsObjectSelected(0));
  EXPECT_EQ(selection.GetSelectionCount(), 0);

  // Toggle object 0 again (should select)
  selection.SelectObject(0, ObjectSelection::SelectionMode::Toggle);
  EXPECT_TRUE(selection.IsObjectSelected(0));
  EXPECT_EQ(selection.GetSelectionCount(), 1);
}

TEST(ObjectSelectionTest, ToggleAddsToExistingSelection) {
  ObjectSelection selection;

  selection.SelectObject(0, ObjectSelection::SelectionMode::Single);
  selection.SelectObject(1, ObjectSelection::SelectionMode::Toggle);

  EXPECT_TRUE(selection.IsObjectSelected(0));
  EXPECT_TRUE(selection.IsObjectSelected(1));
  EXPECT_EQ(selection.GetSelectionCount(), 2);
}

TEST(ObjectSelectionTest, ToggleRemovesFromExistingSelection) {
  ObjectSelection selection;

  selection.SelectObject(0, ObjectSelection::SelectionMode::Single);
  selection.SelectObject(1, ObjectSelection::SelectionMode::Add);
  selection.SelectObject(2, ObjectSelection::SelectionMode::Add);

  EXPECT_EQ(selection.GetSelectionCount(), 3);

  // Toggle object 1 (should remove it)
  selection.SelectObject(1, ObjectSelection::SelectionMode::Toggle);

  EXPECT_TRUE(selection.IsObjectSelected(0));
  EXPECT_FALSE(selection.IsObjectSelected(1));
  EXPECT_TRUE(selection.IsObjectSelected(2));
  EXPECT_EQ(selection.GetSelectionCount(), 2);
}

// ============================================================================
// Select All Tests
// ============================================================================

TEST(ObjectSelectionTest, SelectAll) {
  ObjectSelection selection;

  selection.SelectAll(5);

  EXPECT_EQ(selection.GetSelectionCount(), 5);
  EXPECT_TRUE(selection.IsObjectSelected(0));
  EXPECT_TRUE(selection.IsObjectSelected(1));
  EXPECT_TRUE(selection.IsObjectSelected(2));
  EXPECT_TRUE(selection.IsObjectSelected(3));
  EXPECT_TRUE(selection.IsObjectSelected(4));
}

TEST(ObjectSelectionTest, SelectAllReplacesExisting) {
  ObjectSelection selection;

  // Select a few objects
  selection.SelectObject(0, ObjectSelection::SelectionMode::Single);
  selection.SelectObject(1, ObjectSelection::SelectionMode::Add);

  // Select all (should replace)
  selection.SelectAll(10);

  EXPECT_EQ(selection.GetSelectionCount(), 10);
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_TRUE(selection.IsObjectSelected(i));
  }
}

// ============================================================================
// Rectangle Selection Tests
// ============================================================================

TEST(ObjectSelectionTest, RectangleSelectionSingleMode) {
  ObjectSelection selection;

  // Create test objects in a grid
  std::vector<zelda3::RoomObject> objects;
  objects.push_back(CreateTestObject(5, 5));   // 0
  objects.push_back(CreateTestObject(10, 5));  // 1
  objects.push_back(CreateTestObject(15, 5));  // 2
  objects.push_back(CreateTestObject(5, 10));  // 3
  objects.push_back(CreateTestObject(10, 10)); // 4
  objects.push_back(CreateTestObject(15, 10)); // 5

  // Select objects in rectangle (5,5) to (10,10)
  // Should select objects at (5,5), (10,5), (5,10), (10,10)
  selection.SelectObjectsInRect(5, 5, 10, 10, objects,
                                ObjectSelection::SelectionMode::Single);

  EXPECT_TRUE(selection.IsObjectSelected(0));  // (5,5)
  EXPECT_TRUE(selection.IsObjectSelected(1));  // (10,5)
  EXPECT_FALSE(selection.IsObjectSelected(2)); // (15,5) - outside
  EXPECT_TRUE(selection.IsObjectSelected(3));  // (5,10)
  EXPECT_TRUE(selection.IsObjectSelected(4));  // (10,10)
  EXPECT_FALSE(selection.IsObjectSelected(5)); // (15,10) - outside

  EXPECT_EQ(selection.GetSelectionCount(), 4);
}

TEST(ObjectSelectionTest, RectangleSelectionAddMode) {
  ObjectSelection selection;

  std::vector<zelda3::RoomObject> objects;
  objects.push_back(CreateTestObject(5, 5));
  objects.push_back(CreateTestObject(10, 5));
  objects.push_back(CreateTestObject(20, 20));

  // Initial selection
  selection.SelectObject(2, ObjectSelection::SelectionMode::Single);
  EXPECT_EQ(selection.GetSelectionCount(), 1);

  // Add rectangle selection
  selection.SelectObjectsInRect(0, 0, 15, 15, objects,
                                ObjectSelection::SelectionMode::Add);

  // Should have object 2 plus objects in rectangle
  EXPECT_TRUE(selection.IsObjectSelected(0)); // In rectangle
  EXPECT_TRUE(selection.IsObjectSelected(1)); // In rectangle
  EXPECT_TRUE(selection.IsObjectSelected(2)); // Previous selection
  EXPECT_EQ(selection.GetSelectionCount(), 3);
}

TEST(ObjectSelectionTest, RectangleSelectionNormalizesCoordinates) {
  ObjectSelection selection;

  std::vector<zelda3::RoomObject> objects;
  objects.push_back(CreateTestObject(5, 5));
  objects.push_back(CreateTestObject(10, 10));

  // Select with inverted coordinates (bottom-right to top-left)
  selection.SelectObjectsInRect(10, 10, 5, 5, objects,
                                ObjectSelection::SelectionMode::Single);

  EXPECT_TRUE(selection.IsObjectSelected(0));
  EXPECT_TRUE(selection.IsObjectSelected(1));
  EXPECT_EQ(selection.GetSelectionCount(), 2);
}

// ============================================================================
// Rectangle Selection State Tests
// ============================================================================

TEST(ObjectSelectionTest, RectangleSelectionStateLifecycle) {
  ObjectSelection selection;

  EXPECT_FALSE(selection.IsRectangleSelectionActive());

  selection.BeginRectangleSelection(10, 10);
  EXPECT_TRUE(selection.IsRectangleSelectionActive());

  selection.UpdateRectangleSelection(50, 50);
  EXPECT_TRUE(selection.IsRectangleSelectionActive());

  std::vector<zelda3::RoomObject> objects;
  selection.EndRectangleSelection(objects,
                                  ObjectSelection::SelectionMode::Single);
  EXPECT_FALSE(selection.IsRectangleSelectionActive());
}

TEST(ObjectSelectionTest, RectangleSelectionCancel) {
  ObjectSelection selection;

  selection.BeginRectangleSelection(10, 10);
  EXPECT_TRUE(selection.IsRectangleSelectionActive());

  selection.CancelRectangleSelection();
  EXPECT_FALSE(selection.IsRectangleSelectionActive());
}

TEST(ObjectSelectionTest, RectangleSelectionBounds) {
  ObjectSelection selection;

  selection.BeginRectangleSelection(10, 20);
  selection.UpdateRectangleSelection(50, 40);

  auto [min_x, min_y, max_x, max_y] = selection.GetRectangleSelectionBounds();

  EXPECT_EQ(min_x, 10);
  EXPECT_EQ(min_y, 20);
  EXPECT_EQ(max_x, 50);
  EXPECT_EQ(max_y, 40);
}

TEST(ObjectSelectionTest, RectangleSelectionBoundsNormalized) {
  ObjectSelection selection;

  // Start at bottom-right, drag to top-left
  selection.BeginRectangleSelection(50, 40);
  selection.UpdateRectangleSelection(10, 20);

  auto [min_x, min_y, max_x, max_y] = selection.GetRectangleSelectionBounds();

  // Should be normalized
  EXPECT_EQ(min_x, 10);
  EXPECT_EQ(min_y, 20);
  EXPECT_EQ(max_x, 50);
  EXPECT_EQ(max_y, 40);
}

TEST(ObjectSelectionTest, RectangleSelectionLargeEnough) {
  ObjectSelection selection;

  selection.BeginRectangleSelection(10, 10);
  selection.UpdateRectangleSelection(12, 12);
  EXPECT_TRUE(selection.IsRectangleLargeEnough(1));
  EXPECT_FALSE(selection.IsRectangleLargeEnough(6));
}

TEST(ObjectSelectionTest, RectangleSelectionLargeEnoughNormalizesSize) {
  ObjectSelection selection;

  selection.BeginRectangleSelection(40, 40);
  selection.UpdateRectangleSelection(10, 20);
  EXPECT_TRUE(selection.IsRectangleLargeEnough(10));
}

// ============================================================================
// Get Selected Indices Tests
// ============================================================================

TEST(ObjectSelectionTest, GetSelectedIndicesSorted) {
  ObjectSelection selection;

  // Select in random order
  selection.SelectObject(5, ObjectSelection::SelectionMode::Single);
  selection.SelectObject(2, ObjectSelection::SelectionMode::Add);
  selection.SelectObject(8, ObjectSelection::SelectionMode::Add);
  selection.SelectObject(1, ObjectSelection::SelectionMode::Add);

  auto indices = selection.GetSelectedIndices();

  // Should be sorted
  ASSERT_EQ(indices.size(), 4);
  EXPECT_EQ(indices[0], 1);
  EXPECT_EQ(indices[1], 2);
  EXPECT_EQ(indices[2], 5);
  EXPECT_EQ(indices[3], 8);
}

TEST(ObjectSelectionTest, GetPrimarySelection) {
  ObjectSelection selection;

  // No selection
  EXPECT_FALSE(selection.GetPrimarySelection().has_value());

  // Select objects
  selection.SelectObject(5, ObjectSelection::SelectionMode::Single);
  selection.SelectObject(2, ObjectSelection::SelectionMode::Add);

  // Primary should be lowest index
  EXPECT_EQ(selection.GetPrimarySelection().value(), 2);
}

// ============================================================================
// Coordinate Conversion Tests
// ============================================================================

TEST(ObjectSelectionTest, RoomToCanvasCoordinates) {
  auto [canvas_x, canvas_y] = ObjectSelection::RoomToCanvasCoordinates(5, 10);

  EXPECT_EQ(canvas_x, 40);  // 5 tiles * 8 pixels
  EXPECT_EQ(canvas_y, 80);  // 10 tiles * 8 pixels
}

TEST(ObjectSelectionTest, CanvasToRoomCoordinates) {
  auto [room_x, room_y] = ObjectSelection::CanvasToRoomCoordinates(40, 80);

  EXPECT_EQ(room_x, 5);   // 40 pixels / 8
  EXPECT_EQ(room_y, 10);  // 80 pixels / 8
}

TEST(ObjectSelectionTest, CoordinateConversionRoundTrip) {
  int room_x = 15;
  int room_y = 20;

  auto [canvas_x, canvas_y] = ObjectSelection::RoomToCanvasCoordinates(room_x, room_y);
  auto [back_room_x, back_room_y] = ObjectSelection::CanvasToRoomCoordinates(canvas_x, canvas_y);

  EXPECT_EQ(back_room_x, room_x);
  EXPECT_EQ(back_room_y, room_y);
}

// ============================================================================
// Object Bounds Tests
// ============================================================================

TEST(ObjectSelectionTest, GetObjectBoundsSingleTile) {
  // Object 0x01 size 0: DimensionService now returns accurate geometry
  auto obj = CreateTestObject(10, 15, 0x00);

  auto [x, y, width, height] = ObjectSelection::GetObjectBounds(obj);

  EXPECT_EQ(x, 10);
  EXPECT_EQ(y, 15);
  EXPECT_GT(width, 0);
  EXPECT_GT(height, 0);
}

TEST(ObjectSelectionTest, GetObjectBoundsMultipleTiles) {
  // Object 0x01 size 0x23: DimensionService returns accurate dimensions
  auto obj = CreateTestObject(5, 8, 0x23);

  auto [x, y, width, height] = ObjectSelection::GetObjectBounds(obj);

  EXPECT_EQ(x, 5);
  EXPECT_EQ(y, 8);
  EXPECT_GT(width, 0);
  EXPECT_GT(height, 0);
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST(ObjectSelectionTest, SelectionChangedCallback) {
  ObjectSelection selection;

  int callback_count = 0;
  selection.SetSelectionChangedCallback([&callback_count]() {
    callback_count++;
  });

  selection.SelectObject(0, ObjectSelection::SelectionMode::Single);
  EXPECT_EQ(callback_count, 1);

  selection.SelectObject(1, ObjectSelection::SelectionMode::Add);
  EXPECT_EQ(callback_count, 2);

  selection.ClearSelection();
  EXPECT_EQ(callback_count, 3);
}

}  // namespace
}  // namespace yaze::editor
