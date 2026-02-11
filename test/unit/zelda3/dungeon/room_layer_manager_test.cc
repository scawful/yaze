#include "gtest/gtest.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {

class RoomLayerManagerTest : public ::testing::Test {
 protected:
  RoomLayerManager manager_;
};

// =============================================================================
// Layer Visibility Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, DefaultVisibilityAllLayersVisible) {
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG1_Layout));
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG1_Objects));
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG2_Layout));
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG2_Objects));
}

TEST_F(RoomLayerManagerTest, SetLayerVisibleWorks) {
  manager_.SetLayerVisible(LayerType::BG1_Objects, false);
  EXPECT_FALSE(manager_.IsLayerVisible(LayerType::BG1_Objects));
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG1_Layout));  // Others unchanged

  manager_.SetLayerVisible(LayerType::BG1_Objects, true);
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG1_Objects));
}

TEST_F(RoomLayerManagerTest, ResetRestoresDefaults) {
  manager_.SetLayerVisible(LayerType::BG1_Layout, false);
  manager_.SetLayerVisible(LayerType::BG2_Objects, false);
  manager_.SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Translucent);

  manager_.Reset();

  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG1_Layout));
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG2_Objects));
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout), LayerBlendMode::Normal);
}

// =============================================================================
// Blend Mode Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, DefaultBlendModeIsNormal) {
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG1_Layout), LayerBlendMode::Normal);
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout), LayerBlendMode::Normal);
}

TEST_F(RoomLayerManagerTest, SetBlendModeUpdatesAlpha) {
  manager_.SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Normal);
  EXPECT_EQ(manager_.GetLayerAlpha(LayerType::BG2_Layout), 255);

  manager_.SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Translucent);
  EXPECT_EQ(manager_.GetLayerAlpha(LayerType::BG2_Layout), 180);

  manager_.SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Off);
  EXPECT_EQ(manager_.GetLayerAlpha(LayerType::BG2_Layout), 0);
}

// =============================================================================
// Draw Order Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, DefaultDrawOrderBG2First) {
  manager_.SetBG2OnTop(false);
  auto order = manager_.GetDrawOrder();

  // BG2 should be drawn first (background)
  EXPECT_EQ(order[0], LayerType::BG2_Layout);
  EXPECT_EQ(order[1], LayerType::BG2_Objects);
  EXPECT_EQ(order[2], LayerType::BG1_Layout);
  EXPECT_EQ(order[3], LayerType::BG1_Objects);
}

TEST_F(RoomLayerManagerTest, BG2OnTopDrawOrderBG1First) {
  manager_.SetBG2OnTop(true);
  auto order = manager_.GetDrawOrder();

  // Draw order remains BG2 then BG1; "BG2 on top" only affects color math
  EXPECT_EQ(order[0], LayerType::BG2_Layout);
  EXPECT_EQ(order[1], LayerType::BG2_Objects);
  EXPECT_EQ(order[2], LayerType::BG1_Layout);
  EXPECT_EQ(order[3], LayerType::BG1_Objects);
}

// =============================================================================
// Per-Object Translucency Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, DefaultObjectsNotTranslucent) {
  EXPECT_FALSE(manager_.IsObjectTranslucent(0));
  EXPECT_FALSE(manager_.IsObjectTranslucent(10));
  EXPECT_EQ(manager_.GetObjectAlpha(0), 255);
}

TEST_F(RoomLayerManagerTest, SetObjectTranslucencyWorks) {
  manager_.SetObjectTranslucency(5, true, 128);
  EXPECT_TRUE(manager_.IsObjectTranslucent(5));
  EXPECT_EQ(manager_.GetObjectAlpha(5), 128);

  // Other objects unaffected
  EXPECT_FALSE(manager_.IsObjectTranslucent(4));
  EXPECT_EQ(manager_.GetObjectAlpha(4), 255);
}

TEST_F(RoomLayerManagerTest, ClearObjectTranslucencyWorks) {
  manager_.SetObjectTranslucency(5, true, 128);
  manager_.SetObjectTranslucency(10, true, 64);
  manager_.ClearObjectTranslucency();

  EXPECT_FALSE(manager_.IsObjectTranslucent(5));
  EXPECT_FALSE(manager_.IsObjectTranslucent(10));
}

// =============================================================================
// LayerMergeType Integration Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, ApplyLayerMergingNormal) {
  // LayerMergeType(id, name, see, top, trans)
  // "Normal" mode: visible=true, on_top=false, translucent=false
  LayerMergeType merge{0x06, "Normal", true, false, false};
  manager_.ApplyLayerMerging(merge);

  EXPECT_FALSE(manager_.IsBG2OnTop());  // Normal has Layer2OnTop=false
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout), LayerBlendMode::Normal);
}

TEST_F(RoomLayerManagerTest, ApplyLayerMergingTranslucent) {
  LayerMergeType merge{0x04, "Translucent", true, true, true};
  manager_.ApplyLayerMerging(merge);

  EXPECT_TRUE(manager_.IsBG2OnTop());
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout), LayerBlendMode::Translucent);
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Objects), LayerBlendMode::Translucent);
}

TEST_F(RoomLayerManagerTest, ApplyLayerMergingOff) {
  LayerMergeType merge{0x00, "Off", false, false, false};
  manager_.ApplyLayerMerging(merge);

  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout), LayerBlendMode::Normal);
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Objects), LayerBlendMode::Normal);
}

