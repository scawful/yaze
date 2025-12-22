#include "gtest/gtest.h"
#include "zelda3/dungeon/room_layer_manager.h"

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

  // BG1 should be drawn first when BG2 is on top
  EXPECT_EQ(order[0], LayerType::BG1_Layout);
  EXPECT_EQ(order[1], LayerType::BG1_Objects);
  EXPECT_EQ(order[2], LayerType::BG2_Layout);
  EXPECT_EQ(order[3], LayerType::BG2_Objects);
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

  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout), LayerBlendMode::Off);
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Objects), LayerBlendMode::Off);
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

}  // namespace zelda3
}  // namespace yaze
