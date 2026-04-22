#include "app/editor/dungeon/interaction/door_interaction_handler.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "app/editor/dungeon/interaction/sprite_interaction_handler.h"
#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"

#include <gtest/gtest.h>
#include <array>

#include "app/editor/dungeon/dungeon_room_store.h"
namespace yaze::editor {
namespace {

// ============================================================================
// Sprite Interaction Handler Tests
// ============================================================================

class SpriteInteractionHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ImGui::CreateContext();
    ImGui::GetIO().DisplaySize = ImVec2(1024, 768);

    canvas_ = std::make_unique<gui::Canvas>("TestCanvas", ImVec2(512, 512),
                                            gui::CanvasGridSize::k16x16);

    ctx_.rooms = &rooms_;
    ctx_.current_room_id = 0;
    ctx_.canvas = canvas_.get();
    ctx_.on_mutation = [this]() {
      mutation_count_++;
    };
    ctx_.on_invalidate_cache = [this]() {
      invalidate_count_++;
    };

    handler_.SetContext(&ctx_);
  }

  void TearDown() override {
    canvas_.reset();
    ImGui::DestroyContext();
  }

  void AddSprites(int count) {
    auto& sprites = rooms_[0].GetSprites();
    for (int i = 0; i < count; ++i) {
      sprites.emplace_back(static_cast<uint8_t>(i % 256),
                           static_cast<uint8_t>(i % 32),
                           static_cast<uint8_t>(i % 32), 0, 0);
    }
  }

  std::unique_ptr<gui::Canvas> canvas_;
  DungeonRoomStore rooms_;
  InteractionContext ctx_;
  SpriteInteractionHandler handler_;
  int mutation_count_ = 0;
  int invalidate_count_ = 0;
};

TEST_F(SpriteInteractionHandlerTest, PlacementSucceedsUnderLimit) {
  handler_.SetSpriteId(0x42);
  handler_.BeginPlacement();
  EXPECT_TRUE(handler_.IsPlacementActive());

  handler_.HandleClick(80, 80);

  EXPECT_FALSE(handler_.was_placement_blocked());
  EXPECT_EQ(rooms_[0].GetSprites().size(), 1);
}

TEST_F(SpriteInteractionHandlerTest, PlacementBlocksAtSpriteLimit) {
  constexpr int kMaxSprites = 64;
  AddSprites(kMaxSprites);
  ASSERT_EQ(rooms_[0].GetSprites().size(), static_cast<size_t>(kMaxSprites));

  handler_.SetSpriteId(0xFF);
  handler_.BeginPlacement();
  handler_.HandleClick(80, 80);

  EXPECT_TRUE(handler_.was_placement_blocked());
  EXPECT_EQ(handler_.placement_block_reason(),
            SpriteInteractionHandler::PlacementBlockReason::kSpriteLimit);
  EXPECT_EQ(rooms_[0].GetSprites().size(), static_cast<size_t>(kMaxSprites));
}

TEST_F(SpriteInteractionHandlerTest, ClearPlacementBlocked) {
  AddSprites(64);
  handler_.SetSpriteId(0xFF);
  handler_.BeginPlacement();
  handler_.HandleClick(80, 80);

  EXPECT_TRUE(handler_.was_placement_blocked());
  handler_.clear_placement_blocked();
  EXPECT_FALSE(handler_.was_placement_blocked());
  EXPECT_EQ(handler_.placement_block_reason(),
            SpriteInteractionHandler::PlacementBlockReason::kNone);
}

TEST_F(SpriteInteractionHandlerTest, PlacementUnderLimitDoesNotBlock) {
  AddSprites(63);  // One below limit
  handler_.SetSpriteId(0x01);
  handler_.BeginPlacement();
  handler_.HandleClick(80, 80);

  EXPECT_FALSE(handler_.was_placement_blocked());
  EXPECT_EQ(rooms_[0].GetSprites().size(), 64u);
}

TEST_F(SpriteInteractionHandlerTest, PlacementModeLifecycle) {
  EXPECT_FALSE(handler_.IsPlacementActive());
  handler_.BeginPlacement();
  EXPECT_TRUE(handler_.IsPlacementActive());
  handler_.CancelPlacement();
  EXPECT_FALSE(handler_.IsPlacementActive());
}