// =============================================================================
// Static Helper Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, GetLayerNameReturnsCorrectStrings) {
  EXPECT_STREQ(RoomLayerManager::GetLayerName(LayerType::BG1_Layout), "BG1 Layout");
  EXPECT_STREQ(RoomLayerManager::GetLayerName(LayerType::BG1_Objects), "BG1 Objects");
  EXPECT_STREQ(RoomLayerManager::GetLayerName(LayerType::BG2_Layout), "BG2 Layout");
  EXPECT_STREQ(RoomLayerManager::GetLayerName(LayerType::BG2_Objects), "BG2 Objects");
}

TEST_F(RoomLayerManagerTest, GetBlendModeNameReturnsCorrectStrings) {
  EXPECT_STREQ(RoomLayerManager::GetBlendModeName(LayerBlendMode::Normal), "Normal");
  EXPECT_STREQ(RoomLayerManager::GetBlendModeName(LayerBlendMode::Translucent), "Translucent");
  EXPECT_STREQ(RoomLayerManager::GetBlendModeName(LayerBlendMode::Off), "Off");
}

TEST_F(RoomLayerManagerTest, CompositeToOutputUsesBackdropWhenLayersAreEmpty) {
  Room room(/*room_id=*/0, /*rom=*/nullptr);

  // Ensure all layer buffers are initialized and empty (255 == transparent fill).
  room.bg1_buffer().EnsureBitmapInitialized();
  room.bg2_buffer().EnsureBitmapInitialized();
  room.object_bg1_buffer().EnsureBitmapInitialized();
  room.object_bg2_buffer().EnsureBitmapInitialized();
  room.bg1_buffer().bitmap().Fill(255);
  room.bg2_buffer().bitmap().Fill(255);
  room.object_bg1_buffer().bitmap().Fill(255);
  room.object_bg2_buffer().bitmap().Fill(255);

  gfx::Bitmap output;
  manager_.CompositeToOutput(room, output);

  ASSERT_TRUE(output.is_active());
  ASSERT_EQ(output.width(), 512);
  ASSERT_EQ(output.height(), 512);
  ASSERT_GT(output.size(), 0u);

  // With no visible pixels in any layer, the composite should remain at the
  // backdrop value (0), not the transparent fill (255).
  EXPECT_EQ(output.data()[0], 0);
}

TEST_F(RoomLayerManagerTest, PriorityCompositing_BG2Priority1OverBG1Priority0) {
  // This matches SNES Mode 1 behavior: BG2 tiles with priority=1 can appear
  // above BG1 tiles with priority=0.
  manager_.SetPriorityCompositing(true);

  Room room(/*room_id=*/0, /*rom=*/nullptr);
  room.bg1_buffer().EnsureBitmapInitialized();
  room.bg2_buffer().EnsureBitmapInitialized();
  room.object_bg1_buffer().EnsureBitmapInitialized();
  room.object_bg2_buffer().EnsureBitmapInitialized();
  room.bg1_buffer().bitmap().Fill(255);
  room.bg2_buffer().bitmap().Fill(255);
  room.object_bg1_buffer().bitmap().Fill(255);
  room.object_bg2_buffer().bitmap().Fill(255);
  room.bg1_buffer().ClearPriorityBuffer();
  room.bg2_buffer().ClearPriorityBuffer();
  room.object_bg1_buffer().ClearPriorityBuffer();
  room.object_bg2_buffer().ClearPriorityBuffer();

  // Put an opaque pixel in BG1 (priority 0) and a competing pixel in BG2
  // (priority 1) at the same location.
  room.bg1_buffer().bitmap().mutable_data()[0] = 10;
  room.bg1_buffer().mutable_priority_data()[0] = 0;
  room.bg2_buffer().bitmap().mutable_data()[0] = 20;
  room.bg2_buffer().mutable_priority_data()[0] = 1;

  gfx::Bitmap output;
  manager_.CompositeToOutput(room, output);
  ASSERT_TRUE(output.is_active());
  EXPECT_EQ(output.data()[0], 20);
}

TEST_F(RoomLayerManagerTest, PriorityCompositing_BG1Priority0OverBG2Priority0) {
  // BG1 with priority=0 should still be above BG2 with priority=0.
  manager_.SetPriorityCompositing(true);

  Room room(/*room_id=*/0, /*rom=*/nullptr);
  room.bg1_buffer().EnsureBitmapInitialized();
  room.bg2_buffer().EnsureBitmapInitialized();
  room.object_bg1_buffer().EnsureBitmapInitialized();
  room.object_bg2_buffer().EnsureBitmapInitialized();
  room.bg1_buffer().bitmap().Fill(255);
  room.bg2_buffer().bitmap().Fill(255);
  room.object_bg1_buffer().bitmap().Fill(255);
  room.object_bg2_buffer().bitmap().Fill(255);
  room.bg1_buffer().ClearPriorityBuffer();
  room.bg2_buffer().ClearPriorityBuffer();

  room.bg1_buffer().bitmap().mutable_data()[0] = 11;
  room.bg1_buffer().mutable_priority_data()[0] = 0;
  room.bg2_buffer().bitmap().mutable_data()[0] = 22;
  room.bg2_buffer().mutable_priority_data()[0] = 0;

  gfx::Bitmap output;
  manager_.CompositeToOutput(room, output);
  ASSERT_TRUE(output.is_active());
  EXPECT_EQ(output.data()[0], 11);
}

}  // namespace zelda3
}  // namespace yaze
