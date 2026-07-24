#include "app/editor/dungeon/dungeon_canvas_transform.h"
#include "app/editor/dungeon/interaction/door_interaction_handler.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "app/editor/dungeon/interaction/item_interaction_handler.h"
#include "app/editor/dungeon/interaction/sprite_interaction_handler.h"
#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"

#include <gtest/gtest.h>
#include <array>
#include <optional>

#include "app/editor/dungeon/dungeon_room_store.h"
namespace yaze::editor {
namespace {

// ============================================================================
// Sprite Interaction Handler Tests
// ============================================================================

class TestableSpriteInteractionHandler : public SpriteInteractionHandler {
 public:
  using BaseEntityHandler::GetCanvasTransform;
};

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
  TestableSpriteInteractionHandler handler_;
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

TEST_F(SpriteInteractionHandlerTest,
       PlacementOutsideRoomDoesNotMutateOrAppend) {
  handler_.SetSpriteId(0x42);
  handler_.BeginPlacement();

  EXPECT_TRUE(handler_.HandleClick(-1, 80));
  EXPECT_TRUE(handler_.HandleClick(dungeon_coords::kRoomPixelWidth, 80));

  EXPECT_TRUE(rooms_[0].GetSprites().empty());
  EXPECT_EQ(mutation_count_, 0);
  EXPECT_EQ(invalidate_count_, 0);
}

TEST_F(SpriteInteractionHandlerTest,
       PlacementAndHitTestingStayStableAcrossZoomAndPan) {
  constexpr std::array<float, 3> kScales = {0.5f, 1.0f, 2.0f};
  constexpr std::array<ImVec2, 2> kScrolling = {ImVec2(48.0f, 24.0f),
                                                ImVec2(-56.0f, -32.0f)};
  constexpr ImVec2 kPlacementRoomPixel(80.0f, 96.0f);
  constexpr ImVec2 kHitRoomPixel(88.0f, 104.0f);

  for (const ImVec2 scrolling : kScrolling) {
    for (const float scale : kScales) {
      rooms_[0].GetSprites().clear();
      canvas_->set_scrolling(scrolling);
      canvas_->set_global_scale(scale);
      const DungeonCanvasTransform transform = handler_.GetCanvasTransform();

      handler_.SetSpriteId(0x42);
      handler_.BeginPlacement();
      const auto [placement_x, placement_y] =
          transform.ScreenToRoomPixelCoordinates(
              transform.RoomPixelsToScreen(kPlacementRoomPixel));
      ASSERT_TRUE(handler_.HandleClick(placement_x, placement_y));
      handler_.CancelPlacement();

      ASSERT_EQ(rooms_[0].GetSprites().size(), 1u)
          << "scale=" << scale << ", scroll=" << scrolling.x << ","
          << scrolling.y;
      EXPECT_EQ(rooms_[0].GetSprites()[0].x(), 5);
      EXPECT_EQ(rooms_[0].GetSprites()[0].y(), 6);

      const auto [hit_x, hit_y] = transform.ScreenToRoomPixelCoordinates(
          transform.RoomPixelsToScreen(kHitRoomPixel));
      const auto hit = handler_.GetEntityAtPosition(hit_x, hit_y);
      ASSERT_TRUE(hit.has_value())
          << "scale=" << scale << ", scroll=" << scrolling.x << ","
          << scrolling.y;
      EXPECT_EQ(*hit, 0u);
    }
  }
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

TEST_F(SpriteInteractionHandlerTest, GhostCapacityStateIsNormalBelowLastSlot) {
  AddSprites(62);

  EXPECT_EQ(handler_.GetPlacementGhostCapacityState(),
            SpriteInteractionHandler::GhostCapacityState::kNormal);
}

TEST_F(SpriteInteractionHandlerTest,
       GhostCapacityStateWarnsOnLastAvailableSlot) {
  AddSprites(63);

  EXPECT_EQ(handler_.GetPlacementGhostCapacityState(),
            SpriteInteractionHandler::GhostCapacityState::kNearLimit);
}

TEST_F(SpriteInteractionHandlerTest, GhostCapacityStateBlocksWhenRoomIsFull) {
  AddSprites(64);

  EXPECT_EQ(handler_.GetPlacementGhostCapacityState(),
            SpriteInteractionHandler::GhostCapacityState::kAtLimit);
}

TEST_F(SpriteInteractionHandlerTest,
       DeleteAllClearsSpritesAndFiresMutationCallbacks) {
  AddSprites(3);
  handler_.SelectSprite(1);

  const int mutations_before = mutation_count_;
  const int invalidations_before = invalidate_count_;

  handler_.DeleteAll();

  EXPECT_TRUE(rooms_[0].GetSprites().empty());
  EXPECT_FALSE(handler_.HasSelection());
  EXPECT_EQ(mutation_count_, mutations_before + 1);
  EXPECT_EQ(invalidate_count_, invalidations_before + 1);
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

TEST_F(DoorInteractionHandlerTest, PlacementOutsideRoomDoesNotMutateOrAppend) {
  handler_.SetDoorType(zelda3::DoorType::NormalDoor);
  handler_.BeginPlacement();

  EXPECT_TRUE(handler_.HandleClick(-1, 16));
  EXPECT_TRUE(handler_.HandleClick(dungeon_coords::kRoomPixelWidth, 16));

  EXPECT_TRUE(rooms_[0].GetDoors().empty());
  EXPECT_EQ(mutation_count_, 0);
  EXPECT_EQ(invalidate_count_, 0);
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
  rooms_[0].ClearObjectStreamDirty();
  ASSERT_FALSE(rooms_[0].object_stream_dirty());

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
  EXPECT_TRUE(rooms_[0].object_stream_dirty());
}

TEST_F(DoorInteractionHandlerTest, MutateDoorTypeNoOpWhenTypeUnchanged) {
  zelda3::Room::Door door;
  door.position = 0x04;
  door.type = zelda3::DoorType::SmallKeyDoor;
  door.direction = zelda3::DoorDirection::East;
  rooms_[0].AddDoor(door);
  rooms_[0].ClearObjectStreamDirty();

  const int mutations_before = mutation_count_;
  const int invalidations_before = invalidate_count_;

  EXPECT_FALSE(handler_.MutateDoorType(0, zelda3::DoorType::SmallKeyDoor));
  EXPECT_EQ(mutation_count_, mutations_before);
  EXPECT_EQ(invalidate_count_, invalidations_before);
  EXPECT_FALSE(rooms_[0].object_stream_dirty());
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

  const int hit_x = door_x + (door_w / 2);
  const int hit_y = door_y + door_h - 4;
  const auto hit = handler_.GetEntityAtPosition(hit_x, hit_y);
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, 0u);
}

TEST_F(DoorInteractionHandlerTest,
       DragReleaseMovesDoorAndMarksObjectStreamDirty) {
  zelda3::Room::Door door;
  door.position = 0;
  door.type = zelda3::DoorType::NormalDoor;
  door.direction = zelda3::DoorDirection::North;
  auto [byte1, byte2] = door.EncodeBytes();
  door.byte1 = byte1;
  door.byte2 = byte2;
  rooms_[0].AddDoor(door);
  rooms_[0].ClearObjectStreamDirty();

  const auto [door_x, door_y, door_w, door_h] =
      rooms_[0].GetDoors()[0].GetEditorBounds();
  ASSERT_TRUE(handler_.HandleClick(door_x + door_w / 2, door_y + door_h / 2));

  handler_.HandleDrag(ImVec2(30.0f * 8.0f, static_cast<float>(door_y)),
                      ImVec2(0.0f, 0.0f));
  handler_.HandleRelease();

  const auto& moved = rooms_[0].GetDoors()[0];
  EXPECT_EQ(moved.position, 1);
  EXPECT_EQ(moved.direction, zelda3::DoorDirection::North);
  const auto [expected_b1, expected_b2] = moved.EncodeBytes();
  EXPECT_EQ(moved.byte1, expected_b1);
  EXPECT_EQ(moved.byte2, expected_b2);
  EXPECT_TRUE(rooms_[0].object_stream_dirty());
  EXPECT_EQ(mutation_count_, 1);
  EXPECT_EQ(invalidate_count_, 1);
}

TEST_F(DoorInteractionHandlerTest,
       ClickReleaseWithoutMovementDoesNotMutateOrMarkDirty) {
  zelda3::Room::Door door;
  // South positions 9-11 share moving-axis snap slots with 6-8. A plain click
  // must preserve the original ROM position instead of canonicalizing it.
  door.position = 9;
  door.type = zelda3::DoorType::NormalDoor;
  door.direction = zelda3::DoorDirection::South;
  auto [byte1, byte2] = door.EncodeBytes();
  door.byte1 = byte1;
  door.byte2 = byte2;
  rooms_[0].AddDoor(door);
  rooms_[0].ClearObjectStreamDirty();

  const auto [door_x, door_y, door_w, door_h] =
      rooms_[0].GetDoors()[0].GetEditorBounds();
  ASSERT_TRUE(handler_.HandleClick(door_x + door_w / 2, door_y + door_h / 2));

  handler_.HandleRelease();

  EXPECT_EQ(rooms_[0].GetDoors()[0].position, 9);
  EXPECT_EQ(rooms_[0].GetDoors()[0].direction, zelda3::DoorDirection::South);
  EXPECT_FALSE(rooms_[0].object_stream_dirty());
  EXPECT_EQ(mutation_count_, 0);
  EXPECT_EQ(invalidate_count_, 0);
}

TEST_F(DoorInteractionHandlerTest, NudgeMarksObjectStreamDirty) {
  zelda3::Room::Door door;
  door.position = 1;
  door.type = zelda3::DoorType::NormalDoor;
  door.direction = zelda3::DoorDirection::North;
  auto [byte1, byte2] = door.EncodeBytes();
  door.byte1 = byte1;
  door.byte2 = byte2;
  rooms_[0].AddDoor(door);
  rooms_[0].ClearObjectStreamDirty();
  handler_.SelectDoor(0);

  ASSERT_TRUE(handler_.NudgeSelected(1, 0));

  EXPECT_EQ(rooms_[0].GetDoors()[0].position, 2);
  EXPECT_TRUE(rooms_[0].object_stream_dirty());
}

TEST_F(DoorInteractionHandlerTest, PairBadgeClickNavigatesToNeighborDoor) {
  ctx_.current_room_id = 0;

  zelda3::Room::Door door;
  door.position = 0;
  door.type = zelda3::DoorType::NormalDoor;
  door.direction = zelda3::DoorDirection::East;
  rooms_[0].AddDoor(door);

  zelda3::Room::Door neighbor_door;
  neighbor_door.position = door.position;
  neighbor_door.type = zelda3::DoorType::NormalDoor;
  neighbor_door.direction = zelda3::DoorDirection::West;
  rooms_[1].AddDoor(neighbor_door);

  int target_room = -1;
  std::optional<size_t> target_door;
  int target_tile_x = -1;
  int target_tile_y = -1;
  ctx_.on_door_pair_navigation = [&](int room_id,
                                     std::optional<size_t> door_index,
                                     int tile_x, int tile_y) {
    target_room = room_id;
    target_door = door_index;
    target_tile_x = tile_x;
    target_tile_y = tile_y;
  };
  handler_.SetContext(&ctx_);
  handler_.SelectDoor(0);

  const auto [door_x, door_y, door_w, door_h] =
      rooms_[0].GetDoors()[0].GetEditorBounds();
  const int badge_x = door_x + door_w + 8;
  const int badge_y = door_y + door_h / 2;

  ASSERT_TRUE(handler_.HandleOverlayClick(badge_x, badge_y));
  EXPECT_EQ(target_room, 1);
  ASSERT_TRUE(target_door.has_value());
  EXPECT_EQ(*target_door, 0u);
  const auto [expected_tile_x, expected_tile_y] =
      rooms_[1].GetDoors()[0].GetTileCoords();
  EXPECT_EQ(target_tile_x, expected_tile_x);
  EXPECT_EQ(target_tile_y, expected_tile_y);
}

TEST_F(DoorInteractionHandlerTest, DeleteAllClearsDoorsAndFiresCallbacks) {
  AddDoors(3);
  handler_.SelectDoor(1);

  const int mutations_before = mutation_count_;
  const int invalidations_before = invalidate_count_;

  handler_.DeleteAll();

  EXPECT_TRUE(rooms_[0].GetDoors().empty());
  EXPECT_FALSE(handler_.HasSelection());
  EXPECT_EQ(mutation_count_, mutations_before + 1);
  EXPECT_EQ(invalidate_count_, invalidations_before + 1);
}

class ItemInteractionHandlerTest : public ::testing::Test {
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

  void AddItems(int count) {
    auto& items = rooms_[0].GetPotItems();
    for (int i = 0; i < count; ++i) {
      items.push_back(zelda3::PotItem{static_cast<uint16_t>(0x1000 + i),
                                      static_cast<uint8_t>(i)});
    }
  }

  std::unique_ptr<gui::Canvas> canvas_;
  DungeonRoomStore rooms_;
  InteractionContext ctx_;
  ItemInteractionHandler handler_;
  int mutation_count_ = 0;
  int invalidate_count_ = 0;
};

TEST_F(ItemInteractionHandlerTest, PlacementOutsideRoomDoesNotMutateOrAppend) {
  handler_.SetItemId(0x42);
  handler_.BeginPlacement();

  EXPECT_TRUE(handler_.HandleClick(80, -1));
  EXPECT_TRUE(handler_.HandleClick(80, dungeon_coords::kRoomPixelHeight));

  EXPECT_TRUE(rooms_[0].GetPotItems().empty());
  EXPECT_EQ(mutation_count_, 0);
  EXPECT_EQ(invalidate_count_, 0);
}

TEST_F(ItemInteractionHandlerTest, DragReleaseClampsToRoomAndMarksMutation) {
  rooms_[0].GetPotItems().push_back(zelda3::PotItem{0, 0x42});
  ASSERT_TRUE(handler_.HandleClick(0, 0));

  handler_.HandleDrag(ImVec2(5000.0f, 5000.0f), ImVec2(0, 0));
  handler_.HandleRelease();

  ASSERT_EQ(rooms_[0].GetPotItems().size(), 1u);
  EXPECT_EQ(rooms_[0].GetPotItems()[0].GetPixelX(), 508);
  EXPECT_EQ(rooms_[0].GetPotItems()[0].GetPixelY(), 496);
  EXPECT_EQ(mutation_count_, 1);
  EXPECT_EQ(invalidate_count_, 1);
}

TEST_F(ItemInteractionHandlerTest, ClickReleaseWithoutMovementDoesNotMutate) {
  rooms_[0].GetPotItems().push_back(zelda3::PotItem{0x0408, 0x42});
  ASSERT_TRUE(handler_.HandleClick(32, 64));

  handler_.HandleRelease();

  EXPECT_EQ(rooms_[0].GetPotItems()[0].position, 0x0408);
  EXPECT_EQ(mutation_count_, 0);
  EXPECT_EQ(invalidate_count_, 0);
}

TEST_F(ItemInteractionHandlerTest, DeleteAllClearsItemsAndFiresCallbacks) {
  AddItems(4);
  handler_.SelectItem(2);

  const int mutations_before = mutation_count_;
  const int invalidations_before = invalidate_count_;

  handler_.DeleteAll();

  EXPECT_TRUE(rooms_[0].GetPotItems().empty());
  EXPECT_FALSE(handler_.HasSelection());
  EXPECT_EQ(mutation_count_, mutations_before + 1);
  EXPECT_EQ(invalidate_count_, invalidations_before + 1);
}

}  // namespace
}  // namespace yaze::editor
