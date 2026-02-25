#include "app/editor/dungeon/interaction/sprite_interaction_handler.h"
#include "app/editor/dungeon/interaction/door_interaction_handler.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"

#include <gtest/gtest.h>
#include <array>

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
    ctx_.on_mutation = [this]() { mutation_count_++; };
    ctx_.on_invalidate_cache = [this]() { invalidate_count_++; };

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
  std::array<zelda3::Room, 296> rooms_;
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
    ctx_.on_mutation = [this]() { mutation_count_++; };
    ctx_.on_invalidate_cache = [this]() { invalidate_count_++; };

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
  std::array<zelda3::Room, 296> rooms_;
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

}  // namespace
}  // namespace yaze::editor