// ============================================================================
// Door Interaction Handler Tests
// ============================================================================

class DoorInteractionHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ImGui::CreateContext();
    ImGui::GetIO().DisplaySize = ImVec2(1024, 768);

    canvas_ = std::make_unique<gui::Canvas>("TestCanvas", ImVec2(512, 512),
                                            gui::CanvasGridSize::k16x16);

    ctx_.rooms = &rooms_;
    ctx_.current_room_id = 0;
    ctx_.canvas = canvas_.get();
    ctx_.on_mutation = [this]() {
      mutation_count_++;
    };
    ctx_.on_invalidate_cache = [this]() {
      invalidate_count_++;
    };

    handler_.SetContext(&ctx_);
  }

  void TearDown() override {
    canvas_.reset();
    ImGui::DestroyContext();
  }

  void AddDoors(int count) {
    for (int i = 0; i < count; ++i) {
      zelda3::Room::Door door;
      door.position = static_cast<uint8_t>(i % 32);
      door.type = zelda3::DoorType::NormalDoor;
      door.direction = zelda3::DoorDirection::North;
      rooms_[0].AddDoor(door);
    }
  }

  std::unique_ptr<gui::Canvas> canvas_;
  DungeonRoomStore rooms_;
  InteractionContext ctx_;
  DoorInteractionHandler handler_;
  int mutation_count_ = 0;
  int invalidate_count_ = 0;
};

TEST_F(DoorInteractionHandlerTest, PlacementBlocksAtDoorLimit) {
  constexpr int kMaxDoors = 16;
  AddDoors(kMaxDoors);
  ASSERT_EQ(rooms_[0].GetDoors().size(), static_cast<size_t>(kMaxDoors));

  handler_.SetDoorType(zelda3::DoorType::NormalDoor);
  handler_.BeginPlacement();

  // HandleClick calls PlaceDoorAtSnappedPosition internally.
  // Even if the position is invalid, the door limit check should fire first.
  handler_.HandleClick(80, 80);

  EXPECT_TRUE(handler_.was_placement_blocked());
  EXPECT_EQ(handler_.placement_block_reason(),
            DoorInteractionHandler::PlacementBlockReason::kDoorLimit);
  EXPECT_EQ(rooms_[0].GetDoors().size(), static_cast<size_t>(kMaxDoors));
}

TEST_F(DoorInteractionHandlerTest, ClearPlacementBlocked) {
  AddDoors(16);
  handler_.SetDoorType(zelda3::DoorType::NormalDoor);
  handler_.BeginPlacement();
  handler_.HandleClick(80, 80);

  EXPECT_TRUE(handler_.was_placement_blocked());
  handler_.clear_placement_blocked();
  EXPECT_FALSE(handler_.was_placement_blocked());
  EXPECT_EQ(handler_.placement_block_reason(),
            DoorInteractionHandler::PlacementBlockReason::kNone);
}

TEST_F(DoorInteractionHandlerTest, PlacementModeLifecycle) {
  EXPECT_FALSE(handler_.IsPlacementActive());
  handler_.BeginPlacement();
  EXPECT_TRUE(handler_.IsPlacementActive());
  handler_.CancelPlacement();
  EXPECT_FALSE(handler_.IsPlacementActive());
}

TEST_F(DoorInteractionHandlerTest, GhostCapacityStateIsNormalBelowLastSlot) {
  AddDoors(14);

  EXPECT_EQ(handler_.GetPlacementGhostCapacityState(),
            DoorInteractionHandler::GhostCapacityState::kNormal);
}

TEST_F(DoorInteractionHandlerTest, GhostCapacityStateWarnsOnLastAvailableSlot) {
  AddDoors(15);

  EXPECT_EQ(handler_.GetPlacementGhostCapacityState(),
            DoorInteractionHandler::GhostCapacityState::kNearLimit);
}

TEST_F(DoorInteractionHandlerTest, GhostCapacityStateBlocksWhenRoomIsFull) {
  AddDoors(16);

  EXPECT_EQ(handler_.GetPlacementGhostCapacityState(),
            DoorInteractionHandler::GhostCapacityState::kAtLimit);
}

