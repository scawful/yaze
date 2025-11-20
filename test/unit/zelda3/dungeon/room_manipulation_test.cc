// Tests for Room object manipulation methods (Phase 3)

#include <gtest/gtest.h>
#include "app/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace test {

class RoomManipulationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    // Create a minimal ROM for testing
    std::vector<uint8_t> dummy_data(0x200000, 0);
    rom_->LoadFromData(dummy_data, false);

    room_ = std::make_unique<Room>(0, rom_.get());
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<Room> room_;
};

TEST_F(RoomManipulationTest, AddObject) {
  RoomObject obj(0x10, 10, 20, 3, 0);

  auto status = room_->AddObject(obj);
  ASSERT_TRUE(status.ok());

  auto objects = room_->GetTileObjects();
  EXPECT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].id_, 0x10);
  EXPECT_EQ(objects[0].x(), 10);
  EXPECT_EQ(objects[0].y(), 20);
}

TEST_F(RoomManipulationTest, AddInvalidObject) {
  // Invalid X position (> 63)
  RoomObject obj(0x10, 100, 20, 3, 0);

  auto status = room_->AddObject(obj);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(room_->GetTileObjects().size(), 0);
}

TEST_F(RoomManipulationTest, RemoveObject) {
  RoomObject obj1(0x10, 10, 20, 3, 0);
  RoomObject obj2(0x20, 15, 25, 2, 1);

  room_->AddObject(obj1);
  room_->AddObject(obj2);

  EXPECT_EQ(room_->GetTileObjects().size(), 2);

  auto status = room_->RemoveObject(0);
  ASSERT_TRUE(status.ok());

  auto objects = room_->GetTileObjects();
  EXPECT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].id_, 0x20);
}

TEST_F(RoomManipulationTest, RemoveInvalidIndex) {
  auto status = room_->RemoveObject(0);
  EXPECT_FALSE(status.ok());
}

TEST_F(RoomManipulationTest, UpdateObject) {
  RoomObject obj(0x10, 10, 20, 3, 0);
  room_->AddObject(obj);

  RoomObject updated(0x20, 15, 25, 5, 1);
  auto status = room_->UpdateObject(0, updated);
  ASSERT_TRUE(status.ok());

  auto objects = room_->GetTileObjects();
  EXPECT_EQ(objects[0].id_, 0x20);
  EXPECT_EQ(objects[0].x(), 15);
  EXPECT_EQ(objects[0].y(), 25);
}

TEST_F(RoomManipulationTest, FindObjectAt) {
  RoomObject obj1(0x10, 10, 20, 3, 0);
  RoomObject obj2(0x20, 15, 25, 2, 1);

  room_->AddObject(obj1);
  room_->AddObject(obj2);

  auto result = room_->FindObjectAt(15, 25, 1);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), 1);

  auto not_found = room_->FindObjectAt(99, 99, 0);
  EXPECT_FALSE(not_found.ok());
}

TEST_F(RoomManipulationTest, ValidateObject) {
  // Valid Type 1 object
  RoomObject valid1(0x10, 10, 20, 3, 0);
  EXPECT_TRUE(room_->ValidateObject(valid1));

  // Valid Type 2 object
  RoomObject valid2(0x110, 30, 40, 0, 1);
  EXPECT_TRUE(room_->ValidateObject(valid2));

  // Invalid X (> 63)
  RoomObject invalid_x(0x10, 100, 20, 3, 0);
  EXPECT_FALSE(room_->ValidateObject(invalid_x));

  // Invalid layer (> 2)
  RoomObject invalid_layer(0x10, 10, 20, 3, 5);
  EXPECT_FALSE(room_->ValidateObject(invalid_layer));

  // Invalid size for Type 1 (> 15)
  RoomObject invalid_size(0x10, 10, 20, 20, 0);
  EXPECT_FALSE(room_->ValidateObject(invalid_size));
}

TEST_F(RoomManipulationTest, MultipleOperations) {
  // Add several objects
  for (int i = 0; i < 5; i++) {
    RoomObject obj(0x10 + i, i * 5, i * 6, i, 0);
    ASSERT_TRUE(room_->AddObject(obj).ok());
  }

  EXPECT_EQ(room_->GetTileObjects().size(), 5);

  // Update middle object
  RoomObject updated(0x99, 30, 35, 7, 1);
  ASSERT_TRUE(room_->UpdateObject(2, updated).ok());

  // Verify update
  auto objects = room_->GetTileObjects();
  EXPECT_EQ(objects[2].id_, 0x99);

  // Remove first object
  ASSERT_TRUE(room_->RemoveObject(0).ok());
  EXPECT_EQ(room_->GetTileObjects().size(), 4);

  // Verify first object is now what was second
  EXPECT_EQ(room_->GetTileObjects()[0].id_, 0x11);
}

TEST_F(RoomManipulationTest, LayerOrganization) {
  // Add objects to different layers
  RoomObject layer0_obj(0x10, 10, 10, 2, 0);
  RoomObject layer1_obj(0x20, 20, 20, 3, 1);
  RoomObject layer2_obj(0x30, 30, 30, 4, 2);

  room_->AddObject(layer0_obj);
  room_->AddObject(layer1_obj);
  room_->AddObject(layer2_obj);

  // Verify can find by layer
  EXPECT_TRUE(room_->FindObjectAt(10, 10, 0).ok());
  EXPECT_TRUE(room_->FindObjectAt(20, 20, 1).ok());
  EXPECT_TRUE(room_->FindObjectAt(30, 30, 2).ok());

  // Wrong layer should not find
  EXPECT_FALSE(room_->FindObjectAt(10, 10, 1).ok());
}

}  // namespace test
}  // namespace zelda3
}  // namespace yaze