TEST_F(DoorInteractionHandlerTest, PlacementBlocksAtInvalidPosition) {
  // Clicking in the middle of the canvas (far from any wall) under the door
  // limit should set kInvalidPosition — not kDoorLimit.
  ASSERT_LT(rooms_[0].GetDoors().size(), 16u);

  handler_.SetDoorType(zelda3::DoorType::NormalDoor);
  handler_.BeginPlacement();
  // (200, 200) is roughly the center of a 512x512 canvas — not a wall
  handler_.HandleClick(200, 200);

  EXPECT_TRUE(handler_.was_placement_blocked());
  EXPECT_EQ(handler_.placement_block_reason(),
            DoorInteractionHandler::PlacementBlockReason::kInvalidPosition);
  EXPECT_EQ(rooms_[0].GetDoors().size(), 0u);
}

TEST_F(DoorInteractionHandlerTest,
       MutateDoorTypeReencodesBytesAndFiresMutationCallbacks) {
  zelda3::Room::Door door;
  door.position = 0x08;
  door.type = zelda3::DoorType::NormalDoor;
  door.direction = zelda3::DoorDirection::North;
  rooms_[0].AddDoor(door);

  const int mutations_before = mutation_count_;
  const int invalidations_before = invalidate_count_;

  const bool changed =
      handler_.MutateDoorType(0, zelda3::DoorType::CurtainDoor);
  EXPECT_TRUE(changed);

  const auto& updated = rooms_[0].GetDoors()[0];
  EXPECT_EQ(updated.type, zelda3::DoorType::CurtainDoor);
  EXPECT_EQ(updated.position, 0x08);
  EXPECT_EQ(updated.direction, zelda3::DoorDirection::North);

  zelda3::Room::Door expected;
  expected.position = 0x08;
  expected.type = zelda3::DoorType::CurtainDoor;
  expected.direction = zelda3::DoorDirection::North;
  const auto [expected_b1, expected_b2] = expected.EncodeBytes();
  EXPECT_EQ(updated.byte1, expected_b1);
  EXPECT_EQ(updated.byte2, expected_b2);

  EXPECT_EQ(mutation_count_, mutations_before + 1);
  EXPECT_EQ(invalidate_count_, invalidations_before + 1);
}

TEST_F(DoorInteractionHandlerTest, MutateDoorTypeNoOpWhenTypeUnchanged) {
  zelda3::Room::Door door;
  door.position = 0x04;
  door.type = zelda3::DoorType::SmallKeyDoor;
  door.direction = zelda3::DoorDirection::East;
  rooms_[0].AddDoor(door);

  const int mutations_before = mutation_count_;

  EXPECT_FALSE(handler_.MutateDoorType(0, zelda3::DoorType::SmallKeyDoor));
  EXPECT_EQ(mutation_count_, mutations_before);
}

TEST_F(DoorInteractionHandlerTest, MutateDoorTypeRejectsOutOfRangeIndex) {
  AddDoors(1);
  EXPECT_FALSE(handler_.MutateDoorType(5, zelda3::DoorType::CurtainDoor));
  EXPECT_EQ(rooms_[0].GetDoors()[0].type, zelda3::DoorType::NormalDoor);
}

TEST_F(DoorInteractionHandlerTest,
       HitTestingUsesNorthCurtainDoorEditorFootprint) {
  zelda3::Room::Door door;
  door.position = 0;
  door.type = zelda3::DoorType::CurtainDoor;
  door.direction = zelda3::DoorDirection::North;
  rooms_[0].AddDoor(door);

  const auto [door_x, door_y, door_w, door_h] =
      rooms_[0].GetDoors()[0].GetEditorBounds();
  ASSERT_EQ(door_w, 32);
  ASSERT_EQ(door_h, 32);

  const float scale = canvas_->global_scale();
  const int hit_x = static_cast<int>((door_x + (door_w / 2)) * scale);
  const int hit_y = static_cast<int>((door_y + door_h - 4) * scale);
  const auto hit = handler_.GetEntityAtPosition(hit_x, hit_y);
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, 0u);
}

}  // namespace
}  // namespace yaze::editor
